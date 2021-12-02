#ifndef _TPCC_QUERY_H_
#define _TPCC_QUERY_H_

#include "global.h"
#include "helper.h"
#include "query.h"
#include <omp.h>

class workload;

// items of new order transaction
struct Item_no {
	uint64_t ol_i_id;
	uint64_t ol_supply_w_id;
	uint64_t ol_quantity;
};

class tpcc_query : public base_query {
public:
	void init(uint64_t thd_id, workload* h_wl);
	void init(uint64_t thd_id, workload* h_wl, uint64_t *pre_alloced_part_to_access);
	TPCCTxnType type;
	/**********************************************/
	// common txn input for both payment & new-order
	/**********************************************/
	uint64_t w_id;
	uint64_t d_id;
	uint64_t c_id;
	/**********************************************/
	// txn input for payment
	/**********************************************/
	uint64_t d_w_id;
	uint64_t c_w_id;
	uint64_t c_d_id;
	char c_last[LASTNAME_LEN];
	double h_amount;
	bool by_last_name;
	/**********************************************/
	// txn input for new-order
	/**********************************************/
	Item_no* items;
	bool rbk;
	bool remote;
	uint64_t ol_cnt;
	uint64_t o_entry_d;
	// Input for delivery
	uint64_t o_carrier_id;
	uint64_t ol_delivery_d;
	// for order-status
	// for stock-level
	uint64_t threshold;


private:
	// warehouse id to partition id mapping
//	uint64_t wh_to_part(uint64_t wid);
	void gen_payment(uint64_t thd_id);
	void gen_new_order(uint64_t thd_id);
	void gen_order_status(uint64_t thd_id);
	void gen_delivery(uint64_t thd_id);
	void gen_stock(uint64_t thd_id);

	void wh_of_socket(uint64_t socket_id, uint64_t &wh_id_begin, uint64_t &wh_id_end) {

		assert(g_threads_per_socket);

		uint64_t wh_per_thread = g_num_wh / omp_get_num_threads();
		uint64_t wh_rest = g_num_wh - wh_per_thread * omp_get_num_threads();
		uint64_t thread_num_begin = socket_id * g_threads_per_socket;
		uint64_t thread_num_end = (socket_id + 1) * g_threads_per_socket;
		wh_id_begin = 1 + wh_per_thread * thread_num_begin + std::min(thread_num_begin, wh_rest);
		wh_id_end = wh_per_thread * thread_num_end + std::min(thread_num_end, wh_rest);

		assert(wh_id_begin <= g_num_wh);
		assert(wh_id_end <= g_num_wh);
	}

	void wh_numa_dist(uint64_t numa_distance, uint64_t &wh_id_begin, uint64_t &wh_id_end) {
		auto&& this_socket = get_node();
		uint64_t target_socket;
		if (numa_distance == 0) {
			target_socket = this_socket;
		} else {
			target_socket = g_numa_dist_matrix[this_socket][numa_distance];
		}
		wh_of_socket(target_socket, wh_id_begin, wh_id_end);
	}

	void wh_enforce_numa_dist(uint64_t &wh_id) {
		auto orig = wh_id;
		if (g_enforce_numa_distance) {
			uint64_t begin, end;
			wh_numa_dist(g_numa_distance, begin, end);
			uint64_t range = end - begin + 1;
			wh_id = begin + ((wh_id - 1) % range);
		}
	}
};

#endif
