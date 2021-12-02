#include "txn.h"
#include "row.h"
#include "row_occ.h"
#include "utils/mem_alloc.h"

void
Row_occ::init(row_t* row) {
	_row = row;
	int part_id = row->get_part_id();
#if defined(SCOPED_LATCH_T) && SCOPED_LATCH
	/* _latch = (SCOPED_LATCH_T*)
		mem_allocator.alloc(sizeof(SCOPED_LATCH_T), part_id); */
#endif
	new(&_latch) SCOPED_LATCH_T;
	wts = 0;
	//blatch = false;
}

RC
Row_occ::access(txn_man* txn, TsType type, uint64_t tuple_size) {
	RC rc = RCOK;
#if SCOPED_LATCH
	SCOPED_LATCH_T::scoped_lock lock(_latch);
#else
	_latch.lock();
#endif  // SCOPED_LATCH
	if (type == R_REQ) {
		if (txn->start_ts < wts)
			rc = Abort;
		else {
			txn->cur_row.load(std::memory_order_relaxed)->copy(_row, tuple_size);
			rc = RCOK;
		}
	}
	else
		assert(false);
#if !SCOPED_LATCH
	_latch.unlock();
#endif  // SCOPED_LATCH
	return rc;
}


bool
Row_occ::validate(uint64_t ts) {
	if (ts < wts) return false;
	else return true;
}

void
Row_occ::write(row_t* data, uint64_t ts, uint64_t tuple_size) {
	_row->copy(data, tuple_size);
	if (PER_ROW_VALID) {
		assert(ts > wts);
		wts = ts;
	}
}

#if SCOPED_LATCH
void
Row_occ::latch(SCOPED_LATCH_T::scoped_lock& lock) {
	lock.acquire(_latch);
}

bool
Row_occ::try_latch(SCOPED_LATCH_T::scoped_lock& lock) {
	bounded_backoff_function bf;

	while (!lock.try_acquire(_latch))
	{
		if (__builtin_expect(!bf(), 0))
			return false;
	}

	// Successfully acquired lock (after spinning)
	return true;
}


void
Row_occ::release() {
}

#else

void
Row_occ::latch() {
	_latch.lock();
}

bool
Row_occ::try_latch() {
	bounded_backoff_function bf;

	while (!_latch.try_lock())
	{
		if (__builtin_expect(!bf(), 0))
			return false;
	}

	// Successfully acquired lock (after spinning)
	return true;
}

void
Row_occ::release() {
	_latch.unlock();
}

#endif  // SCOPED_LATCH
