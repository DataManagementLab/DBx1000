#include "txn.h"
#include "row.h"
#include "row_silo.h"
#include "utils/mem_alloc.h"

#if CC_ALG==SILO

void
Row_silo::init(row_t* row)
{
	_row = row;
#if ATOMIC_WORD
	_tid_word = 0;
#else
#if SCOPED_LATCH
	new(&_latch) SCOPED_LATCH_T;
#else
	_latch = (LATCH_T*)_mm_malloc(sizeof(LATCH_T), 64);
	new(_latch) SCOPED_LATCH_T;
#endif  // SCOPED_LATCH
	_tid = 0;
#endif
}

RC
Row_silo::access(txn_man* txn, TsType type, row_t* local_row, uint64_t tuple_size) {
#if ATOMIC_WORD
	uint64_t v = 0;
	uint64_t v2 = 1;
	backoff_function bf;
	while (v2 != v) {
		v = _tid_word.load(std::memory_order_relaxed);
		while (v & LOCK_BIT) {
			bf();
			v = _tid_word.load(std::memory_order_relaxed);
		}
		bf.reset();
		std::atomic_thread_fence(std::memory_order_acquire);
		local_row->copy(_row, tuple_size);
		COMPILER_BARRIER
			v2 = _tid_word.load(std::memory_order_acquire);
	}
	txn->last_tid = v & (~LOCK_BIT);
#else
#if SCOPED_LATCH
	SCOPED_LATCH_T::scoped_lock scoped_lock_instance;
#define lock_instance scoped_lock_instance
#else
#define lock_instance
#endif  // SCOPED_LATCH
	lock(lock_instance);
	local_row->copy(_row, tuple_size);
	txn->last_tid = _tid;
	release(lock_instance);
#endif
	return RCOK;
}

bool
Row_silo::validate(ts_t tid, bool in_write_set) {
#if ATOMIC_WORD
	uint64_t v = _tid_word.load(std::memory_order_acquire);
	if (in_write_set)
		return tid == (v & (~LOCK_BIT));

	if (v & LOCK_BIT)
		return false;
	else if (tid != (v & (~LOCK_BIT)))
		return false;
	else
		return true;
#else
#if SCOPED_LATCH
	SCOPED_LATCH_T::scoped_lock scoped_lock_instance;
#define lock_instance scoped_lock_instance
#else
#define lock_instance
#endif  // SCOPED_LATCH
	if (in_write_set)
		return tid == _tid;
	if (!try_lock(scoped_lock_instance))
		return false;
	bool valid = (tid == _tid);
	release(scoped_lock_instance);
	return valid;
#endif
}

void
Row_silo::write(row_t* data, uint64_t tid) {
	_row->copy(data, data->get_tuple_size());
#if ATOMIC_WORD
	uint64_t v = _tid_word.load(std::memory_order_relaxed);
	M_ASSERT(tid > (v & (~LOCK_BIT)) && (v & LOCK_BIT), "tid=%ld, v & LOCK_BIT=%ld, v & (~LOCK_BIT)=%ld\n", tid, (v & LOCK_BIT), (v & (~LOCK_BIT)));
	_tid_word.store((tid | LOCK_BIT), std::memory_order_release);
#else
	_tid = tid;
#endif
}

#if ATOMIC_WORD || !defined(SCOPED_LATCH_T) || !SCOPED_LATCH
void
Row_silo::lock() {
#if ATOMIC_WORD
	uint64_t v = _tid_word.load(std::memory_order_relaxed);
	backoff_function bf;
	while ((v & LOCK_BIT) || !_tid_word.compare_exchange_weak(v, v | LOCK_BIT), std::memory_order_relaxed) {
		bf();
	}
	std::atomic_thread_fence(std::memory_order_acquire);
#else
	_latch->lock();
#endif
}

void
Row_silo::release() {
#if ATOMIC_WORD
	assert(_tid_word & LOCK_BIT);
	_tid_word.store(_tid_word.load(std::memory_order_relaxed) & (~LOCK_BIT), std::memory_order_release);
#else 
	_latch->unlock();
#endif
}

bool
Row_silo::try_lock()
{
#if ATOMIC_WORD
	uint64_t v = _tid_word.load(std::memory_order_relaxed);
	if (v & LOCK_BIT) // already locked
		return false;
	return _tid_word.compare_exchange_strong(v, (v | LOCK_BIT), std::memory_order_acquire);
#else
	return _latch->try_lock();
#endif
}
#else 
void
Row_silo::lock(SCOPED_LATCH_T::scoped_lock &lock) {
	lock.acquire(_latch);
}

void
Row_silo::release(SCOPED_LATCH_T::scoped_lock &lock) {
	lock.release();
}

bool
Row_silo::try_lock(SCOPED_LATCH_T::scoped_lock &lock)
{
	return lock.try_acquire(_latch);
}
#endif  // ATOMIC_WORD || !defined(SCOPED_LATCH_T) || !SCOPED_LATCH

uint64_t
Row_silo::get_tid()
{
#if ATOMIC_WORD
	assert(ATOMIC_WORD);
	return _tid_word.load(std::memory_order_relaxed) & (~LOCK_BIT);
#else
	return _tid;
#endif  // ATOMIC_WORD
}

#endif
