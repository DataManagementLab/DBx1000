#include "txn.h"
#include "row.h"
#include "wl.h"
#include "ycsb.h"
#include "thread.h"
#include "utils/mem_alloc.h"
#include "occ.h"
#include "table.h"
#include "catalog.h"
#include "index_btree.h"
#include "index_hash.h"

void txn_man::init(thread_t* h_thd, workload* h_wl, uint64_t thd_id) {
	this->h_thd = h_thd;
	this->h_wl = h_wl;
	pthread_mutex_init(&txn_lock, NULL);
	lock_ready = false;
	ready_part = 0;
	row_cnt = 0;
	wr_cnt = 0;
	insert_cnt = 0;
	accesses = (Access**)_mm_malloc(sizeof(Access*) * MAX_ROW_PER_TXN, 64);
	for (int i = 0; i < MAX_ROW_PER_TXN; i++)
		accesses[i] = NULL;
	num_accesses_alloc = 0;
#if CC_ALG == TICTOC || CC_ALG == SILO
	_pre_abort = (g_params["pre_abort"] == "true");
	if (g_params["validation_lock"] == "no-wait")
		_validation_no_wait = true;
	else if (g_params["validation_lock"] == "waiting")
		_validation_no_wait = false;
	else
		assert(false);
#endif
#if CC_ALG == TICTOC
	_max_wts = 0;
	_write_copy_ptr = (g_params["write_copy_form"] == "ptr");
	_atomic_timestamp = (g_params["atomic_timestamp"] == "true");
#elif CC_ALG == SILO
	_cur_tid = 0;
#endif

}

void txn_man::set_txn_id(txnid_t txn_id) {
	this->txn_id = txn_id;
}

txnid_t txn_man::get_txn_id() {
	return this->txn_id;
}

workload* txn_man::get_wl() {
	return h_wl;
}

uint64_t txn_man::get_thd_id() {
	return h_thd->get_thd_id();
}

void txn_man::set_ts(ts_t timestamp) {
	this->timestamp = timestamp;
}

ts_t txn_man::get_ts() {
	return this->timestamp;
}

void txn_man::cleanup(RC rc) {
/* #if CC_ALG == HEKATON
	row_cnt = 0;
	wr_cnt = 0;
	insert_cnt = 0;
	return;
#endif */
	
#if TPCC_ALL_TXN && CC_ALG != SILO && CC_ALG != TICTOC
	maintain_indexes(rc);
#endif  // TPCC_ALL_TXN && CC_ALG

#if CC_ALG != HEKATON && CC_ALG != HSTORE
	for (int rid = row_cnt - 1; rid >= 0; rid--) {
		row_t* orig_r = accesses[rid]->orig_row;
		access_t type = accesses[rid]->type;
		if (type == WR && rc == Abort)
			type = XP;

#if (CC_ALG == NO_WAIT || CC_ALG == DL_DETECT) && ISOLATION_LEVEL == REPEATABLE_READ
		if (type == RD) {
			accesses[rid]->data = NULL;
			continue;
		}
#endif

		if (ROLL_BACK && type == XP &&
			(CC_ALG == DL_DETECT ||
				CC_ALG == NO_WAIT ||
				CC_ALG == WAIT_DIE))
		{
			orig_r->return_row(type, this, accesses[rid]->orig_data, orig_r->get_tuple_size());
		}
		else {
			orig_r->return_row(type, this, accesses[rid]->data, orig_r->get_tuple_size());
		}
#if CC_ALG != TICTOC && CC_ALG != SILO
		accesses[rid]->data = NULL;
#endif
	}

	if (rc == Abort) {
		for (UInt32 i = 0; i < insert_cnt; i++) {
			row_t* row = insert_rows[i];
			assert(g_part_alloc == false);
#if MALLOC_TYPE != HAWK_ALLOC
#if CC_ALG != HSTORE && CC_ALG != OCC
			mem_allocator.free(row->getManager(), 0);
#endif
#if !THREAD_ALLOC_HAWK_INSERT
			row->free_row();
			mem_allocator.free(row, sizeof(row));
#endif  // THREAD_ALLOC_HAWK_INSERT
#endif  // !THREAD_ALLOC_HAWK
		}
	}
#endif  // CC_ALG != HEKATON && CC_ALG != HSTORE
	row_cnt = 0;
	wr_cnt = 0;
	insert_cnt = 0;
#if CC_ALG == DL_DETECT
	dl_detector.clear_dep(get_txn_id());
#endif
}

row_t* txn_man::get_row(row_t* row, access_t type, uint64_t tuple_size) {
#if ALLOW_READ_ONLY
	if (type == RO)
	{
		return row;
	}
#endif  // ALLOW_READ_ONLY

	if (CC_ALG == HSTORE)
		return row;

	//assert(row_cnt < MAX_ROW_PER_TXN);

	// Abort due to row buffer overflow!
	if(row_cnt >= MAX_ROW_PER_TXN) {
		std::cout << "Max row count reached, aborting...!\n";
		return NULL;
	}

	uint64_t starttime = get_sys_clock();
	RC rc = RCOK;
	if (accesses[row_cnt] == NULL) {
		Access* access = (Access*)_mm_malloc(sizeof(Access), 64);
		accesses[row_cnt] = access;
#if (CC_ALG == SILO || CC_ALG == TICTOC)
		access->data = (row_t*)_mm_malloc(sizeof(row_t), 64);
		access->data->init(MAX_TUPLE_SIZE);
		access->orig_data = (row_t*)_mm_malloc(sizeof(row_t), 64);
		access->orig_data->init(MAX_TUPLE_SIZE);
#elif (CC_ALG == DL_DETECT || CC_ALG == NO_WAIT || CC_ALG == WAIT_DIE)
		access->orig_data = (row_t*)_mm_malloc(sizeof(row_t), 64);
		access->orig_data->init(MAX_TUPLE_SIZE);
#endif
		num_accesses_alloc++;
	}

	rc = row->get_row(type, this, accesses[row_cnt]->data, tuple_size);


	if (rc == Abort) {
		return NULL;
	}
	accesses[row_cnt]->type = type;
	accesses[row_cnt]->orig_row = row;
#if CC_ALG == TICTOC
	accesses[row_cnt]->wts = last_wts;
	accesses[row_cnt]->rts = last_rts;
#elif CC_ALG == SILO
	accesses[row_cnt]->tid = last_tid;
#elif CC_ALG == HEKATON
	accesses[row_cnt]->history_entry = history_entry;
#endif

#if ROLL_BACK && (CC_ALG == DL_DETECT || CC_ALG == NO_WAIT || CC_ALG == WAIT_DIE)
	if (type == WR) {
		accesses[row_cnt]->orig_data->table = row->get_table();
		accesses[row_cnt]->orig_data->copy(row, tuple_size);
	}
#endif

#if (CC_ALG == NO_WAIT || CC_ALG == DL_DETECT) && ISOLATION_LEVEL == REPEATABLE_READ
	if (type == RD)
		row->return_row(type, this, accesses[row_cnt]->data);
#endif

	row_cnt++;
	if (type == WR)
		wr_cnt++;

	uint64_t timespan = get_sys_clock() - starttime;
	INC_TMP_STATS(get_thd_id(), time_man, timespan);
	return accesses[row_cnt - 1]->data;
}

void txn_man::insert_row(row_t* row, table_t* table) {
	if (CC_ALG == HSTORE)
		return;
	if(insert_cnt >= MAX_WRITE_SET) {
		throw std::runtime_error("Insert Row buffer full!");
	}
	insert_rows[insert_cnt++] = row;
}

#if TPCC_ALL_TXN
void txn_man::insert_index(INDEX* index, idx_key_t key, row_t *row, int pid, bool no_collision) {
	if(insert_index_cnt >= MAX_WRITE_SET) {
		throw std::runtime_error("Insert Index Buffer full!");
	}
	insert_index_ops[insert_index_cnt++] = {index, key, row, pid, no_collision};
}

void txn_man::delete_index(INDEX* index, idx_key_t key, itemid_t *item, itemid_t *hint, int pid) {
	if (delete_index_cnt >= MAX_WRITE_SET) {
		throw std::runtime_error("Delete Index Buffer full!");
	}
	delete_index_ops[delete_index_cnt++] = {index, key, item, hint, pid};
}
#endif  // TPCC_ALL_TXN

RC txn_man::finish(RC rc) {
/* #if CC_ALG == HSTORE
	return RCOK;
#endif */
	uint64_t starttime = get_sys_clock();
#if CC_ALG == OCC
	if (rc == RCOK)
		rc = occ_man.validate(this);
	else 
		cleanup(rc);
#elif CC_ALG == TICTOC
	if (rc == RCOK)
		rc = validate_tictoc();
	else 
		cleanup(rc);
#elif CC_ALG == SILO
	if (rc == RCOK)
		rc = validate_silo();
	else 
		cleanup(rc);
#elif CC_ALG == HEKATON
	rc = validate_hekaton(rc);
	cleanup(rc);
#else 
	cleanup(rc);
#endif
	uint64_t timespan = get_sys_clock() - starttime;
	INC_TMP_STATS(get_thd_id(), time_man,  timespan);
	INC_STATS(get_thd_id(), time_cleanup,  timespan);
	return rc;
}

void
txn_man::release() {
	for (int i = 0; i < num_accesses_alloc; i++)
		mem_allocator.free(accesses[i], 0);
	mem_allocator.free(accesses, 0);
}

#if TPCC_ALL_TXN
void txn_man::maintain_indexes(RC rc) {
	if (rc == RCOK) {
		// Maintain indexes
		
		// Delete entries
		for (UInt32 i = 0; i < delete_index_cnt; ++i) {
			auto&& entry = delete_index_ops[i];

			// TODO free row and related resources
			//row_t* row = (row_t*)entry.item->location;
			// mem_allocator.free(item->location, );

			entry.index->index_delete_safe(entry.key, entry.item, entry.hint, entry.pid);
		}

		// Insert entries
		for (UInt32 i = 0; i < insert_index_cnt; ++i) {
			auto&& entry = insert_index_ops[i];

			itemid_t* m_item;
#if INDEX_STRUCT == IDX_HASH_PERFECT
			itemid_t item;
			m_item = &item;
#else
			m_item =
				(itemid_t*)mem_allocator.alloc(sizeof(itemid_t), entry.pid);
#endif  // INDEX_STRUCT
			m_item->init();
			m_item->type = DT_row;
			m_item->location = entry.row;
			m_item->valid = true;

			entry.index->index_insert_safe(entry.key, m_item, entry.pid, entry.no_collision);
		}
	}
	insert_index_cnt = 0;
	delete_index_cnt = 0;
}
#endif  // TPCC_ALL_TXN
