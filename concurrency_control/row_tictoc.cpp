#include "row_tictoc.h"
#include "row.h"
#include "txn.h"
#include "utils/mem_alloc.h"
#include <mm_malloc.h>

#if CC_ALG==TICTOC

void
Row_tictoc::init(row_t* row)
{
	_row = row;
#if ATOMIC_WORD
	_ts_word = 0;
#else
	_latch = (LATCH_T*)_mm_malloc(sizeof(LATCH_T), 64);
	new(_latch) LATCH_T;
	_wts = 0;
	_rts = 0;
#endif
#if TICTOC_MV
	_hist_wts = 0;
#endif
}

RC
Row_tictoc::access(txn_man* txn, TsType type, row_t* local_row, uint64_t tuple_size)
{
#if ATOMIC_WORD
	uint64_t v = 0;
	uint64_t v2 = 1;
	uint64_t lock_mask = LOCK_BIT;
	backoff_function bf;
	if (WRITE_PERMISSION_LOCK && type == P_REQ)
		lock_mask = WRITE_BIT;

	while ((v2 | RTS_MASK) != (v | RTS_MASK)) {
		v = _ts_word.load(std::memory_order_relaxed);
		while (v & lock_mask) {
			bf();
			v = _ts_word.load(std::memory_order_relaxed);
		}
		bf.reset();
		std::atomic_thread_fence(std::memory_order_acquire);
		local_row->copy(_row, tuple_size);
		COMPILER_BARRIER
			v2 = _ts_word.load(std::memory_order_acquire);
#if WRITE_PERMISSION_LOCK
		if (type == R_REQ) {
			v |= WRITE_BIT;
			v2 |= WRITE_BIT;
		}
#endif
	}
	txn->last_wts = v & WTS_MASK;
	txn->last_rts = ((v & RTS_MASK) >> WTS_LEN) + txn->last_wts;
#else
	lock();
	txn->last_wts = _wts;
	txn->last_rts = _rts;
	local_row->copy(_row);
	release();
#endif
	return RCOK;
}

void
Row_tictoc::write_data(row_t* data, ts_t wts)
{
#if ATOMIC_WORD
	uint64_t v = _ts_word.load(std::memory_order_relaxed);
#if TICTOC_MV
	_hist_wts = v & WTS_MASK;
#endif
#if WRITE_PERMISSION_LOCK
	//bool result = __sync_bool_compare_and_swap(&_ts_word, v, v | LOCK_BIT);
	bool result = _ts_word.compare_exchange_strong(v, v | LOCK_BIT, std::memory_order_acquire);
	assert(result);
#endif
	v &= ~(RTS_MASK | WTS_MASK); // clear wts and rts.
	v |= wts;
	_ts_word.store(v, std::memory_order_relaxed);
	_row->copy(data, data->get_tuple_size());
#if WRITE_PERMISSION_LOCK
	//_ts_word &= (~LOCK_BIT);
	_ts_word.fetch_and((~LOCK_BIT), std::memory_order_release);
#endif
#else 
#if TICTOC_MV
	_hist_wts = _wts;
#endif
	_wts = wts;
	_rts = wts;
	_row->copy(data);
#endif
}

bool
Row_tictoc::renew_lease(ts_t wts, ts_t rts)
{
#if !ATOMIC_WORD
	if (_wts != wts) {
#if TICTOC_MV
		if (wts == _hist_wts && rts < _wts)
			return true;
#endif
		return false;
	}
	_rts = rts;
#endif
	return true;
}

bool
Row_tictoc::try_renew(ts_t wts, ts_t rts, ts_t& new_rts, uint64_t thd_id)
{
#if ATOMIC_WORD
	uint64_t v = _ts_word.load(std::memory_order_acquire);
	uint64_t lock_mask = (WRITE_PERMISSION_LOCK) ? WRITE_BIT : LOCK_BIT;
	if ((v & WTS_MASK) == wts && ((v & RTS_MASK) >> WTS_LEN) >= rts - wts)
		return true;
	if (v & lock_mask)
		return false;
#if TICTOC_MV
	COMPILER_BARRIER
		uint64_t hist_wts = _hist_wts;
	if (wts != (v & WTS_MASK)) {
		if (wts == hist_wts && rts < (v & WTS_MASK)) {
			return true;
		}
		else {
			return false;
		}
	}
#else
	if (wts != (v & WTS_MASK))
		return false;
#endif

	ts_t delta_rts = rts - wts;
	if (delta_rts < ((v & RTS_MASK) >> WTS_LEN)) // the rts has already been extended.
		return true;
	bool rebase = false;
	if (delta_rts >= (1 << RTS_LEN)) {
		rebase = true;
		uint64_t delta = (delta_rts & ~((1 << RTS_LEN) - 1));
		delta_rts &= ((1 << RTS_LEN) - 1);
		wts += delta;
	}
	uint64_t v2 = 0;
	v2 |= wts;
	v2 |= (delta_rts << WTS_LEN);
	while (true) {
		//uint64_t pre_v = __sync_val_compare_and_swap(&_ts_word, v, v2);
		bool success = _ts_word.compare_exchange_weak(v, v2, std::memory_order_acquire);
		if (success)
			return true;
		//v = pre_v;
		if (rebase || (v & lock_mask) || (wts != (v & WTS_MASK)))
			return false;
		else if (rts < ((v & RTS_MASK) >> WTS_LEN))
			return true;
	}
	assert(false);
	return false;
#else
#if TICTOC_MV
	if (wts < _hist_wts)
		return false;
#else 
	if (wts != _wts)
		return false;
#endif
	int ret = _latch->try_lock();
	if (ret == EBUSY)
		return false;

	if (wts != _wts) {
#if TICTOC_MV
		if (wts == _hist_wts && rts < _wts) {
			_latch->unlock();
			return true;
		}
#endif
		_latch->unlock();
		return false;
	}
	if (rts > _rts)
		_rts = rts;
	_latch->unlock();
	new_rts = rts;
	return true;
#endif
}


ts_t
Row_tictoc::get_wts()
{
#if ATOMIC_WORD
	return _ts_word.load(std::memory_order_relaxed) & WTS_MASK;
#else
	return _wts;
#endif
}

void
Row_tictoc::get_ts_word(bool& lock, uint64_t& rts, uint64_t& wts)
{
	assert(ATOMIC_WORD);
	uint64_t v = _ts_word.load(std::memory_order_relaxed);
	lock = ((v & LOCK_BIT) != 0);
	wts = v & WTS_MASK;
	rts = ((v & RTS_MASK) >> WTS_LEN) + (v & WTS_MASK);
}

ts_t
Row_tictoc::get_rts()
{
#if ATOMIC_WORD
	uint64_t v = _ts_word.load(std::memory_order_relaxed);
	return ((v & RTS_MASK) >> WTS_LEN) + (v & WTS_MASK);
#else
	return _rts;
#endif

}

void
Row_tictoc::lock()
{
#if ATOMIC_WORD  
	uint64_t lock_mask = (WRITE_PERMISSION_LOCK) ? WRITE_BIT : LOCK_BIT;
	uint64_t v = _ts_word.load(std::memory_order_relaxed);
	backoff_function bf;
	while ((v & lock_mask) || !_ts_word.compare_exchange_weak(v, v | lock_mask), std::memory_order_relaxed) {
		bf();
	}
	std::atomic_thread_fence(std::memory_order_acquire);
#else 
	_latch->lock();
#endif
}

bool
Row_tictoc::try_lock()
{
#if ATOMIC_WORD
	uint64_t lock_mask = (WRITE_PERMISSION_LOCK) ? WRITE_BIT : LOCK_BIT;
	uint64_t v = _ts_word.load(std::memory_order_relaxed);
	if (v & lock_mask) // already locked
		return false;
	return _ts_word.compare_exchange_strong(v, v | lock_mask, std::memory_order_acquire);
#else
	return _latch->try_lock();
#endif
}

void
Row_tictoc::release()
{
#if ATOMIC_WORD
	uint64_t lock_mask = (WRITE_PERMISSION_LOCK) ? WRITE_BIT : LOCK_BIT;
	_ts_word.fetch_and((~lock_mask), std::memory_order_release);
#else 
	_latch->unlock();
#endif
}

#endif
