#include "global.h"
#include "query.h"
#include "tpcc_query.h"
#include "tpcc.h"
#include "tpcc_helper.h"
#include "mem_alloc.h"
#include "wl.h"
#include "table.h"

void tpcc_query::init(uint64_t thd_id, workload* h_wl) {
	init(thd_id, h_wl, NULL);
}

void tpcc_query::init(uint64_t thd_id, workload* h_wl, uint64_t *pre_alloced_part_to_access) {
	double x = (double)(RAND(100, thd_id)) / 100.0;

	if (pre_alloced_part_to_access != NULL) {
		part_to_access = pre_alloced_part_to_access;
	}
	else {
		part_to_access = (uint64_t*)
			mem_allocator.alloc(sizeof(uint64_t) * g_part_cnt, thd_id);
	}
#if TPCC_ALL_TXN
	double perc_stock = 0.04;
	double perc_delivery = perc_stock + 0.04;
	double perc_order_status = perc_delivery + 0.04;
	double perc_payment = perc_delivery + (1.0 - perc_delivery) * g_perc_payment;

	if (x < perc_stock) {
		gen_stock(thd_id);
	} else if (x < perc_delivery) {
		gen_delivery(thd_id);
	} else if (x < perc_order_status) {
		gen_order_status(thd_id);
	} else if ( x < perc_payment) {
		gen_payment(thd_id);
	} else {
		gen_new_order(thd_id);
	}
	
#else
	if (x < g_perc_payment)
		gen_payment(thd_id);
	else
		gen_new_order(thd_id);
#endif  // TPCC_ALL_TXN
}

void tpcc_query::gen_payment(uint64_t thd_id) {
	type = TPCC_PAYMENT;
	if (FIRST_PART_LOCAL)
		w_id = thd_id % g_num_wh + 1;
	else
		w_id = URand(1, g_num_wh, thd_id % g_num_wh);
	
	if(!g_enforce_numa_distance_remote_only)
		wh_enforce_numa_dist(w_id);

	d_w_id = w_id;
	uint64_t part_id = wh_to_part(w_id);
	part_to_access[0] = part_id;
	part_num = 1;

	d_id = URand(1, DIST_PER_WARE, thd_id);
	h_amount = URand(1, 5000, thd_id);
	int x = URand(1, 100, thd_id);
	int y = URand(1, 100, thd_id);


	if (x <= (100 - g_perc_remote_payment)) {
		// home warehouse
		c_d_id = d_id;
		c_w_id = w_id;
	}
	else {
		// remote warehouse
		c_d_id = URand(1, DIST_PER_WARE, thd_id);
		if (g_num_wh > 1) {
			do {
				c_w_id = URand(1, g_num_wh, thd_id);
				wh_enforce_numa_dist(c_w_id);
			} while (c_w_id == w_id);
			
			if (wh_to_part(w_id) != wh_to_part(c_w_id)) {
				part_to_access[1] = wh_to_part(c_w_id);
				part_num = 2;
			}
		}
		else
			c_w_id = w_id;
	}
	if (y <= 60) {
		// by last name
		by_last_name = true;
		Lastname(NURand(255, 0, 999, thd_id), c_last);
	}
	else {
		// by cust id
		by_last_name = false;
		c_id = NURand(1023, 1, g_cust_per_dist, thd_id);
	}
}

void tpcc_query::gen_new_order(uint64_t thd_id) {
	type = TPCC_NEW_ORDER;
	if (FIRST_PART_LOCAL)
		w_id = thd_id % g_num_wh + 1;
	else
		w_id = URand(1, g_num_wh, thd_id % g_num_wh);

	if (!g_enforce_numa_distance_remote_only)
		wh_enforce_numa_dist(w_id);

	d_id = URand(1, DIST_PER_WARE, thd_id);
	c_id = NURand(1023, 1, g_cust_per_dist, thd_id);
	rbk = URand(1, 100, thd_id);
	ol_cnt = URand(5, 15, thd_id);
	o_entry_d = 2013;
	items = (Item_no*)_mm_malloc(sizeof(Item_no) * ol_cnt, 64);
	remote = false;
	part_to_access[0] = wh_to_part(w_id);
	part_num = 1;

	for (UInt32 oid = 0; oid < ol_cnt; oid++) {
		items[oid].ol_i_id = NURand(8191, 1, g_max_items, thd_id);
		UInt32 x = URand(1, 100, thd_id);
		if (x > g_perc_remote_neworder || g_num_wh == 1)
			items[oid].ol_supply_w_id = w_id;
		else {
			do {
				items[oid].ol_supply_w_id = URand(1, g_num_wh, thd_id);
				wh_enforce_numa_dist(items[oid].ol_supply_w_id);
			} while (items[oid].ol_supply_w_id == w_id);
			
			remote = true;
		}
		items[oid].ol_quantity = URand(1, 10, thd_id);
	}
	// Remove duplicate items
	for (UInt32 i = 0; i < ol_cnt; i++) {
		for (UInt32 j = 0; j < i; j++) {
			if (items[i].ol_i_id == items[j].ol_i_id) {
				for (UInt32 k = i; k < ol_cnt - 1; k++)
					items[k] = items[k + 1];
				ol_cnt--;
				i--;
			}
		}
	}
	for (UInt32 i = 0; i < ol_cnt; i++)
		for (UInt32 j = 0; j < i; j++)
			assert(items[i].ol_i_id != items[j].ol_i_id);
	// update part_to_access
	for (UInt32 i = 0; i < ol_cnt; i++) {
		UInt32 j;
		for (j = 0; j < part_num; j++)
			if (part_to_access[j] == wh_to_part(items[i].ol_supply_w_id))
				break;
		if (j == part_num) // not found! add to it.
			part_to_access[part_num++] = wh_to_part(items[i].ol_supply_w_id);
	}
}

void
tpcc_query::gen_order_status(uint64_t thd_id) {
	type = TPCC_ORDER_STATUS;
	if (FIRST_PART_LOCAL)
		w_id = thd_id % g_num_wh + 1;
	else
		w_id = URand(1, g_num_wh, thd_id % g_num_wh);

	if (!g_enforce_numa_distance_remote_only)
		wh_enforce_numa_dist(w_id);

	part_to_access[0] = wh_to_part(w_id);
	part_num = 1;

	d_id = URand(1, DIST_PER_WARE, thd_id);
	c_w_id = w_id;
	c_d_id = d_id;
	int y = URand(1, 100, thd_id);
	if (y <= 60) {
		// by last name
		by_last_name = true;
		Lastname(NURand(255, 0, 999, thd_id), c_last);
	}
	else {
		// by cust id
		by_last_name = false;
		c_id = NURand(1023, 1, g_cust_per_dist, thd_id);
	}
}

void
tpcc_query::gen_delivery(uint64_t thd_id) {
	type = TPCC_DELIVERY;

	if (FIRST_PART_LOCAL)
		w_id = thd_id % g_num_wh + 1;
	else
		w_id = URand(1, g_num_wh, thd_id % g_num_wh);

	part_to_access[0] = wh_to_part(w_id);
	part_num = 1;

	o_carrier_id = URand(1, 10, thd_id);
	ol_delivery_d = 2021;
}

void
tpcc_query::gen_stock(uint64_t thd_id) {
	type = TPCC_STOCK_LEVEL;

	if (FIRST_PART_LOCAL)
		w_id = thd_id % g_num_wh + 1;
	else
		w_id = URand(1, g_num_wh, thd_id % g_num_wh);

	part_to_access[0] = wh_to_part(w_id);
	part_num = 1;

	// d_id constant
	d_id = thd_id % DIST_PER_WARE + 1;

	// threshold of minimum quantity of stock random[10 .. 20]
	threshold = URand(10, 20, thd_id);
}
