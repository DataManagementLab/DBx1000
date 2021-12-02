#ifndef _TPCC_H_
#define _TPCC_H_

#include "wl.h"
#include "txn.h"

class table_t;
class INDEX;
class tpcc_query;

class tpcc_wl : public workload {
public:
#if REPLICATE_INTERNALS
	tpcc_wl() = default;
	tpcc_wl(const tpcc_wl&) = default;
#endif  // REPLICATE_INTERNALS
	RC init();
	RC init_table();
	RC size_table();
	RC init_schema(const char* schema_file);
	RC get_txn_man(txn_man*& txn_manager, thread_t* h_thd);
	table_t* t_warehouse = nullptr;
	table_t* t_district = nullptr;
	table_t* t_customer = nullptr;
	table_t* t_history = nullptr;
	table_t* t_neworder = nullptr;
	table_t* t_order = nullptr;
	table_t* t_orderline = nullptr;
	table_t* t_item = nullptr;
	table_t* t_stock = nullptr;

	INDEX* i_item = nullptr;
	INDEX* i_warehouse = nullptr;
	INDEX* i_district = nullptr;
	INDEX* i_customer_id = nullptr;
#if INDEX_STRUCT == IDX_HASH_PERFECT
	IndexHash* i_customer_last = nullptr;
#else
	INDEX* i_customer_last = nullptr;
#endif // IDX_HASH_PERFECT
	INDEX* i_stock = nullptr;
	INDEX* i_order = nullptr; // key = (w_id, d_id, o_id)
	INDEX* i_order_cust = nullptr; // Part by w_id, key = (d_id, c_id) = orderCustKey
	INDEX* i_orderline = nullptr; // key = (w_id, d_id, o_id)
	//INDEX* i_orderline_wd = nullptr; // key = (w_id, d_id). 
	INDEX* i_neworder = nullptr; // key = (w_id, d_id). 

	//bool** delivering = nullptr;
#if REPLICATE_INTERNALS
	static inline
#endif  // REPLICATE_INTERNALS
	std::array<std::atomic_flag, NUM_WH> delivering;
	uint32_t next_tid = 0;
#if REPLICATE_INTERNALS
	static inline
#endif  // REPLICATE_INTERNALS
	std::atomic_uint64_t workload_size = 0;
	static inline thread_local uint64_t thread_workload_size = 0;
private:
	uint64_t num_wh = 0;
	void init_tab_item(uint32_t part_id = 0);
	void init_tab_wh(uint32_t wid);
	void init_tab_dist(uint64_t w_id);
	void init_tab_stock(uint64_t w_id);
	void init_tab_cust(uint64_t d_id, uint64_t w_id);
	void init_tab_hist(uint64_t c_id, uint64_t d_id, uint64_t w_id);
	void init_tab_order(uint64_t d_id, uint64_t w_id);

	void size_tab_item(uint32_t part_id = 0);
	void size_tab_wh(uint32_t wid);
	void size_tab_dist(uint64_t w_id);
	void size_tab_stock(uint64_t w_id);
	void size_tab_cust(uint64_t d_id, uint64_t w_id);
	void size_tab_hist(uint64_t c_id, uint64_t d_id, uint64_t w_id);
	void size_tab_order(uint64_t d_id, uint64_t w_id);

	void init_permutation(uint64_t* perm_c_id, uint64_t wid);

	static void* threadInitItem(void* This);
	static void* threadInitWh(void* This);
	static void* threadInitDist(void* This);
	static void* threadInitStock(void* This);
	static void* threadInitCust(void* This);
	static void* threadInitHist(void* This);
	static void* threadInitOrder(void* This);

	static void* threadInitWarehouse(void* This, int tid);
	static void* threadSizeWarehouse(void* This, int tid);
};

class tpcc_txn_man : public txn_man
{
public:
	void init(thread_t* h_thd, workload* h_wl, uint64_t part_id);
	RC run_txn(base_query* query);
private:
	tpcc_wl* _wl;
#if REPLICATE_ITEM
	uint32_t _node = 0;
#endif  // REPLICATE_ITEM
	RC run_payment(tpcc_query* m_query);
	RC run_new_order(tpcc_query* m_query);
	RC run_order_status(tpcc_query* query);
	RC run_delivery(tpcc_query* query);
	RC run_stock_level(tpcc_query* query);

	inline uint32_t getItemPart() const noexcept {
#if REPLICATE_ITEM
		return _node;
#else
		return 0;
#endif  // REPLICATE_ITEM
	}

#if REPLICATE_INTERNALS
	static inline std::array<std::atomic<tpcc_wl*>, MAX_SOCKETS> _wl_replicas{};
#endif  // REPLICATE_INTERNALS
};

#endif
