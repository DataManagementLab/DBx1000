#include <omp.h>

#include "global.h"
#include "helper.h"
#include "tpcc.h"
#include "wl.h"
#include "thread.h"
#include "table.h"
#include "index_hash.h"
#include "index_btree.h"
#include "tpcc_helper.h"
#include "row.h"
#include "query.h"
#include "txn.h"
#include "utils/mem_alloc.h"
#include "tpcc_const.h"

RC tpcc_wl::size_table() {
	num_wh = g_num_wh;

	/******** fill in data ************/
	// data filling process:
	//- item
	//- wh
	//	- stock
	// 	- dist
	//  	- cust
	//	  	- hist
	//		- order 
	//		- new order
	//		- order line
	/**********************************/
    UInt32 buff_size = std::max(g_thread_cnt, g_num_wh);
	tpcc_buffer = new drand48_data * [buff_size];

	for (uint64_t tid = 0; tid < buff_size; ++tid)
	{
		tpcc_buffer[tid] = (drand48_data*)_mm_malloc(sizeof(drand48_data), 64);
		srand48_r(tid + 1, tpcc_buffer[tid]);
	}
    
	auto wl = this;

#pragma omp parallel
	{
		uint32_t wh_per_thread = g_num_wh / omp_get_num_threads();
		uint32_t wh_rest = g_num_wh - wh_per_thread * omp_get_num_threads();
		uint32_t num_wh = wh_per_thread + (omp_get_thread_num() < wh_rest);
		uint32_t prev_wh = wh_per_thread * omp_get_thread_num() + std::min((uint32_t)omp_get_thread_num(), wh_rest);
		for (uint32_t i = 0; i < num_wh; ++i)
		{
			threadSizeWarehouse(wl, prev_wh + i);
		}

#if REPLICATE_ITEM
		static std::array<std::atomic_flag, MAX_SOCKETS> socket_inited;

		int node = get_node();

		if (socket_inited.at(node).test_and_set() == false)
		{
			std::cout << "Initialising item replica on node: " << node << "\n";
			wl->workload_size += g_max_items * sizeof(i_record);
		}
#endif  // REPLICATE_ITEM
	}

	printf("TPCC Data Size Calculation Complete!\n");
	std::cout << "Workload size: " << wl->workload_size << "\n";
	
	return RCOK;
}

// TODO ITEM table is assumed to be in partition 0
void tpcc_wl::size_tab_item(uint32_t part_id) {

	thread_workload_size += g_max_items * sizeof(i_record);
}

void tpcc_wl::size_tab_wh(uint32_t wid) {
	assert(wid >= 1 && wid <= g_num_wh);
	
	thread_workload_size += sizeof(w_record);
	return;
}

void tpcc_wl::size_tab_dist(uint64_t wid) {

	thread_workload_size += DIST_PER_WARE * sizeof(d_record);
}

void tpcc_wl::size_tab_stock(uint64_t wid) {

    thread_workload_size += g_max_items * sizeof(s_record);
}

void tpcc_wl::size_tab_cust(uint64_t did, uint64_t wid) {
	assert(g_cust_per_dist >= 1000);

	thread_workload_size += g_cust_per_dist * sizeof(c_record);
}

void tpcc_wl::size_tab_hist(uint64_t c_id, uint64_t d_id, uint64_t w_id) {

	thread_workload_size += sizeof(h_record);

}

void tpcc_wl::size_tab_order(uint64_t did, uint64_t wid) {
	for (UInt64 oid = 1; oid <= g_cust_per_dist; oid++) {
		uint64_t o_ol_cnt = 1;
		o_ol_cnt = URand(5, 15, wid - 1);

		// ORDER-LINE	
#if !TPCC_SMALL
		thread_workload_size += o_ol_cnt * sizeof(ol_record);
#endif
		// NEW ORDER
		if (oid > 2100) {
			thread_workload_size += sizeof(no_record);
		}
	}
	thread_workload_size += g_cust_per_dist * sizeof(o_record);
}

/*==================================================================+
| ROUTINE NAME
| GetPermutation
+==================================================================*/

void* tpcc_wl::threadSizeWarehouse(void* This, int tid) {
	tpcc_wl* wl = (tpcc_wl*)This;
	//int tid = ATOM_FETCH_ADD(wl->next_tid, 1);
	uint32_t wid = tid + 1;
	assert((uint64_t)tid < g_num_wh);

	thread_workload_size = 0;

#if !REPLICATE_ITEM
	if (tid == 0)
		wl->size_tab_item();
#endif  // REPLICATE_ITEM
	wl->size_tab_wh(wid);
	wl->size_tab_dist(wid);
	wl->size_tab_stock(wid);
	for (uint64_t did = 1; did <= DIST_PER_WARE; did++) {
		wl->size_tab_cust(did, wid);
		wl->size_tab_order(did, wid);
		for (uint64_t cid = 1; cid <= g_cust_per_dist; cid++)
			wl->size_tab_hist(cid, did, wid);
	}

#pragma omp critical
	{
		std::cout << "Size of warehouse " << wid << ": " << thread_workload_size << "\n";
	}

	wl->workload_size += thread_workload_size;

	return NULL;
}
