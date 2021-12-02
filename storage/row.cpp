#include <mm_malloc.h>
#include "global.h"
#include "table.h"
#include "catalog.h"
#include "row.h"
#include "txn.h"
#include "manager.h"

RC row_t::get_row(access_t type, txn_man * txn, row_t *& row, uint64_t tuple_size) {
	RC rc = RCOK;
#if CC_ALG == WAIT_DIE || CC_ALG == NO_WAIT || CC_ALG == DL_DETECT
	uint64_t thd_id = txn->get_thd_id();
	lock_t lt = (type == RD || type == SCAN)? LOCK_SH : LOCK_EX;
#if CC_ALG == DL_DETECT
	uint64_t * txnids;
	int txncnt; 
	rc = this->getManager()->lock_get(lt, txn, txnids, txncnt);	
#else
	rc = this->getManager()->lock_get(lt, txn);
#endif

	if (rc == RCOK) {
		row = this;
	} else if (rc == Abort) {} 
	else if (rc == WAIT) {
		assert(CC_ALG == WAIT_DIE || CC_ALG == DL_DETECT);
		uint64_t starttime = get_sys_clock();
#if CC_ALG == DL_DETECT	
		bool dep_added = false;
#endif
		uint64_t endtime;
		txn->lock_abort.store(false, std::memory_order_relaxed);
		INC_STATS(txn->get_thd_id(), wait_cnt, 1);
		while (!txn->lock_ready.load(std::memory_order_relaxed) && !txn->lock_abort.load(std::memory_order_relaxed)) 
		{
#if CC_ALG == WAIT_DIE 
			continue;
#elif CC_ALG == DL_DETECT
			backoff_function bf;
			uint64_t last_detect = starttime;
			uint64_t last_try = starttime;

			uint64_t now = get_sys_clock();
			if (now - starttime > g_timeout ) {
				txn->lock_abort.store(true, std::memory_order_relaxed);
				break;
			}
			if (g_no_dl) {
				bf();
				continue;
			}
			int ok = 0;
			if ((now - last_detect > g_dl_loop_detect) && (now - last_try > DL_LOOP_TRIAL)) {
				if (!dep_added) {
					ok = dl_detector.add_dep(txn->get_txn_id(), txnids, txncnt, txn->row_cnt);
					if (ok == 0)
						dep_added = true;
					else if (ok == 16)
						last_try = now;
				}
				if (dep_added) {
					ok = dl_detector.detect_cycle(txn->get_txn_id());
					if (ok == 16)  // failed to lock the deadlock detector
						last_try = now;
					else if (ok == 0) 
						last_detect = now;
					else if (ok == 1) {
						last_detect = now;
					}
				}
			} else 
				bf();
#endif
		}
		std::atomic_thread_fence(std::memory_order_acquire);
		if (txn->lock_ready.load(std::memory_order_relaxed)) 
			rc = RCOK;
		else if (txn->lock_abort.load(std::memory_order_relaxed)) { 
			rc = Abort;
			return_row(type, txn, NULL, tuple_size);
		}
		endtime = get_sys_clock();
		INC_TMP_STATS(thd_id, time_wait, endtime - starttime);
		row = this;
	}
	return rc;
#elif CC_ALG == TIMESTAMP || CC_ALG == MVCC || CC_ALG == HEKATON 
	uint64_t thd_id = txn->get_thd_id();
	// For TIMESTAMP RD, a new copy of the row will be returned.
	// for MVCC RD, the version will be returned instead of a copy
	// So for MVCC RD-WR, the version should be explicitly copied.
	//row_t * newr = NULL;
  #if CC_ALG == TIMESTAMP
	// TODO. should not call malloc for each row read. Only need to call malloc once 
	// before simulation starts, like TicToc and Silo.
	txn->cur_row = (row_t *) mem_allocator.alloc(sizeof(row_t), this->get_part_id());
	txn->cur_row.load(std::memory_order_relaxed)->init(get_table(), this->get_part_id());
  #endif

	// TODO need to initialize the table/catalog information.
	TsType ts_type = (type == RD)? R_REQ : P_REQ; 
	rc = this->getManager()->access(txn, ts_type, row, tuple_size);
	if (rc == RCOK ) {
		row = txn->cur_row.load(std::memory_order_relaxed);
	} else if (rc == WAIT) {
		backoff_function bf;
		uint64_t t1 = get_sys_clock();
		while (!txn->ts_ready.load(memory_order_relaxed))
			bf();

		std::atomic_thread_fence(std::memory_order_acquire);
		uint64_t t2 = get_sys_clock();
		INC_TMP_STATS(thd_id, time_wait, t2 - t1);
		row = txn->cur_row.load(std::memory_order_relaxed);
	}
	if (rc != Abort) {
		row->table = get_table();
		assert(row->get_schema() == this->get_schema());
	}
	return rc;
#elif CC_ALG == OCC
	// OCC always make a local copy regardless of read or write
	// Let thread local allocator handle this even with THREAD_ALLOC_HAWK
	txn->cur_row = (row_t *) mem_allocator.alloc(sizeof(row_t), get_part_id());
	char* data = (char *) mem_allocator.alloc(tuple_size, get_part_id());
	txn->cur_row.load(std::memory_order_relaxed)->init(get_table(), get_part_id(), 0, data);
	rc = this->getManager()->access(txn, R_REQ, tuple_size);
	row = txn->cur_row.load(std::memory_order_relaxed);
	return rc;
#elif CC_ALG == TICTOC || CC_ALG == SILO
	// like OCC, tictoc also makes a local copy for each read/write
	row->table = get_table();
	TsType ts_type = (type == RD)? R_REQ : P_REQ; 
	rc = this->getManager()->access(txn, ts_type, row, tuple_size);
	return rc;
#elif CC_ALG == HSTORE || CC_ALG == VLL
	row = this;
	return rc;
#else
	assert(false);
#endif
}

// the "row" is the row read out in get_row(). 
// For locking based CC_ALG, the "row" is the same as "this". 
// For timestamp based CC_ALG, the "row" != "this", and the "row" must be freed.
// For MVCC, the row will simply serve as a version. The version will be 
// delete during history cleanup.
// For TIMESTAMP, the row will be explicity deleted at the end of access().
// (cf. row_ts.cpp)
void row_t::return_row(access_t type, txn_man * txn, row_t * row, uint64_t tuple_size) {	
#if CC_ALG == WAIT_DIE || CC_ALG == NO_WAIT || CC_ALG == DL_DETECT
	assert (row == NULL || row == this || type == XP);
	if (ROLL_BACK && type == XP) {// recover from previous writes.
		this->copy(row, tuple_size);
	}
	this->getManager()->lock_release(txn);
#elif CC_ALG == TIMESTAMP || CC_ALG == MVCC 
	// for RD or SCAN or XP, the row should be deleted.
	// because all WR should be companied by a RD
	// for MVCC RD, the row is not copied, so no need to free. 
  #if CC_ALG == TIMESTAMP
	if (type == RD || type == SCAN) {
		row->free_row();
		mem_allocator.free(row, sizeof(row_t));
	}
  #endif
	if (type == XP) {
		this->getManager()->access(txn, XP_REQ, row, tuple_size);
	} else if (type == WR) {
		assert (type == WR && row != NULL);
		assert (row->get_schema() == this->get_schema());
		RC rc = this->getManager()->access(txn, W_REQ, row, tuple_size);
		assert(rc == RCOK);
	}
#elif CC_ALG == OCC
	assert (row != NULL);
	if (type == WR)
		getManager()->write( row, txn->end_ts, tuple_size);
	row->free_row();
	mem_allocator.free(row, sizeof(row_t));
	return;
#elif CC_ALG == TICTOC || CC_ALG == SILO
	assert (row != NULL);
	return;
#elif CC_ALG == HSTORE || CC_ALG == VLL
	return;
#else 
	assert(false);
#endif
}

