#pragma once 

class table_t;
class Catalog;
class txn_man;
struct TsReqEntry;

#if CC_ALG==SILO
#define LOCK_BIT (1UL << 63)

class alignas(64) Row_silo {
public:
	void 				init(row_t * row);
	RC 					access(txn_man * txn, TsType type, row_t * local_row, uint64_t tuple_size);

	bool				validate(ts_t tid, bool in_write_set);
	void				write(row_t * data, uint64_t tid);

#if ATOMIC_WORD || !defined(SCOPED_LATCH_T) || !SCOPED_LATCH
	void 				lock();
	void 				release();
	bool				try_lock();
#else
#if defined(SCOPED_LATCH_T) && SCOPED_LATCH
	void 				lock(SCOPED_LATCH_T::scoped_lock&);
	void 				release(SCOPED_LATCH_T::scoped_lock&);
	bool				try_lock(SCOPED_LATCH_T::scoped_lock&);
#else
#error First case should apply
#endif  // SCOPED_LATCH
#endif  // ATOMIC_WORD
	uint64_t 			get_tid();

	void 				assert_lock() {
#if ATOMIC_WORD
		assert(_tid_word & LOCK_BIT);
#endif  // ATOMIC_WORD
	}
private:
#if ATOMIC_WORD
	std::atomic_uint64_t	_tid_word;
#else
public:
#if defined(SCOPED_LATCH_T) && SCOPED_LATCH
	SCOPED_LATCH_T _latch;
#elif defined(LATCH_T)
	LATCH_T* _latch;
#endif
private:
	ts_t 				_tid;
#endif
	row_t* _row;
};

#endif
