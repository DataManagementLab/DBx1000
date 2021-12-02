#include "dl_detect.h"
#include "global.h"
#include "helper.h"
#include "txn.h"
#include "row.h"
#include "manager.h"
#include "utils/mem_alloc.h"

/********************************************************/
// The current txn aborts itself only if it holds less
// locks than all the other txns on the loop. 
// In other words, the victim should be the txn that 
// performs the least amount of work
/********************************************************/
void DL_detect::init() {
	dependency = new DepThd[g_thread_cnt];
	V = g_thread_cnt;
}

int
DL_detect::add_dep(uint64_t txnid1, uint64_t* txnids, int cnt, int num_locks) {
	if (g_no_dl)
		return 0;
	int thd1 = get_thdid_from_txnid(txnid1);

	//acquire lock on dependency item
#if SCOPED_LATCH
	SCOPED_LATCH_T::scoped_lock lock(dependency[thd1].lock);
#else
	dependency[thd1].lock.lock();
#endif // LATCH_T

	dependency[thd1].txnid.store(txnid1, std::memory_order_relaxed);
	dependency[thd1].num_locks = num_locks;

	for (int i = 0; i < cnt; i++)
		dependency[thd1].adj.push_back(txnids[i]);

#if !SCOPED_LATCH
	dependency[thd1].lock.unlock();
#endif  // !SCOPED_LATCH
	return 0;
}

bool
DL_detect::nextNode(uint64_t txnid, DetectData* detect_data) {
	int thd = get_thdid_from_txnid(txnid);
	assert(!detect_data->visited[thd]);
	detect_data->visited[thd] = true;
	detect_data->recStack[thd] = true;

#if SCOPED_LATCH
	SCOPED_LATCH_T::scoped_lock lock(dependency[thd].lock);
#else
	dependency[thd].lock.lock();
#endif  // SCOPED_LATCH

	int lock_num = dependency[thd].num_locks;
	int txnid_num = dependency[thd].adj.size();
	uint64_t txnids[txnid_num];
	int n = 0;

	if (dependency[thd].txnid.load(std::memory_order_relaxed) != (SInt64)txnid) {
		detect_data->recStack[thd] = false;
#if SCOPED_LATCH
		lock.release();
#else
		dependency[thd].lock.unlock();
#endif  // SCOPED_LATCH
		return false;
	}

	for (list<uint64_t>::iterator i = dependency[thd].adj.begin(); i != dependency[thd].adj.end(); ++i) {
		txnids[n++] = *i;
	}

#if SCOPED_LATCH
	lock.release();
#else
	dependency[thd].lock.unlock();
#endif  // SCOPED_LATCH

	for (n = 0; n < txnid_num; n++) {
		int nextthd = get_thdid_from_txnid(txnids[n]);

		// next node not visited and txnid is not stale
		if (detect_data->recStack[nextthd]) {
			if ((SInt32)txnids[n] == dependency[nextthd].txnid.load(std::memory_order_relaxed)) {
				detect_data->loop = true;
				detect_data->onloop = true;
				detect_data->loopstart = nextthd;
				break;
			}
		}
		if (!detect_data->visited[nextthd] &&
			dependency[nextthd].txnid.load(std::memory_order_relaxed) == (SInt64)txnids[n] &&
			nextNode(txnids[n], detect_data))
		{
			break;
		}
	}
	detect_data->recStack[thd] = false;
	if (detect_data->loop
		&& detect_data->onloop
		&& lock_num < detect_data->min_lock_num) {
		detect_data->min_lock_num = lock_num;
		detect_data->min_txnid = txnid;
	}
	if (thd == detect_data->loopstart) {
		detect_data->onloop = false;
	}
	return detect_data->loop;
}

// isCycle returns true if there is a loop AND the current txn holds the least 
// number of locks on that loop.
bool DL_detect::isCyclic(uint64_t txnid, DetectData* detect_data) {
	return nextNode(txnid, detect_data);
}

int
DL_detect::detect_cycle(uint64_t txnid) {
	if (g_no_dl)
		return 0;
	uint64_t starttime = get_sys_clock();
	INC_GLOB_STATS(cycle_detect, 1);
	bool deadlock = false;

	int thd = get_thdid_from_txnid(txnid);
	DetectData* detect_data = (DetectData*)
		mem_allocator.alloc(sizeof(DetectData), thd);
	detect_data->visited = (bool*)
		mem_allocator.alloc(sizeof(bool) * V, thd);
	detect_data->recStack = (bool*)
		mem_allocator.alloc(sizeof(bool) * V, thd);
	for (int i = 0; i < V; i++) {
		detect_data->visited[i] = false;
		detect_data->recStack[i] = false;
	}

	detect_data->min_lock_num = 1000;
	detect_data->min_txnid = -1;
	detect_data->loop = false;

	if (isCyclic(txnid, detect_data)) {
		deadlock = true;
		INC_GLOB_STATS(deadlock, 1);
		int thd_to_abort = get_thdid_from_txnid(detect_data->min_txnid);
		if (dependency[thd_to_abort].txnid.load(std::memory_order_relaxed) == (SInt64)detect_data->min_txnid) {
			txn_man* txn = glob_manager->get_txn_man(thd_to_abort);
			txn->lock_abort.store(true, std::memory_order_relaxed);
		}
	}

	mem_allocator.free(detect_data->visited, sizeof(bool) * V);
	mem_allocator.free(detect_data->recStack, sizeof(bool) * V);
	mem_allocator.free(detect_data, sizeof(DetectData));
	uint64_t timespan = get_sys_clock() - starttime;
	INC_GLOB_STATS(dl_detect_time, timespan);
	if (deadlock) return 1;
	else return 0;
}

void DL_detect::clear_dep(uint64_t txnid) {
	if (g_no_dl)
		return;
	int thd = get_thdid_from_txnid(txnid);
#if SCOPED_LATCH
	SCOPED_LATCH_T::scoped_lock lock(dependency[thd].lock);
#else
	dependency[thd].lock.lock();
#endif  // SCOPED_LATCH

	dependency[thd].adj.clear();
	dependency[thd].txnid.store(-1, std::memory_order_relaxed);
	dependency[thd].num_locks = 0;

#if !SCOPED_LATCH
	dependency[thd].lock.unlock();
#endif  // !SCOPED_LATCH
}

