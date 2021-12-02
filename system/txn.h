#pragma once 

#include <atomic>

#include "global.h"
#include "helper.h"

class workload;
class thread_t;
class row_t;
class table_t;
class base_query;
class INDEX;

// each thread has a txn_man. 
// a txn_man corresponds to a single transaction.

//For VLL
enum TxnType {VLL_Blocked, VLL_Free};

class Access {
public:
	access_t 	type;
	row_t * 	orig_row;
	row_t * 	data;
	row_t * 	orig_data;

	void cleanup();
#if CC_ALG == TICTOC
	ts_t 		wts;
	ts_t 		rts;
#elif CC_ALG == SILO
	ts_t 		tid;
	ts_t 		epoch;
#elif CC_ALG == HEKATON
	void * 		history_entry;	
#endif

};

class txn_man
{
public:
	virtual void init(thread_t * h_thd, workload * h_wl, uint64_t part_id);
	void release();
	thread_t * h_thd;
	workload * h_wl;
	myrand * mrand;
	uint64_t abort_cnt;

	virtual RC 		run_txn(base_query * m_query) = 0;
	uint64_t 		get_thd_id();
	workload * 		get_wl();

	void 			set_txn_id(txnid_t txn_id);
	txnid_t 		get_txn_id();

	void 			set_ts(ts_t timestamp);
	ts_t 			get_ts();

	pthread_mutex_t txn_lock;
	std::atomic<row_t *> cur_row;
#if CC_ALG == HEKATON
	std::atomic<void *> history_entry;
#endif
	// [DL_DETECT, NO_WAIT, WAIT_DIE]
	std::atomic_bool 	lock_ready;
	std::atomic_bool 	lock_abort; // forces another waiting txn to abort.
	// [TIMESTAMP, MVCC]
	std::atomic_bool	ts_ready;

	// [HSTORE]
#define USE_ATOMIC true
#if USE_ATOMIC
	std::atomic_int 	ready_part;
#else
	int		ready_part;
#endif  // USE_ATOMIC
	RC 				finish(RC rc);
	void 			cleanup(RC rc);
#if CC_ALG == TICTOC
	ts_t 			get_max_wts() 	{ return _max_wts; }
	void 			update_max_wts(ts_t max_wts);
	ts_t 			last_wts;
	ts_t 			last_rts;
#elif CC_ALG == SILO
	ts_t 			last_tid;
#endif

	// For OCC
	uint64_t 		start_ts;
	uint64_t 		end_ts;
	// following are public for OCC
	int 			row_cnt;
	int	 			wr_cnt;

	Access **		accesses;
	int 			num_accesses_alloc;

	// For VLL
	TxnType 		vll_txn_type;

	template<typename Index_t>
	itemid_t* index_read(Index_t* index, idx_key_t key, int part_id) {
		uint64_t starttime = get_sys_clock();
		itemid_t* item;
		index->index_read(key, item, part_id, get_thd_id());
		INC_TMP_STATS(get_thd_id(), time_index, get_sys_clock() - starttime);
		return item;
	}

	template<typename Index_t>
	void index_read(Index_t* index, idx_key_t key, int part_id, itemid_t*& item) {
		uint64_t starttime = get_sys_clock();
		index->index_read(key, item, part_id, get_thd_id());
		INC_TMP_STATS(get_thd_id(), time_index, get_sys_clock() - starttime);
	}

#if TPCC_ALL_TXN
	template<typename Index_t>
	RC index_read_safe(Index_t* index, idx_key_t key, int part_id, itemid_t &item, bool back = false) {
		uint64_t starttime = get_sys_clock();
		RC rc = index->index_read_safe(key, item, part_id, get_thd_id(), back);
		INC_TMP_STATS(get_thd_id(), time_index, get_sys_clock() - starttime);
		
		return rc;
	}
#endif  // TPCC_ALL_TXN

	row_t* get_row(row_t* row, access_t type, uint64_t tuple_size);
protected:
	void 			insert_row(row_t* row, table_t* table);
#if TPCC_ALL_TXN
	void insert_index(INDEX* index, idx_key_t key, row_t *row, int pid, bool no_collision = true);
	void delete_index(INDEX* index, idx_key_t key, itemid_t *item, itemid_t *hint, int pid);
#else
	void insert_index(INDEX* index, idx_key_t key, row_t *row, int pid){
		// NOOP
	}
	
	void delete_index(INDEX* index, idx_key_t key, itemid_t *item, itemid_t *hint, int pid) {
		// NOOP
	}
#endif  // TPCC_ALL_TXN
private:
	// insert rows
	uint64_t 		insert_cnt;
	row_t* insert_rows[MAX_WRITE_SET];
#if TPCC_ALL_TXN
	struct IndexInsert {
		INDEX* index;
		idx_key_t key;
		row_t* row;
		int pid;
		bool no_collision;
	};

	uint64_t insert_index_cnt;
	IndexInsert insert_index_ops[MAX_WRITE_SET];

	struct IndexDelete {
		INDEX* index;
		idx_key_t key;
		itemid_t* item;
		itemid_t* hint;
		int pid;
	};

	uint64_t delete_index_cnt;
	IndexDelete delete_index_ops[MAX_WRITE_SET];
#endif  // TPCC_ALL_TXN
	txnid_t 		txn_id;
	ts_t 			timestamp;

	bool _write_copy_ptr;
#if CC_ALG == TICTOC || CC_ALG == SILO
	bool 			_pre_abort;
	bool 			_validation_no_wait;
#endif
#if CC_ALG == TICTOC
	bool			_atomic_timestamp;
	ts_t 			_max_wts;
	// the following methods are defined in concurrency_control/tictoc.cpp
	RC				validate_tictoc();
#elif CC_ALG == SILO
	ts_t 			_cur_tid;
	RC				validate_silo();
#elif CC_ALG == HEKATON
	RC 				validate_hekaton(RC rc);
#endif
#if TPCC_ALL_TXN
	void maintain_indexes(RC rc);
#endif  // TPCC_ALL_TXN
};
