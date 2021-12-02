#include <algorithm>

#include "tpcc.h"
#include "tpcc_query.h"
#include "tpcc_helper.h"
#include "query.h"
#include "wl.h"
#include "thread.h"
#include "table.h"
#include "row.h"
#include "index_hash.h"
#include "index_btree.h"
#include "tpcc_const.h"

void tpcc_txn_man::init(thread_t * h_thd, workload * h_wl, uint64_t thd_id) {
#if REPLICATE_INTERNALS
	tpcc_wl* expected_wl = nullptr;
	tpcc_wl* replicated_wl = (tpcc_wl*)_mm_malloc(sizeof(tpcc_wl), 64);
	new(replicated_wl) tpcc_wl(*(tpcc_wl*)h_wl);
	bool success = _wl_replicas.at(get_node()).compare_exchange_strong(expected_wl, replicated_wl);
	if (success) {
		h_wl = (workload*) replicated_wl;
	}
	else {
		free(replicated_wl);
		h_wl = (workload*) expected_wl;
	}
#endif  // REPLICATE_INTERNALS

	txn_man::init(h_thd, h_wl, thd_id);
	_wl = (tpcc_wl *) h_wl;
#if defined(REPLICATE_ITEM) && REPLICATE_ITEM
	_node = get_node();
#endif
}

RC tpcc_txn_man::run_txn(base_query * query) {

	tpcc_query * m_query = (tpcc_query *) query;

	switch (m_query->type) {
		case TPCC_PAYMENT :
			return run_payment(m_query); break;
		case TPCC_NEW_ORDER :
#if TPCC_ALL_TXN
			{
				RC rc = run_new_order(m_query);
				// Count commited neworder
				INC_STATS(get_thd_id(), neworder_cnt, (rc == RCOK));
#ifdef DEBUG
// Debug
	
	uint64_t wid = m_query->w_id;
	uint64_t key = orderCustKey(1, 1);
	INDEX * index = _wl->i_order_cust;
	itemid_t item;
	RC indexRC = index->index_read_safe(key, item, wh_to_part(wid), wid - 1);
	assert(indexRC == RCOK);

	/* index = _wl->i_order;
	item = NULL;
	key = orderPrimaryKey(wid, 1, 3002);
	index->index_read(key, item, d_to_part(wid, 1), wid - 1);
	static std::atomic_bool first = true;
	if(!first.exchange(false)) {
		assert(item != NULL);
	}
	else {
		assert(item == NULL);
	} */
#endif  // DEBUG
				return rc; break;
			}
#else
			return run_new_order(m_query); break;
#endif  // TPCC_ALL_TXN
#if TPCC_ALL_TXN
		case TPCC_STOCK_LEVEL :
			return run_stock_level(m_query); break;
		case TPCC_ORDER_STATUS :
			return run_order_status(m_query); break;
		case TPCC_DELIVERY : {
			return run_delivery(m_query); break;
		}
#endif  // TPCC_ALL_TXN
		default:
			std::abort();
	}
}

RC tpcc_txn_man::run_payment(tpcc_query * query) {
	RC rc = RCOK;
	uint64_t key;
	itemid_t * item;
	
	uint64_t w_id = query->w_id;
    uint64_t d_id = query->d_id;
    uint64_t c_w_id = query->c_w_id;
    uint64_t c_d_id = query->c_d_id;
    uint64_t c_id = query->c_id;
    double h_amount = query->h_amount;
	/*====================================================+
    	EXEC SQL UPDATE warehouse SET w_ytd = w_ytd + :h_amount
		WHERE w_id=:w_id;
	+====================================================*/
	/*===================================================================+
		EXEC SQL SELECT w_street_1, w_street_2, w_city, w_state, w_zip, w_name
		INTO :w_street_1, :w_street_2, :w_city, :w_state, :w_zip, :w_name
		FROM warehouse
		WHERE w_id=:w_id;
	+===================================================================*/

	// TODO for variable length variable (string). Should store the size of 
	// the variable.
	key = query->w_id;
	INDEX * index = _wl->i_warehouse; 
	item = index_read(index, key, wh_to_part(w_id));
	assert(item != NULL);
	row_t * r_wh = ((row_t *)item->location);
	row_t * r_wh_local;
uint64_t t5 = get_sys_clock();
	if (g_wh_update)
		r_wh_local = get_row(r_wh, WR, sizeof(w_record));
	else 
		r_wh_local = get_row(r_wh, RD, sizeof(w_record));
uint64_t tt5 = get_sys_clock() - t5;
INC_STATS(get_thd_id(), debug5, tt5);

	if (r_wh_local == NULL) {
		return finish(Abort);
	}
	double w_ytd;
uint64_t t1 = get_sys_clock();
	r_wh_local->get_value(W_YTD, w_ytd, offsetof(w_record, W_YTD));
	if (g_wh_update) {
		r_wh_local->set_value(W_YTD, w_ytd + query->h_amount, offsetof(w_record, W_YTD), sizeof(w_record::W_YTD));
	}
	char w_name[11];
	char * tmp_str;
	r_wh_local->get_value(W_NAME, tmp_str, offsetof(w_record, W_NAME));
	memcpy(w_name, tmp_str, 10);
	w_name[10] = '\0';
uint64_t tt1 = get_sys_clock() - t1;
INC_STATS(get_thd_id(), debug1, tt1);

	/*=====================================================+
		EXEC SQL UPDATE district SET d_ytd = d_ytd + :h_amount
		WHERE d_w_id=:w_id AND d_id=:d_id;
	+=====================================================*/
uint64_t t3 = get_sys_clock();
	key = distKey(query->d_id, query->d_w_id);
	item = index_read(_wl->i_district, key, wh_to_part(w_id));
	assert(item != NULL);
	row_t * r_dist = ((row_t *)item->location);
	row_t * r_dist_local = get_row(r_dist, WR, sizeof(d_record));
	if (r_dist_local == NULL) {
		return finish(Abort);
	}
uint64_t tt3 = get_sys_clock() - t3;
INC_STATS(get_thd_id(), debug3, tt3);

	double d_ytd;
	r_dist_local->get_value(D_YTD, d_ytd, offsetof(d_record, D_YTD));
	r_dist_local->set_value(D_YTD, d_ytd + query->h_amount, offsetof(d_record, D_YTD), sizeof(d_record::D_YTD));
	char d_name[11];
	r_dist_local->get_value(D_NAME, tmp_str, offsetof(d_record, D_NAME));
	memcpy(d_name, tmp_str, 10);
	d_name[10] = '\0';

	/*====================================================================+
		EXEC SQL SELECT d_street_1, d_street_2, d_city, d_state, d_zip, d_name
		INTO :d_street_1, :d_street_2, :d_city, :d_state, :d_zip, :d_name
		FROM district
		WHERE d_w_id=:w_id AND d_id=:d_id;
	+====================================================================*/

	row_t * r_cust;
	if (query->by_last_name) { 
		/*==========================================================+
			EXEC SQL SELECT count(c_id) INTO :namecnt
			FROM customer
			WHERE c_last=:c_last AND c_d_id=:c_d_id AND c_w_id=:c_w_id;
		+==========================================================*/
		/*==========================================================================+
			EXEC SQL DECLARE c_byname CURSOR FOR
			SELECT c_first, c_middle, c_id, c_street_1, c_street_2, c_city, c_state,
			c_zip, c_phone, c_credit, c_credit_lim, c_discount, c_balance, c_since
			FROM customer
			WHERE c_w_id=:c_w_id AND c_d_id=:c_d_id AND c_last=:c_last
			ORDER BY c_first;
			EXEC SQL OPEN c_byname;
		+===========================================================================*/

		uint64_t key = custNPKey(query->c_last, query->c_d_id, query->c_w_id);
		// XXX: the list is not sorted. But let's assume it's sorted... 
		// The performance won't be much different.
#if INDEX_STRUCT == IDX_HASH_PERFECT
		IndexHash* index = reinterpret_cast<IndexHash*>(_wl->i_customer_last);
#else
		INDEX * index = _wl->i_customer_last;
#endif  // IDX_HASH_PERFECT
		item = index_read(index, key, wh_to_part(c_w_id));
		assert(item != NULL);
		
		int cnt = 0;
		itemid_t * it = item;
		itemid_t * mid = item;
		while (it != NULL) {
			cnt ++;
			it = it->next;
			if (cnt % 2 == 0)
				mid = mid->next;
		}
		r_cust = ((row_t *)mid->location);
		
		/*============================================================================+
			for (n=0; n<namecnt/2; n++) {
				EXEC SQL FETCH c_byname
				INTO :c_first, :c_middle, :c_id,
					 :c_street_1, :c_street_2, :c_city, :c_state, :c_zip,
					 :c_phone, :c_credit, :c_credit_lim, :c_discount, :c_balance, :c_since;
			}
			EXEC SQL CLOSE c_byname;
		+=============================================================================*/
		// XXX: we don't retrieve all the info, just the tuple we are interested in
	}
	else { // search customers by cust_id
		/*=====================================================================+
			EXEC SQL SELECT c_first, c_middle, c_last, c_street_1, c_street_2,
			c_city, c_state, c_zip, c_phone, c_credit, c_credit_lim,
			c_discount, c_balance, c_since
			INTO :c_first, :c_middle, :c_last, :c_street_1, :c_street_2,
			:c_city, :c_state, :c_zip, :c_phone, :c_credit, :c_credit_lim,
			:c_discount, :c_balance, :c_since
			FROM customer
			WHERE c_w_id=:c_w_id AND c_d_id=:c_d_id AND c_id=:c_id;
		+======================================================================*/
		key = custKey(query->c_id, query->c_d_id, query->c_w_id);
		INDEX * index = _wl->i_customer_id;
		item = index_read(index, key, wh_to_part(c_w_id));
		assert(item != NULL);
		r_cust = (row_t *) item->location;
	}

  	/*======================================================================+
	   	EXEC SQL UPDATE customer SET c_balance = :c_balance, c_data = :c_new_data
   		WHERE c_w_id = :c_w_id AND c_d_id = :c_d_id AND c_id = :c_id;
   	+======================================================================*/
	row_t * r_cust_local = get_row(r_cust, WR, sizeof(c_record));
	if (r_cust_local == NULL) {
		return finish(Abort);
	}
	double c_balance;
	double c_ytd_payment;
	double c_payment_cnt;

	r_cust_local->get_value(C_BALANCE, c_balance, offsetof(c_record, C_BALANCE));
	r_cust_local->set_value(C_BALANCE, c_balance - query->h_amount, offsetof(c_record, C_BALANCE), sizeof(c_record::C_BALANCE));
	r_cust_local->get_value(C_YTD_PAYMENT, c_ytd_payment, offsetof(c_record, C_YTD_PAYMENT));
	r_cust_local->set_value(C_YTD_PAYMENT, c_ytd_payment + query->h_amount, offsetof(c_record, C_YTD_PAYMENT), sizeof(c_record::C_YTD_PAYMENT));
	r_cust_local->get_value(C_PAYMENT_CNT, c_payment_cnt, offsetof(c_record, C_PAYMENT_CNT));
	r_cust_local->set_value(C_PAYMENT_CNT, c_payment_cnt + 1, offsetof(c_record, C_PAYMENT_CNT), sizeof(c_record::C_PAYMENT_CNT));

	char* c_credit;
	r_cust_local->get_value(C_CREDIT, c_credit, offsetof(c_record, C_CREDIT));

	if ( strstr(c_credit, "BC") ) {
	
		/*=====================================================+
		    EXEC SQL SELECT c_data
			INTO :c_data
			FROM customer
			WHERE c_w_id=:c_w_id AND c_d_id=:c_d_id AND c_id=:c_id;
		+=====================================================*/
//	  	char c_new_data[501];
//	  	sprintf(c_new_data,"| %4d %2d %4d %2d %4d $%7.2f",
//	      	c_id, c_d_id, c_w_id, d_id, w_id, query->h_amount);
//		char * c_data = r_cust->get_value("C_DATA");
//	  	strncat(c_new_data, c_data, 500 - strlen(c_new_data));
//		r_cust->set_value("C_DATA", c_new_data);
			
	}
	
	char h_data[25];
	char * h_w_name;
	r_wh_local->get_value(W_NAME, h_w_name, offsetof(w_record, W_NAME));
	char * h_d_name;
	r_dist_local->get_value(D_NAME,  h_d_name, offsetof(d_record, D_NAME));
	/*=============================================================================+
	  EXEC SQL INSERT INTO
	  history (h_c_d_id, h_c_w_id, h_c_id, h_d_id, h_w_id, h_date, h_amount, h_data)
	  VALUES (:c_d_id, :c_w_id, :c_id, :d_id, :w_id, :datetime, :h_amount, :h_data);
	  +=============================================================================*/
	row_t * r_hist;
	uint64_t row_id;
	_wl->t_history->get_new_row(r_hist, 0, row_id, sizeof(h_record));
	int64_t date = 2013;		
#if BULK_INSERT
	h_record h_record;
	h_record.H_C_ID = c_id;
	h_record.H_C_D_ID = c_d_id;
	h_record.H_C_W_ID = c_w_id;
	h_record.H_D_ID = d_id;
	h_record.H_W_ID = w_id;
	h_record.H_DATE = date;
	h_record.H_AMOUNT = h_amount;

#if !TPCC_SMALL
	strncpy(h_record.H_DATA, h_w_name, 10);
	int length = strlen(h_data);
	if (length > 10) length = 10;
	strcpy(&h_record.H_DATA[length], "    ");
	strncpy(&h_record.H_DATA[length + 4], h_d_name, 10);
	h_record.H_DATA[length+13] = '\0';
#else
	strncpy(h_data, h_w_name, 10);
	int length = strlen(h_data);
	if (length > 10) length = 10;
	strcpy(&h_data[length], "    ");
	strncpy(&h_data[length + 4], h_d_name, 10);
	h_data[length+14] = '\0';
#endif

	r_hist->set_data(std::move(h_record));
#else
	strncpy(h_data, h_w_name, 10);
	int length = strlen(h_data);
	if (length > 10) length = 10;
	strcpy(&h_data[length], "    ");
	strncpy(&h_data[length + 4], h_d_name, 10);
	h_data[length+14] = '\0';
	r_hist->set_value(H_C_ID, c_id, offsetof(h_record, H_C_ID), sizeof(h_record::H_C_ID));
	r_hist->set_value(H_C_D_ID, c_d_id, offsetof(h_record, H_C_D_ID), sizeof(h_record::H_C_D_ID));
	r_hist->set_value(H_C_W_ID, c_w_id, offsetof(h_record, H_C_W_ID ), sizeof(h_record::H_C_W_ID));
	r_hist->set_value(H_D_ID, d_id, offsetof(h_record, H_D_ID), sizeof(h_record::H_D_ID));
	r_hist->set_value(H_W_ID, w_id, offsetof(h_record, H_W_ID), sizeof(h_record::H_W_ID));
	r_hist->set_value(H_DATE, date, offsetof(h_record, H_DATE), sizeof(h_record::H_DATE));
	r_hist->set_value(H_AMOUNT, h_amount, offsetof(h_record, H_AMOUNT), sizeof(h_record::H_AMOUNT));
#if !TPCC_SMALL
	r_hist->set_value(H_DATA, h_data, offsetof(h_record, H_DATA), sizeof(h_record::H_DATA));
#endif
#endif  // BULK_INSERT
	insert_row(r_hist, _wl->t_history);

	assert( rc == RCOK );
	return finish(rc);
}

RC tpcc_txn_man::run_new_order(tpcc_query * query) {
	RC rc = RCOK;
	uint64_t key;
	itemid_t * item;
	INDEX * index;
	
	bool remote = query->remote;
	const uint64_t w_id = query->w_id;
    const uint64_t d_id = query->d_id;
    const uint64_t c_id = query->c_id;
	const uint64_t ol_cnt = query->ol_cnt;
	const uint64_t o_entry_d = query->o_entry_d;

	/*=======================================================================+
	EXEC SQL SELECT c_discount, c_last, c_credit, w_tax
		INTO :c_discount, :c_last, :c_credit, :w_tax
		FROM customer, warehouse
		WHERE w_id = :w_id AND c_w_id = w_id AND c_d_id = :d_id AND c_id = :c_id;
	+========================================================================*/
	key = w_id;
	index = _wl->i_warehouse; 
	item = index_read(index, key, wh_to_part(w_id));
	assert(item != NULL);
	row_t * r_wh = ((row_t *)item->location);
	row_t * r_wh_local = get_row(r_wh, RD_RO, sizeof(w_record));
	if (r_wh_local == NULL) {
		return finish(Abort);
	}
	double w_tax;
	r_wh_local->get_value(W_TAX, w_tax, offsetof(w_record, W_TAX)); 
	key = custKey(c_id, d_id, w_id);
	index = _wl->i_customer_id;
	item = index_read(index, key, wh_to_part(w_id));
	assert(item != NULL);
	row_t * r_cust = (row_t *) item->location;
	row_t * r_cust_local = get_row(r_cust, RD_RO, sizeof(c_record));
	if (r_cust_local == NULL) {
		return finish(Abort); 
	}
	uint64_t c_discount;
	//char * c_last;
	//char * c_credit;
	r_cust_local->get_value(C_DISCOUNT, c_discount, offsetof(c_record, C_DISCOUNT));
	//c_last = r_cust_local->get_value(C_LAST);
	//c_credit = r_cust_local->get_value(C_CREDIT);
 	
	/*==================================================+
	EXEC SQL SELECT d_next_o_id, d_tax
		INTO :d_next_o_id, :d_tax
		FROM district WHERE d_id = :d_id AND d_w_id = :w_id;
	EXEC SQL UPDATE d istrict SET d _next_o_id = :d _next_o_id + 1
		WH ERE d _id = :d_id AN D d _w _id = :w _id ;
	+===================================================*/
	key = distKey(d_id, w_id);
	item = index_read(_wl->i_district, key, wh_to_part(w_id));
	assert(item != NULL);
	row_t * r_dist = ((row_t *)item->location);
	row_t * r_dist_local = get_row(r_dist, WR, sizeof(d_record));
	if (r_dist_local == NULL) {
		return finish(Abort);
	}
	//double d_tax;
	int64_t o_id;
	//d_tax = *(double *) r_dist_local->get_value(D_TAX);
	r_dist_local->get_value(D_NEXT_O_ID, o_id, offsetof(d_record, D_NEXT_O_ID));
	int64_t next_o_id = o_id + 1;
	r_dist_local->set_value(D_NEXT_O_ID, next_o_id, offsetof(d_record, D_NEXT_O_ID), sizeof(d_record::D_NEXT_O_ID));

	/*========================================================================================+
	EXEC SQL INSERT INTO ORDERS (o_id, o_d_id, o_w_id, o_c_id, o_entry_d, o_ol_cnt, o_all_local)
		VALUES (:o_id, :d_id, :w_id, :c_id, :datetime, :o_ol_cnt, :o_all_local);
	+========================================================================================*/
	row_t * r_order;
	uint64_t row_id;
	_wl->t_order->get_new_row(r_order, 0, row_id, sizeof(o_record));
	int64_t all_local = (remote? 0 : 1);
#if BULK_INSERT
	o_record o_record;
	o_record.O_ID = o_id;
	o_record.O_C_ID = c_id;
	o_record.O_D_ID = d_id;
	o_record.O_W_ID = w_id;
	o_record.O_ENTRY_D = o_entry_d;
	o_record.O_OL_CNT = ol_cnt;
	o_record.O_ALL_LOCAL = all_local;
	r_order->set_data(std::move(o_record));
#else
	r_order->set_value(O_ID, o_id, offsetof(o_record, O_ID), sizeof(o_record::O_ID));
	r_order->set_value(O_C_ID, c_id, offsetof(o_record, O_C_ID), sizeof(o_record::O_C_ID));
	r_order->set_value(O_D_ID, d_id, offsetof(o_record, O_D_ID), sizeof(o_record::O_D_ID));
	r_order->set_value(O_W_ID, w_id, offsetof(o_record, O_W_ID), sizeof(o_record::O_W_ID));
	r_order->set_value(O_ENTRY_D, o_entry_d, offsetof(o_record, O_ENTRY_D), sizeof(o_record::O_ENTRY_D));
	r_order->set_value(O_OL_CNT, ol_cnt, offsetof(o_record, O_OL_CNT), sizeof(o_record::O_OL_CNT));
	r_order->set_value(O_ALL_LOCAL, all_local, offsetof(o_record, O_ALL_LOCAL), sizeof(o_record::O_ALL_LOCAL));
#endif  // BULK_INSERT
	insert_row(r_order, _wl->t_order);
#if TPCC_ALL_TXN
	idx_key_t o_key = orderPrimaryKey(w_id, d_id, o_id);
	idx_key_t cust_key = orderCustKey(d_id, c_id);

#ifdef DEBUG
	if (g_thread_cnt == 1)
	{
		itemid_t * debugItem  = index_read(_wl->i_order, o_key, d_to_part(w_id, d_id));
		if (debugItem) {
			uint64_t debugOID = 0;
			((row_t*) debugItem->location)->get_value(O_ID, debugOID, offsetof(struct o_record, O_ID));
			assert(debugOID == o_id);
		}
		assert(debugItem == NULL);
	}
#endif  // DEBUG

	insert_index(_wl->i_order, o_key, r_order, d_to_part(w_id, d_id), false);

	insert_index(_wl->i_order_cust, cust_key, r_order, wh_to_part(w_id), false);

#endif  // TPCC_ALL_TXN

	/*=======================================================+
    EXEC SQL INSERT INTO NEW_ORDER (no_o_id, no_d_id, no_w_id)
        VALUES (:o_id, :d_id, :w_id);
    +=======================================================*/
	row_t * r_no;
	_wl->t_neworder->get_new_row(r_no, 0, row_id, sizeof(no_record));
#if BULK_INSERT
	no_record no_record;
	no_record.NO_O_ID = o_id;
	no_record.NO_D_ID = d_id;
	no_record.NO_W_ID = w_id;
	r_no->set_data(std::move(no_record));
#else
	r_no->set_value(NO_O_ID, o_id, offsetof(no_record, NO_O_ID), sizeof(no_record::NO_O_ID));
	r_no->set_value(NO_D_ID, d_id, offsetof(no_record, NO_D_ID), sizeof(no_record::NO_D_ID));
	r_no->set_value(NO_W_ID, w_id, offsetof(no_record, NO_W_ID), sizeof(no_record::NO_W_ID));
#endif  // BULK_INSERT
	insert_row(r_no, _wl->t_neworder);
#if TPCC_ALL_TXN
	idx_key_t no_key = distKey(d_id, w_id);
	insert_index(_wl->i_neworder, no_key, r_no, wh_to_part(w_id), false);
#endif  // TPCC_ALL_TXN

#if BATCHED_ACCESS
	itemid_t* item_item_p[15];
	itemid_t* stock_item_p[15];
	row_t* item_row_p[15];
	row_t* stock_row_p[15];

	// Batch read stock index
	for (UInt32 ol_number = 0; ol_number < ol_cnt; ol_number++) {
		uint64_t ol_i_id = query->items[ol_number].ol_i_id;
		uint64_t ol_supply_w_id = query->items[ol_number].ol_supply_w_id;

		uint64_t stock_key = stockKey(ol_i_id, ol_supply_w_id);
		INDEX * stock_index = _wl->i_stock;
		index_read(stock_index, stock_key, wh_to_part(ol_supply_w_id), stock_item_p[ol_number]);
		assert(stock_item_p[ol_number] != NULL);

		//_mm_prefetch(stock_item_p[ol_number], _MM_HINT_NTA);
		__builtin_prefetch(stock_item_p[ol_number], 0, 0);

	}

	// Batch read item index
	for (UInt32 ol_number = 0; ol_number < ol_cnt; ol_number++) {
		uint64_t ol_i_id = query->items[ol_number].ol_i_id;

		uint64_t key = ol_i_id;
		item_item_p[ol_number] = index_read(_wl->i_item, key, getItemPart());
		assert(item_item_p[ol_number] != NULL);

		//_mm_prefetch(item_item_p[ol_number], _MM_HINT_NTA);
		__builtin_prefetch(item_item_p[ol_number], 0 , 0);
	}

	// Batch prefetch stock records
	for (UInt32 ol_number = 0; ol_number < ol_cnt; ol_number++) {
		stock_row_p[ol_number] = ((row_t *)stock_item_p[ol_number]->location);
		
		if (stock_row_p[ol_number] == NULL) {
			return finish(Abort);
		}

		//_mm_prefetch(stock_row_p[ol_number], _MM_HINT_NTA);
		__builtin_prefetch(stock_row_p[ol_number], 0 , 0);
	}

	// Batch prefetch item records
	for (UInt32 ol_number = 0; ol_number < ol_cnt; ol_number++) {
		item_row_p[ol_number] = ((row_t *)item_item_p[ol_number]->location);
		//_mm_prefetch(item_row_p[ol_number], _MM_HINT_NTA);
		__builtin_prefetch(item_row_p[ol_number], 0 , 0);
	}

	// Batch prefetch data of stock records
	for (UInt32 ol_number = 0; ol_number < ol_cnt; ol_number++) {

		char* ptr = reinterpret_cast<char*>(stock_row_p[ol_number]->data);

		for(uint64_t i = 0; i < (sizeof(s_record)/64); i+= 64)
		{
			//_mm_prefetch(ptr+i, _MM_HINT_NTA);
			__builtin_prefetch(ptr+i, 0 , 0);
		}
	}

	// Batch get item records and prefetch their data
	for (UInt32 ol_number = 0; ol_number < ol_cnt; ol_number++) {
		item_row_p[ol_number] = get_row(item_row_p[ol_number], RD_RO, sizeof(i_record));
		if (item_row_p[ol_number] == NULL) {
			return finish(Abort);
		}
		//_mm_prefetch(item_row_p[ol_number]->data, _MM_HINT_NTA);
		__builtin_prefetch(item_row_p[ol_number]->data, 0 , 0);
	}

	// Batch get stock records
	for (UInt32 ol_number = 0; ol_number < ol_cnt; ol_number++) {

		stock_row_p[ol_number] = get_row(stock_row_p[ol_number], WR, sizeof(s_record));
		if (stock_row_p[ol_number] == NULL) {
			return finish(Abort);
		}
	}

#endif  // BATCHED_ACCESS

#if BULK_INSERT
	row_t *ol_rows;
	_wl->t_orderline->get_new_row(ol_rows, 0, row_id, sizeof(ol_record), ol_cnt);
#endif  // BULK_INSERT
	idx_key_t ol_key = orderlineKey(query->w_id, d_id, o_id);
	for (UInt32 ol_number = 0; ol_number < ol_cnt; ol_number++) {

		uint64_t ol_i_id = query->items[ol_number].ol_i_id;
		uint64_t ol_supply_w_id = query->items[ol_number].ol_supply_w_id;
		uint64_t ol_quantity = query->items[ol_number].ol_quantity;
		/*===========================================+
		EXEC SQL SELECT i_price, i_name , i_data
			INTO :i_price, :i_name, :i_data
			FROM item
			WHERE i_id = :ol_i_id;
		+===========================================*/
#if BATCHED_ACCESS
		auto&& r_item_local = item_row_p[ol_number];
#else
		key = ol_i_id;
		item = index_read(_wl->i_item, key, getItemPart());
		assert(item != NULL);
		row_t * r_item = ((row_t *)item->location);

		row_t * r_item_local = get_row(r_item, RD_RO, sizeof(i_record));
		if (r_item_local == NULL) {
			return finish(Abort);
		}
#endif  // BATCHED_ACCESS
		int64_t i_price;
		//char * i_name;
		//char * i_data;
		r_item_local->get_value(I_PRICE, i_price, offsetof(i_record, I_PRICE));
		//i_name = r_item_local->get_value(I_NAME);
		//i_data = r_item_local->get_value(I_DATA);

		/*===================================================================+
		EXEC SQL SELECT s_quantity, s_data,
				s_dist_01, s_dist_02, s_dist_03, s_dist_04, s_dist_05,
				s_dist_06, s_dist_07, s_dist_08, s_dist_09, s_dist_10
			INTO :s_quantity, :s_data,
				:s_dist_01, :s_dist_02, :s_dist_03, :s_dist_04, :s_dist_05,
				:s_dist_06, :s_dist_07, :s_dist_08, :s_dist_09, :s_dist_10
			FROM stock
			WHERE s_i_id = :ol_i_id AND s_w_id = :ol_supply_w_id;
		EXEC SQL UPDATE stock SET s_quantity = :s_quantity
			WHERE s_i_id = :ol_i_id
			AND s_w_id = :ol_supply_w_id;
		+===============================================*/

#if BATCHED_ACCESS
		auto&& r_stock_local = stock_row_p[ol_number];
#else
		uint64_t stock_key = stockKey(ol_i_id, ol_supply_w_id);
		INDEX * stock_index = _wl->i_stock;
		itemid_t * stock_item;
		index_read(stock_index, stock_key, wh_to_part(ol_supply_w_id), stock_item);
		assert(item != NULL);
		row_t * r_stock = ((row_t *)stock_item->location);
		row_t * r_stock_local = get_row(r_stock, WR, sizeof(s_record));
		if (r_stock_local == NULL) {
			return finish(Abort);
		}
#endif  // BATCHED_ACCESS
		
		// XXX s_dist_xx are not retrieved.
		UInt64 s_quantity;
		int64_t s_remote_cnt;
		r_stock_local->get_value(S_QUANTITY, s_quantity, offsetof(s_record, S_QUANTITY));
#if !TPCC_SMALL
		int64_t s_ytd;
		int64_t s_order_cnt;
		char * s_data;
		r_stock_local->get_value(S_YTD, s_ytd, offsetof(s_record, S_YTD));
		r_stock_local->set_value(S_YTD, s_ytd + ol_quantity, offsetof(s_record, S_YTD), sizeof(s_record::S_YTD));
		r_stock_local->get_value(S_ORDER_CNT, s_order_cnt, offsetof(s_record, S_ORDER_CNT));
		r_stock_local->set_value(S_ORDER_CNT, s_order_cnt + 1, offsetof(s_record, S_ORDER_CNT), sizeof(s_record::S_ORDER_CNT));
		r_stock_local->get_value(S_DATA, s_data, offsetof(s_record, S_DATA));
#endif
		if (remote) {
			r_stock_local->get_value(S_REMOTE_CNT, s_remote_cnt, offsetof(s_record, S_REMOTE_CNT));
			s_remote_cnt ++;
			r_stock_local->set_value(S_REMOTE_CNT, s_remote_cnt, offsetof(s_record, S_REMOTE_CNT), sizeof(s_record::S_REMOTE_CNT));
		}
		uint64_t quantity;
		if (s_quantity > ol_quantity + 10) {
			quantity = s_quantity - ol_quantity;
		} else {
			quantity = s_quantity - ol_quantity + 91;
		}
		r_stock_local->set_value(S_QUANTITY, quantity, offsetof(s_record, S_QUANTITY), sizeof(s_record::S_QUANTITY));

		/*====================================================+
		EXEC SQL INSERT
			INTO order_line(ol_o_id, ol_d_id, ol_w_id, ol_number,
				ol_i_id, ol_supply_w_id,
				ol_quantity, ol_amount, ol_dist_info)
			VALUES(:o_id, :d_id, :w_id, :ol_number,
				:ol_i_id, :ol_supply_w_id,
				:ol_quantity, :ol_amount, :ol_dist_info);
		+====================================================*/
		// XXX district info is not inserted.
		row_t * r_ol;
#if BULK_INSERT
		r_ol = ol_rows + ol_number;

		ol_record record;
		record.OL_O_ID = o_id;
		record.OL_D_ID = d_id;
		record.OL_W_ID = w_id;
		record.OL_NUMBER = ol_number;
		record.OL_I_ID = ol_i_id;
#if !TPCC_SMALL
		int w_tax=1, d_tax=1;
		int64_t ol_amount = ol_quantity * i_price * (1 + w_tax + d_tax) * (1 - c_discount);
		record.OL_SUPPLY_W_ID = ol_supply_w_id;
		record.OL_QUANTITY = ol_quantity;
		record.OL_AMOUNT = ol_amount;
#endif

		r_ol->set_data(std::move(record));

#else
		uint64_t row_id;
		_wl->t_orderline->get_new_row(r_ol, 0, row_id, sizeof(ol_record));
		r_ol->set_value(OL_O_ID, o_id, offsetof(ol_record, OL_O_ID), sizeof(ol_record::OL_O_ID ));
		r_ol->set_value(OL_D_ID, d_id, offsetof(ol_record, OL_D_ID), sizeof(ol_record::OL_D_ID ));
		r_ol->set_value(OL_W_ID, w_id, offsetof(ol_record, OL_W_ID), sizeof(ol_record::OL_W_ID ));
		r_ol->set_value(OL_NUMBER, ol_number, offsetof(ol_record, OL_NUMBER ), sizeof(ol_record::OL_NUMBER ));
		r_ol->set_value(OL_I_ID, ol_i_id, offsetof(ol_record,OL_I_ID ), sizeof(ol_record::OL_I_ID ));
#if !TPCC_SMALL
		int w_tax=1, d_tax=1;
		int64_t ol_amount = ol_quantity * i_price * (1 + w_tax + d_tax) * (1 - c_discount);
		r_ol->set_value(OL_SUPPLY_W_ID, ol_supply_w_id, offsetof(ol_record, OL_SUPPLY_W_ID), sizeof(ol_record::OL_SUPPLY_W_ID));
		r_ol->set_value(OL_QUANTITY, ol_quantity, offsetof(ol_record, OL_QUANTITY), sizeof(ol_record::OL_QUANTITY));
		r_ol->set_value(OL_AMOUNT, ol_amount, offsetof(ol_record, OL_AMOUNT), sizeof(ol_record::OL_QUANTITY));
#endif		
#endif  // BULK_INSERT
		insert_row(r_ol, _wl->t_orderline);
#if TPCC_ALL_TXN
		insert_index(_wl->i_orderline, ol_key, r_ol, d_to_part(w_id, d_id), false);
#endif  // TPCC_ALL_TXN
	}
	assert( rc == RCOK );

	rc = finish(rc);

#ifdef DEBUG
	if (g_thread_cnt == 1)
	{
		idx_key_t o_key = orderPrimaryKey(w_id, d_id, o_id);
		idx_key_t cust_key = orderCustKey(d_id, c_id);

		auto orderItem = index_read(_wl->i_order, o_key, d_to_part(w_id, d_id));
		auto orderCustItem = index_read(_wl->i_order_cust, cust_key, wh_to_part(w_id));
		assert(orderItem != NULL);
		assert(orderCustItem != NULL);
		assert(orderItem->location == orderCustItem->location);
	}
#endif  // DEBUG

	return rc;
}

RC 
tpcc_txn_man::run_order_status(tpcc_query * query) {
	RC rc = RCOK;
#if TPCC_ALL_TXN
	row_t * r_cust_local;
	uint64_t c_id = 0;
	if (query->by_last_name) {
		// EXEC SQL SELECT count(c_id) INTO :namecnt FROM customer
		// 	WHERE c_last=:c_last AND c_d_id=:d_id AND c_w_id=:w_id;
		// EXEC SQL DECLARE c_name CURSOR FOR SELECT c_balance, c_first, c_middle, c_id
		// FROM customer
		// WHERE c_last=:c_last AND c_d_id=:d_id AND c_w_id=:w_id ORDER BY c_first;
		// EXEC SQL OPEN c_name;
		// if (namecnt%2) namecnt++; / / Locate midpoint customer for (n=0; n<namecnt/ 2; n++)
		// {
		//	   	EXEC SQL FETCH c_name
		//	   	INTO :c_balance, :c_first, :c_middle, :c_id;
		// }
		// EXEC SQL CLOSE c_name;

		uint64_t key = custNPKey(query->c_last, query->d_id, query->w_id);
		// XXX: the list is not sorted. But let's assume it's sorted... 
		// The performance won't be much different.
#if INDEX_STRUCT == IDX_HASH_PERFECT
		IndexHash* index = reinterpret_cast<IndexHash*>(_wl->i_customer_last);
#else
		INDEX * index = _wl->i_customer_last;
#endif  // IDX_HASH_PERFECT
		uint64_t thd_id = get_thd_id();
		itemid_t * item = index_read(index, key, wh_to_part(query->w_id));
		int cnt = 0;
		itemid_t * it = item;
		itemid_t * mid = item;
		while (it != NULL) {
			cnt ++;
			it = it->next;
			if (cnt % 2 == 0)
				mid = mid->next;
		}
		assert(mid != NULL);
		row_t *r_cust = ((row_t *)mid->location);

		assert(r_cust != NULL);

		r_cust_local = get_row(r_cust, RD, sizeof(c_record));
		if (r_cust_local == NULL) {
			return finish(Abort);
		}

		r_cust_local->get_value(C_ID, c_id, offsetof(c_record, C_ID));

	} else {
		// EXEC SQL SELECT c_balance, c_first, c_middle, c_last
		// INTO :c_balance, :c_first, :c_middle, :c_last
		// FROM customer
		// WHERE c_id=:c_id AND c_d_id=:d_id AND c_w_id=:w_id;
		uint64_t key = custKey(query->c_id, query->c_d_id, query->c_w_id);
		INDEX * index = _wl->i_customer_id;
		itemid_t * item = index_read(index, key, wh_to_part(query->c_w_id));
		row_t *r_cust = (row_t *) item->location;

		r_cust_local = get_row(r_cust, RD, sizeof(c_record));
		if (r_cust_local == NULL) {
			return finish(Abort);
		}

		c_id = query->c_id;
	}

#if TPCC_ACCESS_ALL
	double c_balance;
	r_cust_local->get_value(C_BALANCE, c_balance);
	char * c_first = r_cust_local->get_value(C_FIRST);
	char * c_middle = r_cust_local->get_value(C_MIDDLE);
	char * c_last = r_cust_local->get_value(C_LAST);
#endif

	// EXEC SQL SELECT o_id, o_carrier_id, o_entry_d
	// INTO :o_id, :o_carrier_id, :entdate FROM orders
	// ORDER BY o_id DESC;
	uint64_t key = orderCustKey(query->d_id, c_id);
	INDEX * index = _wl->i_order_cust;
	itemid_t itemCopy;
	itemid_t *item = NULL;
	RC indexRc = index_read_safe(index, key, wh_to_part(query->c_w_id), itemCopy);
	assert(indexRc == RCOK);
	item = &itemCopy;
	row_t * r_order = (row_t *) item->location;
	row_t * r_order_local = get_row(r_order, RD, sizeof(o_record));
	if (r_order_local == NULL) {
		return finish(Abort);
	}

	uint64_t o_id, o_entry_d, o_carrier_id;
	r_order_local->get_value(O_ID, o_id, offsetof(o_record, O_ID));
#if TPCC_ACCESS_ALL
	r_order_local->get_value(O_ENTRY_D, o_entry_d);
	r_order_local->get_value(O_CARRIER_ID, o_carrier_id);
#endif
#ifdef DEBUG
	{
		itemid_t *debugItem = index_read(_wl->i_district, distKey(query->d_id, query->w_id), wh_to_part(query->w_id));
		assert(debugItem != NULL);
		assert(debugItem->location != NULL);
		auto debugRow = (row_t *) debugItem->location;
#if CC_ALG == MVCC
		auto debugRow_local = get_row(debugRow, RD, sizeof(d_record));
#else
		auto debugRow_local = debugRow;
#endif  // CC_ALG == MVCC
		if (debugRow_local) {
			uint64_t debugNextID = 0;
			debugRow_local->get_value(D_NEXT_O_ID, debugNextID, offsetof(d_record, D_NEXT_O_ID));
			// FIXME: MVCC crashes here
			if(o_id > debugNextID) {
				std::cout << "Found inconsistent NEXT_D_ID: expected=" << debugNextID << ", found=" << o_id << "\n";
			}
			else {
			itemid_t debugItemCopy;
			RC debugRC = index_read_safe(_wl->i_order, orderPrimaryKey(query->w_id, query->d_id, o_id), d_to_part(query->w_id, query->d_id), debugItemCopy);
			assert(debugRC == RCOK);
			assert(debugItemCopy.location != NULL);
			debugRow = (row_t *) debugItemCopy.location;
			uint64_t debugOID = 0;
			debugRow->get_value(O_ID, debugOID, offsetof(o_record, O_ID));
			assert(o_id <= debugOID);
			}
		}
		if(o_id > 3000) {
			//std::cout << "OrderStatus on new order\n";
		}
	}
#endif  // DEBUG
#if DEBUG_ASSERT
	itemid_t * it = item;
	while (it != NULL && it->next != NULL) {
		uint64_t o_id_1, o_id_2;
		((row_t *)it->location)->get_value(O_ID, o_id_1);
		((row_t *)it->next->location)->get_value(O_ID, o_id_2);
		assert(o_id_1 > o_id_2);
	}
#endif

	// EXEC SQL DECLARE c_line CURSOR FOR SELECT ol_i_id, ol_supply_w_id, ol_quantity,
	// ol_amount, ol_delivery_d
	// FROM order_line
	// WHERE ol_o_id=:o_id AND ol_d_id=:d_id AND ol_w_id=:w_id;
	// EXEC SQL OPEN c_line;
	// EXEC SQL WHENEVER NOT FOUND CONTINUE;
	// i=0;
	// while (sql_notfound(FALSE)) {
	// 		i++;
	//		EXEC SQL FETCH c_line
	//		INTO :ol_i_id[i], :ol_supply_w_id[i], :ol_quantity[i], :ol_amount[i], :ol_delivery_d[i];
	// }
	key = orderlineKey(query->w_id, query->d_id, o_id);
	index = _wl->i_orderline;
	item = NULL;
	indexRc = index_read_safe(index, key, d_to_part(query->w_id, query->d_id), itemCopy);
	if ( indexRc != RCOK) {
#ifdef DEBUG
		std::cout << "Did not find Orderline\n";
#endif
		return finish(Abort);
	}
	//assert(indexRc == RCOK);
	item = &itemCopy;
	// DONE: TODO the rows are simply read without any locking mechanism
#ifdef DEBUG
	uint64_t found_ols = 0;
#endif  // DEBUG
	while (item != NULL) {
		row_t * r_orderline = get_row((row_t *) item->location, RD, sizeof(ol_record));
		if (r_orderline == NULL) {
			return finish(Abort);
		}
		//row_t * r_orderline = (row_t *) item->location;
		
#if TPCC_ACCESS_ALL
		int64_t ol_i_id, ol_supply_w_id, ol_quantity, ol_amount, ol_delivery_d;
		r_orderline->get_value(OL_I_ID, ol_i_id);
		r_orderline->get_value(OL_SUPPLY_W_ID, ol_supply_w_id);
		r_orderline->get_value(OL_QUANTITY, ol_quantity);
		r_orderline->get_value(OL_AMOUNT, ol_amount);
		r_orderline->get_value(OL_DELIVERY_D, ol_delivery_d);
#endif
		item = item->next;

#ifdef DEBUG
		++found_ols;
#endif  // DEBUG
	}
#ifdef DEBUG
	assert(found_ols);
#endif  // DEBUG

	assert( rc == RCOK );
	rc = finish(rc);
#endif  // TPCC_ALL_TXN

	return rc;
}


//TODO concurrency for index related operations is not completely supported yet.
// In correct states may happen with the current code.

RC 
tpcc_txn_man::run_delivery(tpcc_query * query) {
	RC rc = RCOK;
#if TPCC_ALL_TXN
	// XXX HACK if another delivery txn is running on this warehouse, simply commit.
	if (_wl->delivering[query->w_id].test_and_set()) {
		return finish(RCOK);
	}
	/* if ( !ATOM_CAS(_wl->delivering[query->w_id], false, true) )
		return finish(RCOK); */

	for (int d_id = 1; d_id <= DIST_PER_WARE; d_id++) {

		// EXEC SQL DECLARE c_no CURSOR FOR
		// 		SELECT no_o_id
		// 			FROM new_order
		// 			WHERE no_d_id = :d_id AND no_w_id = :w_id
		// 			ORDER BY no_o_id ASC;
		// EXEC SQL WHENEVER NOT FOUND continue;
		// EXEC SQL FETCH c_no INTO :no_o_id;

		INDEX * index;
		itemid_t * item;
		uint64_t no_o_id;
		uint64_t key = distKey(d_id, query->w_id);
#ifdef USE_OLD_IMPL
		index = _wl->i_orderline_wd;
		item = index_read(index, key, wh_to_part(query->w_id));
		assert(item != NULL);
		while (item->next != NULL) {
#if DEBUG_ASSERT
			uint64_t o_id_1, o_id_2;
			((row_t *)item->location)->get_value(OL_O_ID, o_id_1);
			((row_t *)item->next->location)->get_value(OL_O_ID, o_id_2);
			assert(o_id_1 > o_id_2);
#endif
			item = item->next;
		}

		row_t * r_orderline = (row_t *)item->location;
		r_orderline->get_value(OL_O_ID, no_o_id, offsetof(ol_record, OL_O_ID));
		
		// DONE: TODO the orderline row should be removed from the table and indexes.
#else
		index = _wl->i_neworder;
		itemid_t itemCopy;
		item = NULL;
		RC indexRC = index_read_safe(index, key, wh_to_part(query->w_id), itemCopy, true);
		if (indexRC != RCOK) {

			continue;
		}
		item = &itemCopy;
		assert(itemCopy.valid);

		assert(rc == RCOK);
		itemid_t* prev = NULL;
/* #ifdef DEBUG
		uint64_t min_oid = uint64_t(-1);
		uint64_t o_id_1 = uint64_t(-1);
#endif  // DEBUG
		while(item->next != NULL) {
			// For debugging
#ifdef DEBUG
			uint64_t o_id_2;
			((row_t *)item->location)->get_value(NO_O_ID, o_id_1, offsetof(no_record, NO_O_ID));
			if (o_id_1 < min_oid) {
				min_oid = o_id_1;
			}
			//((row_t *)item->next->location)->get_value(NO_O_ID, o_id_2, offsetof(no_record, NO_O_ID));
			//assert(o_id_1 > o_id_2);
			//if (!(o_id_1 > o_id_2)) {
			//	std::cerr << "NewOrder index not ordered properly!" << std::endl;
			//	std::abort();
			//}
#endif
			prev = item;
			item = item->next;
		}

#ifdef DEBUG
		assert(min_oid == o_id_1);
#endif */

		row_t * r_neworder = (row_t *)item->location;
		row_t * r_neworder_local = get_row(r_neworder, WR, sizeof(no_record));
		if (r_neworder_local == NULL) {
			return finish(Abort);
		}
		r_neworder_local->get_value(NO_O_ID, no_o_id, offsetof(no_record, NO_O_ID));

		// EXEC SQL DELETE FROM new_order WHERE CURRENT OF c_no;
		// XXX: Delete from index only from back

		delete_index(index, key, NULL, NULL, wh_to_part(query->w_id));
		
#endif  // USE_OLD_IMPL

		// EXEC SQL SELECT o_c_id INTO :c_id FROM orders
		//		WHERE o_id = :no_o_id AND o_d_id = :d_id AND
		//		o_w_id = :w_id;		
		// EXEC SQL UPDATE orders SET o_carrier_id = :o_carrier_id
		// 		WHERE o_id = :no_o_id AND o_d_id = :d_id AND
		// 		o_w_id = :w_id;
		index = _wl->i_order;
		key = orderPrimaryKey(query->w_id, d_id, no_o_id);
		item = NULL;
		indexRC = index_read_safe(index, key, d_to_part(query->w_id, d_id), itemCopy);
		assert(indexRC == RCOK);
		item = &itemCopy;
		row_t * r_order = (row_t *)item->location;
		row_t * r_order_local = get_row(r_order, WR, sizeof(o_record));
		if (r_order_local == NULL) {
			return finish(Abort);
		}

		uint64_t o_c_id;
		r_order_local->get_value(O_C_ID, o_c_id, offsetof(o_record, O_C_ID));
		r_order_local->set_value(O_CARRIER_ID, query->o_carrier_id, offsetof(o_record, O_CARRIER_ID), sizeof(o_record::O_CARRIER_ID));

		// EXEC SQL UPDATE order_line SET ol_delivery_d = :datetime
		// 		WHERE ol_o_id = :no_o_id AND ol_d_id = :d_id AND
		// 		ol_w_id = :w_id;
		// EXEC SQL SELECT SUM(ol_amount) INTO :ol_total FROM order_line
		// 		WHERE ol_o_id = :no_o_id AND ol_d_id = :d_id
		// 		AND ol_w_id = :w_id;
		item = NULL;
		indexRC = index_read_safe(_wl->i_orderline, orderlineKey(query->w_id, d_id, no_o_id), d_to_part(query->w_id, d_id), itemCopy);
		if(indexRC == RCOK) {
			item = &itemCopy;
		}
		double sum_ol_amount = 0;
		while (item != NULL) {
			double ol_amount = 0;
			row_t * r_orderline = (row_t *)item->location;
			row_t *r_orderline_local = get_row(r_orderline, WR, sizeof(ol_record));
			if (r_orderline_local == NULL) {
				return finish(Abort);
			}
#if !TPCC_SMALL
			r_orderline_local->set_value(OL_DELIVERY_D, query->ol_delivery_d, offsetof(ol_record, OL_DELIVERY_D), sizeof(ol_record::OL_DELIVERY_D));
			r_orderline_local->get_value(OL_AMOUNT, ol_amount, offsetof(ol_record, OL_AMOUNT));
#endif  // !TPCC_SMALL
			sum_ol_amount += ol_amount;

			item = item->next;
		}
		
		// EXEC SQL UPDATE customer SET c_balance = c_balance + :ol_total
		// 		WHERE c_id = :c_id AND c_d_id = :d_id AND
		// 		c_w_id = :w_id;
		key = custKey(o_c_id, d_id, query->w_id);
		item = index_read(_wl->i_customer_id, key, wh_to_part(query->w_id));
		row_t * r_cust = (row_t *)item->location;
		row_t * r_cust_local = get_row(r_cust, WR, sizeof(c_record));
		if (r_cust_local == NULL) {
			return finish(Abort);
		}
		double c_balance;

		r_cust_local->get_value(C_BALANCE, c_balance, offsetof(c_record, C_BALANCE));
		r_cust_local->set_value(C_BALANCE, c_balance - sum_ol_amount, offsetof(c_record, C_BALANCE), sizeof(c_record::C_BALANCE));
	}
	rc = finish(RCOK);

	// XXX HACK unlock warehouse, so another delivery txn can run
	_wl->delivering[query->w_id].clear();
#endif  // TPCC_ALL_TXN
	return rc;
}

RC 
tpcc_txn_man::run_stock_level(tpcc_query * query) {
	RC rc = RCOK;

#if TPCC_ALL_TXN
	const auto &w_id = query->w_id;
	const auto &d_id = query->d_id;
	const auto &threshold = query->threshold;
	// EXEC SQL SELECT d_next_o_id INTO :o_id
	// 		FROM district
	// 		WHERE d_w_id=:w_id AND d_id=:d_id;

	idx_key_t key = distKey(d_id, w_id);
	itemid_t *item = index_read(_wl->i_district, key, wh_to_part(w_id));
	assert(item != NULL);
	row_t * r_dist = ((row_t *)item->location);
	row_t * r_dist_local = get_row(r_dist, RD, sizeof(d_record));
	if (r_dist_local == NULL) {
		return finish(Abort);
	}
	int64_t o_id = 0;
	r_dist_local->get_value(D_NEXT_O_ID, o_id, offsetof(d_record, D_NEXT_O_ID));

	// EXEC SQL SELECT COUNT(DISTINCT (s_i_id)) INTO :stock_count
	//		FROM order_line, stock
	//		WHERE ol_w_id=:w_id AND
	//			ol_d_id=:d_id AND ol_o_id<:o_id AND
	//			ol_o_id>=:o_id-20 AND s_w_id=:w_id AND
	//			s_i_id=ol_i_id AND s_quantity < :threshold;


	thread_local std::array<uint64_t, 20*15> i_ids;
	uint64_t found_i_ids = 0;

	// Point lookup for every o_id, since we are using a hash!
	// Select ol_i_id from order_line
	//		where ol_w_id=:w_id AND
	//			ol_d_id=:d_id AND ol_o_id<:o_id AND
	//			ol_o_id>=:o_id-20
	auto index = _wl->i_orderline;
	for(int64_t i = o_id - 20; i < o_id; ++i) {
		key = orderlineKey(w_id, d_id, uint64_t(i));
		item = NULL;
		itemid_t itemCopy;
		//RC indexRC = index_read_safe(index, key, d_to_part(w_id, d_id), itemCopy, true);
		RC indexRC = index_read_safe(index, key, d_to_part(w_id, d_id), itemCopy, false);
		assert(indexRC == RCOK);
		item = &itemCopy;

		uint64_t found_ols = 0;
		while (item != NULL && found_ols < 15) {
			row_t * r_orderline = get_row((row_t *) item->location, RD, sizeof(ol_record));
			if (r_orderline == NULL) {
				return finish(Abort);
			}
			assert(found_i_ids < i_ids.size());
			r_orderline->get_value(OL_I_ID, i_ids[found_i_ids++], offsetof(ol_record, OL_I_ID));

			item = item->next;
			//item = item->prev;

			assert(i_ids[found_i_ids-1] > 0 && i_ids[found_i_ids-1] <= 100'000);
			++found_ols;
		}
#ifdef DEBUG
		if (found_ols > 15) {
			std::cout << "Found more orderlines than expected: " << found_ols << "\n";
		}
		assert(found_ols);
#endif  // DEBUG
	}

	// SELECT COUNT(DISTINCT (s_i_id)) INTO :stock_count
	//		FROM stock
	//		WHERE s_w_id=:w_id AND
	//			s_i_id=ol_i_id AND s_quantity < :threshold;
	uint64_t result = 0;
	uint64_t prev_i_id = uint64_t(-1);

	// Distinct i_ids
	std::sort(i_ids.begin(), i_ids.begin() + found_i_ids);

	INDEX * stock_index = _wl->i_stock;
	for(uint64_t i = 0; i < found_i_ids; ++i) {

		const auto &i_id = i_ids[i];

		if (i_id != prev_i_id) {

			uint64_t stock_key = stockKey(i_id, w_id);
			itemid_t * stock_item = index_read(stock_index, stock_key, wh_to_part(w_id));
			assert(stock_item != NULL);
			row_t * r_stock = ((row_t *)stock_item->location);
			row_t * r_stock_local = get_row(r_stock, RD, sizeof(s_record));
			if (r_stock_local == NULL) {
				return finish(Abort);
			}

			uint64_t quantity;
			r_stock_local->get_value(S_QUANTITY, quantity, offsetof(s_record, S_QUANTITY));

			if(quantity < threshold) {
				++result;
			}

			prev_i_id = i_id;
		}
		else {
			// Skip duplicate i_id
			continue;
		}
	}

	rc = finish(RCOK);
#endif  // TPCC_ALL_TXN

	return RCOK;
}
