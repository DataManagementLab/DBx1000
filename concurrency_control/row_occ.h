#ifndef ROW_OCC_H
#define ROW_OCC_H

class table_t;
class Catalog;
class txn_man;
struct TsReqEntry;

class Row_occ {
public:
	void 				init(row_t* row);
	RC 					access(txn_man* txn, TsType type, uint64_t tuple_size);
#if SCOPED_LATCH
	void 				latch(SCOPED_LATCH_T::scoped_lock& lock);
	bool 				try_latch(SCOPED_LATCH_T::scoped_lock& lock);
	void 				release();
#else
	void 				latch();
	bool 				try_latch();
	void 				release();
#endif  // SCOPED_LATCH
	// ts is the start_ts of the validating txn 
	bool				validate(uint64_t ts);
	void				write(row_t* data, uint64_t ts, uint64_t tuple_size);
private:
#if defined(SCOPED_LATCH_T) && SCOPED_LATCH
	SCOPED_LATCH_T _latch;
#elif defined(LATCH_T)
	LATCH_T _latch;
#endif
	//bool 				blatch;

	row_t* _row;
	// the last update time
	ts_t 				wts;
};

#endif
