#ifndef _PLOCK_H_
#define _PLOCK_H_

#include "global.h"
#include "helper.h"

class txn_man;

// Parition manager for HSTORE
class PartMan {
public:
	void init();
	RC lock(txn_man* txn);
	void unlock(txn_man* txn);
private:
#if defined(SCOPED_LATCH_T) && SCOPED_LATCH
	SCOPED_LATCH_T latch;
#elif defined(LATCH_T)
	LATCH_T latch;
#endif
	txn_man* owner;
	txn_man** waiters;
	UInt32 waiter_cnt;
};

// Partition Level Locking
class Plock {
public:
	void init();
	// lock all partitions in parts
	RC lock(txn_man* txn, uint64_t* parts, uint64_t part_cnt);
	void unlock(txn_man* txn, uint64_t* parts, uint64_t part_cnt);
private:
	PartMan** part_mans;
};

#endif
