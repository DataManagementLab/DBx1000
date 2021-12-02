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

RC tpcc_wl::init() {
	workload::init();

#if TPCC_SMALL
	string path = "TPCC_short_schema.txt";
#else
#if TPCC_ALL_TXN
	string path = "TPCC_full_schema-all_txn.txt";
#else
	string path = "TPCC_full_schema.txt";
#endif  // TPCC_ALL_TXN
#endif
	cout << "reading schema file: " << path << endl;
	init_schema(path.c_str());
	cout << "TPCC schema initialized" << endl;
	if (g_calc_size_only) {
		size_table();
		return ERROR;
	}
	else {
		init_table();
	}
	next_tid = 0;

	for(auto &&d : delivering) {
		d.clear();
	}

	return RCOK;
}

RC tpcc_wl::init_schema(const char* schema_file) {
	workload::init_schema(schema_file);
	t_warehouse = tables["WAREHOUSE"];
	t_district = tables["DISTRICT"];
	t_customer = tables["CUSTOMER"];
	t_history = tables["HISTORY"];
	t_neworder = tables["NEW-ORDER"];
	t_order = tables["ORDER"];
	t_orderline = tables["ORDER-LINE"];
	t_item = tables["ITEM"];
	t_stock = tables["STOCK"];

	i_item = indexes["ITEM_IDX"];
	i_warehouse = indexes["WAREHOUSE_IDX"];
	i_district = indexes["DISTRICT_IDX"];
	i_customer_id = indexes["CUSTOMER_ID_IDX"];
#if INDEX_STRUCT == IDX_HASH_PERFECT
	i_customer_last = reinterpret_cast<IndexHash*>(indexes["CUSTOMER_LAST_IDX"]);
#else
	i_customer_last = indexes["CUSTOMER_LAST_IDX"];
#endif  // IDX_HASH_PERFECT
	i_stock = indexes["STOCK_IDX"];
	i_order = indexes["ORDER_IDX"];
#if TPCC_ALL_TXN
	i_order_cust = indexes["ORDER_CUST_IDX"];
	i_orderline = indexes["ORDER-LINE_IDX"];
	i_neworder = indexes["NEW-ORDER_IDX"];
#endif  // TPCC_ALL_TXN

	return RCOK;
}

RC tpcc_wl::init_table() {
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
	/*
	pthread_t * p_thds = new pthread_t[g_num_wh - 1];
	for (uint32_t i = 0; i < g_num_wh - 1; i++)
		pthread_create(&p_thds[i], NULL, threadInitWarehouse, this);
	threadInitWarehouse(this);
	for (uint32_t i = 0; i < g_num_wh - 1; i++)
		pthread_join(p_thds[i], NULL);
	*/

	auto wl = this;

#pragma omp parallel
	{
		uint32_t wh_per_thread = g_num_wh / omp_get_num_threads();
		uint32_t wh_rest = g_num_wh - wh_per_thread * omp_get_num_threads();
		uint32_t num_wh = wh_per_thread + (omp_get_thread_num() < wh_rest);
		uint32_t prev_wh = wh_per_thread * omp_get_thread_num() + std::min((uint32_t)omp_get_thread_num(), wh_rest);
		for (uint32_t i = 0; i < num_wh; ++i)
		{
			threadInitWarehouse(wl, prev_wh + i);
		}

#if REPLICATE_ITEM
		static std::array<std::atomic_flag, MAX_SOCKETS> socket_inited;

		int node = get_node();

		if (socket_inited.at(node).test_and_set() == false)
		{
			std::cout << "Initialising item replica on node: " << node << "\n";
			
			wl->workload_size += g_max_items * sizeof(i_record);
			for (UInt32 i = 1; i <= g_max_items; i++) {
				row_t* row;
				uint64_t row_id;
				wl->t_item->get_new_row(row, 0, row_id, sizeof(i_record));
				row->set_primary_key(i);
				row->set_value(I_ID, UInt64(i), offsetof(i_record, I_ID), sizeof(i_record::I_ID));
				row->set_value(I_IM_ID, URand(1L, 10000L, 0), offsetof(i_record, I_IM_ID), sizeof(i_record::I_IM_ID));
				char name[24];
				MakeAlphaString(14, 24, name, 0);
				row->set_value(I_NAME, name, offsetof(i_record, I_NAME), sizeof(i_record::I_NAME));
				row->set_value(I_PRICE, URand(1, 100, 0), offsetof(i_record, I_PRICE), sizeof(i_record::I_PRICE));
				char data[50];
				MakeAlphaString(26, 50, data, 0);
				// TODO in TPCC, "original" should start at a random position
				if (RAND(10, 0) == 0)
					strcpy(data, "original");
				row->set_value(I_DATA, data, offsetof(i_record, I_DATA), sizeof(i_record::I_DATA));

				wl->index_insert(i_item, i, row, node);
			}
		}
#endif  // REPLICATE_ITEM
	}

#if TPCC_ALL_TXN
#ifdef DEBUG
	uint64_t wid = 1;
	uint64_t key = orderCustKey(1, 1);
	INDEX * index = wl->i_order_cust;
	itemid_t * item = NULL;
	index->index_read(key, item, wh_to_part(wid), wid - 1);
	assert(item != NULL);

	index = wl->i_order;
	item = NULL;
	key = orderPrimaryKey(wid, 1, 3002);
	index->index_read(key, item, d_to_part(wid, 1), wid - 1);
	static int i = 0;
	assert(item == NULL);
#endif  // DEBUG
#endif  // TPCC_ALL_TXN

	printf("TPCC Data Initialization Complete!\n");
	std::cout << "Workload size: " << wl->workload_size << "\n";

	return RCOK;
}

RC tpcc_wl::get_txn_man(txn_man*& txn_manager, thread_t* h_thd) {
	txn_manager = (tpcc_txn_man*)_mm_malloc(sizeof(tpcc_txn_man), 64);
	new(txn_manager) tpcc_txn_man();
	txn_manager->init(h_thd, this, h_thd->get_thd_id());
	return RCOK;
}

// TODO ITEM table is assumed to be in partition 0
void tpcc_wl::init_tab_item(uint32_t part_id) {
	
	thread_workload_size += g_max_items * sizeof(i_record);
	for (UInt64 i = 1; i <= g_max_items; i++) {
		row_t* row;
		uint64_t row_id;
		t_item->get_new_row(row, 0, row_id, sizeof(i_record));
		row->set_primary_key(i);
		row->set_value(I_ID, i, offsetof(i_record, I_ID), sizeof(i_record::I_ID));
		row->set_value(I_IM_ID, URand(1L, 10000L, 0), offsetof(i_record, I_IM_ID), sizeof(i_record::I_IM_ID));
		char name[24];
		MakeAlphaString(14, 24, name, 0);
		row->set_value(I_NAME, name, offsetof(i_record, I_NAME), sizeof(i_record::I_NAME));
		row->set_value(I_PRICE, URand(1, 100, 0), offsetof(i_record, I_PRICE), sizeof(i_record::I_PRICE));
		char data[50];
		MakeAlphaString(26, 50, data, 0);
		// TODO in TPCC, "original" should start at a random position
		if (RAND(10, 0) == 0)
			strcpy(data, "original");
		row->set_value(I_DATA, data, offsetof(i_record, I_DATA), sizeof(i_record::I_DATA));

		index_insert(i_item, i, row, part_id);
	}
}

void tpcc_wl::init_tab_wh(uint32_t wid) {
	assert(wid >= 1 && wid <= g_num_wh);

	thread_workload_size += sizeof(w_record);

	row_t* row;
	uint64_t row_id;
	t_warehouse->get_new_row(row, 0, row_id, sizeof(w_record));
	row->set_primary_key(wid);

	row->set_value(W_ID, uint64_t(wid), offsetof(w_record, W_ID), sizeof(w_record::W_ID));
	char name[10];
	MakeAlphaString(6, 10, name, wid - 1);
	row->set_value(W_NAME, name, offsetof(w_record, W_NAME), sizeof(w_record::W_NAME));
	char street[20];
	MakeAlphaString(10, 20, street, wid - 1);
	row->set_value(W_STREET_1, street, offsetof(w_record, W_STREET_1), sizeof(w_record::W_STREET_1));
	MakeAlphaString(10, 20, street, wid - 1);
	row->set_value(W_STREET_2, street, offsetof(w_record, W_STREET_2), sizeof(w_record::W_STREET_2));
	MakeAlphaString(10, 20, street, wid - 1);
	row->set_value(W_CITY, street, offsetof(w_record, W_CITY), sizeof(w_record::W_CITY));
	char state[2];
	MakeAlphaString(2, 2, state, wid - 1); /* State */
	row->set_value(W_STATE, state, offsetof(w_record, W_STATE), sizeof(w_record::W_STATE));
	char zip[9];
	MakeNumberString(9, 9, zip, wid - 1); /* Zip */
	row->set_value(W_ZIP, zip, offsetof(w_record, W_ZIP), sizeof(w_record::W_ZIP));
	double tax = (double)URand(0L, 200L, wid - 1) / 1000.0;
	double w_ytd = 300000.00;
	row->set_value(W_TAX, tax, offsetof(w_record, W_TAX), sizeof(w_record::W_TAX));
	row->set_value(W_YTD, w_ytd, offsetof(w_record, W_YTD), sizeof(w_record::W_YTD));

	index_insert(i_warehouse, wid, row, wh_to_part(wid));
	return;
}

void tpcc_wl::init_tab_dist(uint64_t wid) {

	thread_workload_size += DIST_PER_WARE * sizeof(d_record);
	for (uint64_t did = 1; did <= DIST_PER_WARE; did++) {
		row_t* row;
		uint64_t row_id;
		t_district->get_new_row(row, 0, row_id, sizeof(d_record));
		row->set_primary_key(did);

		row->set_value(D_ID, did, offsetof(d_record, D_ID), sizeof(d_record::D_ID));
		row->set_value(D_W_ID, wid, offsetof(d_record, D_W_ID), sizeof(d_record::D_W_ID));
		char name[10];
		MakeAlphaString(6, 10, name, wid - 1);
		row->set_value(D_NAME, name, offsetof(d_record, D_NAME), sizeof(d_record::D_NAME));
		char street[20];
		MakeAlphaString(10, 20, street, wid - 1);
		row->set_value(D_STREET_1, street, offsetof(d_record, D_STREET_1), sizeof(d_record::D_STREET_1));
		MakeAlphaString(10, 20, street, wid - 1);
		row->set_value(D_STREET_2, street, offsetof(d_record, D_STREET_2), sizeof(d_record::D_STREET_2));
		MakeAlphaString(10, 20, street, wid - 1);
		row->set_value(D_CITY, street, offsetof(d_record, D_CITY), sizeof(d_record::D_CITY));
		char state[2];
		MakeAlphaString(2, 2, state, wid - 1); /* State */
		row->set_value(D_STATE, state, offsetof(d_record, D_STATE), sizeof(d_record::D_STATE));
		char zip[9];
		MakeNumberString(9, 9, zip, wid - 1); /* Zip */
		row->set_value(D_ZIP, zip, offsetof(d_record, D_ZIP), sizeof(d_record::D_ZIP));
		double tax = (double)URand(0L, 200L, wid - 1) / 1000.0;
		double w_ytd = 30000.00;
		row->set_value(D_TAX, tax, offsetof(d_record, D_TAX), sizeof(d_record::D_TAX));
		row->set_value(D_YTD, w_ytd, offsetof(d_record, D_YTD), sizeof(d_record::D_YTD));
		row->set_value(D_NEXT_O_ID, int64_t(3001), offsetof(d_record, D_NEXT_O_ID), sizeof(d_record::D_NEXT_O_ID));

		index_insert(i_district, distKey(did, wid), row, wh_to_part(wid));
	}
}

void tpcc_wl::init_tab_stock(uint64_t wid) {

	thread_workload_size += g_max_items * sizeof(s_record);
	for (UInt64 sid = 1; sid <= g_max_items; sid++) {
		row_t* row;
		uint64_t row_id;
		t_stock->get_new_row(row, 0, row_id, sizeof(s_record));
		row->set_primary_key(sid);
		row->set_value(S_I_ID, sid, offsetof(s_record, S_I_ID), sizeof(s_record::S_I_ID));
		row->set_value(S_W_ID, wid, offsetof(s_record, S_W_ID), sizeof(s_record::S_W_ID));
		row->set_value(S_QUANTITY, URand(10, 100, wid - 1), offsetof(s_record, S_QUANTITY), sizeof(s_record::S_QUANTITY));
		row->set_value(S_REMOTE_CNT, uint64_t(0), offsetof(s_record, S_REMOTE_CNT), sizeof(s_record::S_REMOTE_CNT));
#if !TPCC_SMALL
		char s_dist[25];
		char row_name[10] = "S_DIST_";
		for (int i = 1; i <= 10; i++) {
			if (i < 10) {
				row_name[7] = '0';
				row_name[8] = i + '0';
			}
			else {
				row_name[7] = '1';
				row_name[8] = '0';
			}
			row_name[9] = '\0';
			MakeAlphaString(24, 24, s_dist, wid - 1);
			// WARN: Relying on packed s_record struct here!
			row->set_value(row_name, s_dist, offsetof(s_record, S_DIST_01) + sizeof(s_record::S_DIST_01) * (i - 1), sizeof(s_record::S_DIST_01));
		}
		row->set_value(S_YTD, uint64_t(0), offsetof(s_record, S_YTD), sizeof(s_record::S_YTD));
		row->set_value(S_ORDER_CNT, uint64_t(0), offsetof(s_record, S_ORDER_CNT), sizeof(s_record::S_ORDER_CNT));
		char s_data[51];
		int len = MakeAlphaString(26, 50, s_data, wid - 1);
		if (RAND(100, wid - 1) < 10) {
			int idx = URand(0, len - 8, wid - 1);
			strcpy(&s_data[idx], "original");
		}
		row->set_value(S_DATA, s_data, offsetof(s_record, S_DATA), sizeof(s_record::S_DATA));
#endif
		index_insert(i_stock, stockKey(sid, wid), row, wh_to_part(wid));
	}
}

void tpcc_wl::init_tab_cust(uint64_t did, uint64_t wid) {
	assert(g_cust_per_dist >= 1000);
	
	thread_workload_size += g_cust_per_dist * sizeof(c_record);
	for (UInt64 cid = 1; cid <= g_cust_per_dist; cid++) {
		row_t* row;
		uint64_t row_id;
		t_customer->get_new_row(row, 0, row_id, sizeof(c_record));
		row->set_primary_key(cid);

		row->set_value(C_ID, cid, offsetof(c_record, C_ID), sizeof(c_record::C_ID));
		row->set_value(C_D_ID, did, offsetof(c_record, C_D_ID), sizeof(c_record::C_D_ID));
		row->set_value(C_W_ID, wid, offsetof(c_record, C_W_ID), sizeof(c_record::C_W_ID));
		char c_last[LASTNAME_LEN];
		if (cid <= 1000)
			Lastname(cid - 1, c_last);
		else
			Lastname(NURand(255, 0, 999, wid - 1), c_last);
		row->set_value(C_LAST, c_last, offsetof(c_record, C_LAST), sizeof(c_record::C_LAST));
#if !TPCC_SMALL
		char tmp[3] = "OE";
		row->set_value(C_MIDDLE, tmp, offsetof(c_record, C_MIDDLE), sizeof(c_record::C_MIDDLE));
		char c_first[FIRSTNAME_LEN];
		MakeAlphaString(FIRSTNAME_MINLEN, sizeof(c_first), c_first, wid - 1);
		row->set_value(C_FIRST, c_first, offsetof(c_record, C_FIRST), sizeof(c_record::C_FIRST));
		char street[20];
		MakeAlphaString(10, 20, street, wid - 1);
		row->set_value(C_STREET_1, street, offsetof(c_record, C_STREET_1), sizeof(c_record::C_STREET_1));
		MakeAlphaString(10, 20, street, wid - 1);
		row->set_value(C_STREET_2, street, offsetof(c_record, C_STREET_2), sizeof(c_record::C_STREET_2));
		MakeAlphaString(10, 20, street, wid - 1);
		row->set_value(C_CITY, street, offsetof(c_record, C_CITY), sizeof(c_record::C_CITY));
		char state[2];
		MakeAlphaString(2, 2, state, wid - 1); /* State */
		row->set_value(C_STATE, state, offsetof(c_record, C_STATE), sizeof(c_record::C_STATE));
		char zip[9];
		MakeNumberString(9, 9, zip, wid - 1); /* Zip */
		row->set_value(C_ZIP, zip, offsetof(c_record, C_ZIP), sizeof(c_record::C_ZIP));
		char phone[16];
		MakeNumberString(16, 16, phone, wid - 1); /* Zip */
		row->set_value(C_PHONE, phone, offsetof(c_record, C_PHONE), sizeof(c_record::C_PHONE));
		row->set_value(C_SINCE, uint64_t(0), offsetof(c_record, C_SINCE), sizeof(c_record::C_SINCE));
		row->set_value(C_CREDIT_LIM, uint64_t(50000), offsetof(c_record, C_CREDIT_LIM), sizeof(c_record::C_CREDIT_LIM));
		row->set_value(C_DELIVERY_CNT, uint64_t(0), offsetof(c_record, C_DELIVERY_CNT), sizeof(c_record::C_DELIVERY_CNT));
		char c_data[500];
		MakeAlphaString(300, 500, c_data, wid - 1);
		row->set_value(C_DATA, c_data, offsetof(c_record, C_DATA), sizeof(c_record::C_DATA));
#endif
		if (RAND(10, wid - 1) == 0) {
			char tmp[] = "GC";
			row->set_value(C_CREDIT, tmp, offsetof(c_record, C_CREDIT), sizeof(c_record::C_CREDIT));
		}
		else {
			char tmp[] = "BC";
			row->set_value(C_CREDIT, tmp, offsetof(c_record, C_CREDIT), sizeof(c_record::C_CREDIT));
		}
		row->set_value(C_DISCOUNT, (double)RAND(5000, wid - 1) / 10000, offsetof(c_record, C_DISCOUNT), sizeof(c_record::C_DISCOUNT));
		row->set_value(C_BALANCE, double(-10.0), offsetof(c_record, C_BALANCE), sizeof(c_record::C_BALANCE));
		row->set_value(C_YTD_PAYMENT, double(10.0), offsetof(c_record, C_YTD_PAYMENT), sizeof(c_record::C_YTD_PAYMENT));
		row->set_value(C_PAYMENT_CNT, uint64_t(1), offsetof(c_record, C_PAYMENT_CNT), sizeof(c_record::C_PAYMENT_CNT));
		uint64_t key;
		key = custNPKey(c_last, did, wid);
		index_insert(i_customer_last, key, row, wh_to_part(wid));
		key = custKey(cid, did, wid);
		index_insert(i_customer_id, key, row, wh_to_part(wid));
	}
}

void tpcc_wl::init_tab_hist(uint64_t c_id, uint64_t d_id, uint64_t w_id) {
	
	thread_workload_size += sizeof(h_record);

	row_t* row;
	uint64_t row_id;
	t_history->get_new_row(row, 0, row_id, sizeof(h_record));
	row->set_primary_key(0);
	row->set_value(H_C_ID, c_id, offsetof(h_record, H_C_ID), sizeof(h_record::H_C_ID));
	row->set_value(H_C_D_ID, d_id, offsetof(h_record, H_C_D_ID), sizeof(h_record::H_C_D_ID));
	row->set_value(H_D_ID, d_id, offsetof(h_record, H_D_ID), sizeof(h_record::H_D_ID));
	row->set_value(H_C_W_ID, w_id, offsetof(h_record, H_C_W_ID), sizeof(h_record::H_C_W_ID));
	row->set_value(H_W_ID, w_id, offsetof(h_record, H_W_ID), sizeof(h_record::H_W_ID));
	row->set_value(H_DATE, uint64_t(0), offsetof(h_record, H_DATE), sizeof(h_record::H_DATE));
	row->set_value(H_AMOUNT, double(10.0), offsetof(h_record, H_AMOUNT), sizeof(h_record::H_AMOUNT));
#if !TPCC_SMALL
	char h_data[24];
	MakeAlphaString(12, 24, h_data, w_id - 1);
	row->set_value(H_DATA, h_data, offsetof(h_record, H_DATA), sizeof(h_record::H_DATA));
#endif

}

void tpcc_wl::init_tab_order(uint64_t did, uint64_t wid) {
	uint64_t perm[g_cust_per_dist];
	init_permutation(perm, wid); /* initialize permutation of customer numbers */
	
	thread_workload_size += g_cust_per_dist * sizeof(o_record);
	for (UInt64 oid = 1; oid <= g_cust_per_dist; oid++) {
		row_t* row;
		uint64_t row_id;
		t_order->get_new_row(row, 0, row_id, sizeof(o_record));
		row->set_primary_key(oid);
		uint64_t o_ol_cnt = 1;
		uint64_t cid = perm[oid - 1]; //get_permutation();
		row->set_value(O_ID, oid, offsetof(o_record, O_ID), sizeof(o_record::O_ID));
		row->set_value(O_C_ID, cid, offsetof(o_record, O_C_ID), sizeof(o_record::O_C_ID));
		row->set_value(O_D_ID, did, offsetof(o_record, O_D_ID), sizeof(o_record::O_D_ID));
		row->set_value(O_W_ID, wid, offsetof(o_record, O_W_ID), sizeof(o_record::O_W_ID));
		uint64_t o_entry = 2013;
		row->set_value(O_ENTRY_D, o_entry, offsetof(o_record, O_ENTRY_D), sizeof(o_record::O_ENTRY_D));
		if (oid < 2101)
			row->set_value(O_CARRIER_ID, URand(1, 10, wid - 1), offsetof(o_record, O_CARRIER_ID), sizeof(o_record::O_CARRIER_ID));
		else
			row->set_value(O_CARRIER_ID, uint64_t(0), offsetof(o_record, O_CARRIER_ID), sizeof(o_record::O_CARRIER_ID));
		o_ol_cnt = URand(5, 15, wid - 1);
		row->set_value(O_OL_CNT, o_ol_cnt, offsetof(o_record, O_OL_CNT), sizeof(o_record::O_OL_CNT));
		row->set_value(O_ALL_LOCAL, uint64_t(1), offsetof(o_record, O_ALL_LOCAL), sizeof(o_record::O_ALL_LOCAL));

#if TPCC_ALL_TXN
#ifdef DEBUG
		itemid_t *item = NULL;
		idx_key_t key = orderPrimaryKey(wid, did, oid);
		i_order->index_read(key, item, d_to_part(wid, did), wid - 1);
		assert(item == NULL);
#endif  // DEBUG


		index_insert(i_order, orderPrimaryKey(wid, did, oid), row, d_to_part(wid, did));
		index_insert(i_order_cust, orderCustKey(did, cid), row, wh_to_part(wid), false);

#ifdef DEBUG
		item = NULL;
		key = orderPrimaryKey(wid, 1, 3002);
		i_order->index_read(key, item, d_to_part(wid, 1), wid - 1);
		assert(item == NULL);
#endif  // DEBUG
#endif  // TPCC_ALL_TXN

		// ORDER-LINE	
#if !TPCC_SMALL
		thread_workload_size += o_ol_cnt * sizeof(ol_record);
		for (uint64_t ol = 1; ol <= o_ol_cnt; ol++) {
			t_orderline->get_new_row(row, 0, row_id, sizeof(ol_record));
			row->set_value(OL_O_ID, oid, offsetof(ol_record, OL_O_ID), sizeof(ol_record::OL_O_ID));
			row->set_value(OL_D_ID, did, offsetof(ol_record, OL_D_ID), sizeof(ol_record::OL_D_ID));
			row->set_value(OL_W_ID, wid, offsetof(ol_record, OL_W_ID), sizeof(ol_record::OL_W_ID));
			row->set_value(OL_NUMBER, ol, offsetof(ol_record, OL_NUMBER), sizeof(ol_record::OL_NUMBER));
			row->set_value(OL_I_ID, URand(1, 100000, wid - 1), offsetof(ol_record, OL_I_ID), sizeof(ol_record::OL_I_ID));
			row->set_value(OL_SUPPLY_W_ID, wid, offsetof(ol_record, OL_SUPPLY_W_ID), sizeof(ol_record::OL_SUPPLY_W_ID));
			if (oid < 2101) {
				row->set_value(OL_DELIVERY_D, o_entry, offsetof(ol_record, OL_DELIVERY_D), sizeof(ol_record::OL_DELIVERY_D));
				row->set_value(OL_AMOUNT, uint64_t(0), offsetof(ol_record, OL_AMOUNT), sizeof(ol_record::OL_AMOUNT));
			}
			else {
				row->set_value(OL_DELIVERY_D, uint64_t(0), offsetof(ol_record, OL_DELIVERY_D), sizeof(ol_record::OL_DELIVERY_D));
				row->set_value(OL_AMOUNT, (double)URand(1, 999999, wid - 1) / 100, offsetof(ol_record, OL_AMOUNT), sizeof(ol_record::OL_AMOUNT));
			}
			row->set_value(OL_QUANTITY, uint64_t(5), offsetof(ol_record, OL_QUANTITY), sizeof(ol_record::OL_QUANTITY));
			char ol_dist_info[24];
			MakeAlphaString(24, 24, ol_dist_info, wid - 1);
			row->set_value(OL_DIST_INFO, ol_dist_info, offsetof(ol_record, OL_DIST_INFO), sizeof(ol_record::OL_DIST_INFO));

#if TPCC_ALL_TXN
			index_insert(i_orderline, orderlineKey(wid, did, oid), row, d_to_part(wid, did), false);
#endif  // TPCC_ALL_TXN
		}
#endif
		// NEW ORDER
		if (oid > 2100) {
			thread_workload_size += sizeof(no_record);

			t_neworder->get_new_row(row, 0, row_id, sizeof(no_record));
			row->set_value(NO_O_ID, oid, offsetof(no_record, NO_O_ID), sizeof(no_record::NO_O_ID));
			row->set_value(NO_D_ID, did, offsetof(no_record, NO_D_ID), sizeof(no_record::NO_D_ID));
			row->set_value(NO_W_ID, wid, offsetof(no_record, NO_W_ID), sizeof(no_record::NO_W_ID));

#if TPCC_ALL_TXN
			index_insert(i_neworder, distKey(did, wid), row, wh_to_part(wid), false);
#ifndef NDEBUG
			itemid_t item;
			i_neworder->index_read_safe(distKey(did, wid), item, wh_to_part(wid), 0, false);
			assert(item.location == row);
#endif  // !NDEBUG
#endif  // TPCC_ALL_TXN
		}
	}

#if TPCC_ALL_TXN
#ifdef DEBUG
	uint64_t key = orderCustKey(1, 1);
	INDEX * index = i_order_cust;
	itemid_t * item = NULL;
	index->index_read(key, item, wh_to_part(wid), wid - 1);
	assert(item != NULL);

	index = i_order;
	item = NULL;
	key = orderPrimaryKey(wid, 1, 3002);
	index->index_read(key, item, d_to_part(wid, 1), wid - 1);
	static int i = 0;
	assert(item == NULL);
#endif  // DEBUG
#endif  // TPCC_ALL_TXN
}

/*==================================================================+
| ROUTINE NAME
| InitPermutation
+==================================================================*/

void
tpcc_wl::init_permutation(uint64_t* perm_c_id, uint64_t wid) {
	uint64_t i;
	// Init with consecutive values
	for (i = 0; i < g_cust_per_dist; i++)
		perm_c_id[i] = i + 1;

	// shuffle
	for (i = 0; i < g_cust_per_dist - 1; i++) {
		uint64_t j = URand(i + 1, g_cust_per_dist - 1, wid - 1);
		uint64_t tmp = perm_c_id[i];
		perm_c_id[i] = perm_c_id[j];
		perm_c_id[j] = tmp;
	}
}


/*==================================================================+
| ROUTINE NAME
| GetPermutation
+==================================================================*/

void* tpcc_wl::threadInitWarehouse(void* This, int tid) {
	tpcc_wl* wl = (tpcc_wl*)This;
	//int tid = ATOM_FETCH_ADD(wl->next_tid, 1);
	uint32_t wid = tid + 1;
	assert((uint64_t)tid < g_num_wh);

	thread_workload_size = 0;

#if !REPLICATE_ITEM
	if (tid == 0)
		wl->init_tab_item();
#endif  // REPLICATE_ITEM
	wl->init_tab_wh(wid);
	wl->init_tab_dist(wid);
	wl->init_tab_stock(wid);
	for (uint64_t did = 1; did <= DIST_PER_WARE; did++) {
		wl->init_tab_cust(did, wid);
		wl->init_tab_order(did, wid);
		for (uint64_t cid = 1; cid <= g_cust_per_dist; cid++)
			wl->init_tab_hist(cid, did, wid);
	}

#pragma omp critical
	{
		std::cout << "Size of warehouse " << wid << ": " << thread_workload_size << "\n";
	}

	wl->workload_size += thread_workload_size;

	return NULL;
}
