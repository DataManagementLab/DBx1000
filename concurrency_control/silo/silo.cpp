#include "txn.h"
#include "row.h"
#include "row_silo.h"

#if CC_ALG == SILO

RC
txn_man::validate_silo()
{
	RC rc = RCOK;
	// lock write tuples in the primary key order.
	int write_set[wr_cnt];
#if !ATOMIC_WORD && SCOPED_LATCH
	SCOPED_LATCH_T::scoped_lock scoped_lock_instances[wr_cnt];
#define lock_instance(i) scoped_lock_instances[i]
#else
#define lock_instance(i)
#endif  // SCOPED_LATCH
	int cur_wr_idx = 0;
	int read_set[row_cnt - wr_cnt];
	int cur_rd_idx = 0;
#if BOUNDED_SPINNING
	bounded_backoff_function bf;
#endif  // BOUNDED_SPINNING

	for (int rid = 0; rid < row_cnt; rid++) {
		if (accesses[rid]->type == WR)
			write_set[cur_wr_idx++] = rid;
		else
			read_set[cur_rd_idx++] = rid;
	}

#if !BOUNDED_SPINNING
	//! XXX: This is claimed to be very expensive
	// bubble sort the write_set, in primary key order 
	for (int i = wr_cnt - 1; i >= 1; i--) {
		for (int j = 0; j < i; j++) {
			if (accesses[write_set[j]]->orig_row->get_primary_key() >
				accesses[write_set[j + 1]]->orig_row->get_primary_key())
			{
				int tmp = write_set[j];
				write_set[j] = write_set[j + 1];
				write_set[j + 1] = tmp;
			}
		}
	}
#endif  // BOUNDED_SPINNING

	int num_locks = 0;
	ts_t max_tid = 0;
	bool done = false;
	if (_pre_abort) {
		for (int i = 0; i < wr_cnt; i++) {
			row_t* row = accesses[write_set[i]]->orig_row;
			if (row->getManager()->get_tid() != accesses[write_set[i]]->tid) {
				rc = Abort;
				goto final;
			}
		}
		for (int i = 0; i < row_cnt - wr_cnt; i++) {
			Access* access = accesses[read_set[i]];
			if (access->orig_row->getManager()->get_tid() != accesses[read_set[i]]->tid) {
				rc = Abort;
				goto final;
			}
		}
	}

	// lock all rows in the write set.
	if (_validation_no_wait) {
		while (!done) {
			num_locks = 0;
			for (int i = 0; i < wr_cnt; i++) {
				row_t* row = accesses[write_set[i]]->orig_row;
				if (!row->getManager()->try_lock(lock_instance(i)))
					break;
				row->getManager()->assert_lock();
				num_locks++;
				if (row->getManager()->get_tid() != accesses[write_set[i]]->tid)
				{
					rc = Abort;
					goto final;
				}
			}
			if (num_locks == wr_cnt)
				done = true;
			else {
				for (int i = 0; i < num_locks; i++)
					accesses[write_set[i]]->orig_row->getManager()->release(lock_instance(i));
				if (_pre_abort) {
					num_locks = 0;
					for (int i = 0; i < wr_cnt; i++) {
						row_t* row = accesses[write_set[i]]->orig_row;
						if (row->getManager()->get_tid() != accesses[write_set[i]]->tid) {
							rc = Abort;
							goto final;
						}
					}
					for (int i = 0; i < row_cnt - wr_cnt; i++) {
						Access* access = accesses[read_set[i]];
						if (access->orig_row->getManager()->get_tid() != accesses[read_set[i]]->tid) {
							rc = Abort;
							goto final;
						}
					}
				}
#if BOUNDED_SPINNING
				// Bounded spinning, abort if back-off reaches limit
				if (__builtin_expect(!bf(), 0))
				{
					rc = Abort;
					goto final;
				}
#else
				PAUSE
#endif  // BOUNDED_SPINNING
			}
		}
	}
	else {
		for (int i = 0; i < wr_cnt; i++) {
			row_t* row = accesses[write_set[i]]->orig_row;
			row->getManager()->lock(lock_instance(i));
			num_locks++;
			if (row->getManager()->get_tid() != accesses[write_set[i]]->tid) {
				rc = Abort;
				goto final;
			}
		}
	}

	// validate rows in the read set
	// for repeatable_read, no need to validate the read set.
	for (int i = 0; i < row_cnt - wr_cnt; i++) {
		Access* access = accesses[read_set[i]];
		bool success = access->orig_row->getManager()->validate(access->tid, false);
		if (!success) {
			rc = Abort;
			goto final;
		}
		if (access->tid > max_tid)
			max_tid = access->tid;
	}
	// validate rows in the write set
	for (int i = 0; i < wr_cnt; i++) {
		Access* access = accesses[write_set[i]];
		bool success = access->orig_row->getManager()->validate(access->tid, true);
		if (!success) {
			rc = Abort;
			goto final;
		}
		if (access->tid > max_tid)
			max_tid = access->tid;
	}
	if (max_tid > _cur_tid)
		_cur_tid = max_tid + 1;
	else
		_cur_tid++;
	final:
#if TPCC_ALL_TXN
	maintain_indexes(rc);
#endif  // TPCC_ALL_TXN
	if (rc == Abort) {
		for (int i = 0; i < num_locks; i++)
			accesses[write_set[i]]->orig_row->getManager()->release(lock_instance(i));
		cleanup(rc);
	}
	else {
		for (int i = 0; i < wr_cnt; i++) {
			Access* access = accesses[write_set[i]];
			access->orig_row->getManager()->write(
				access->data, _cur_tid);
			accesses[write_set[i]]->orig_row->getManager()->release(lock_instance(i));
		}
		cleanup(rc);
	}
	return rc;
}
#endif
