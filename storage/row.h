#pragma once 

#include <cassert>
#include "global.h"
#include "table.h"
#include "catalog.h"
#include "txn.h"
#include "row_lock.h"
#include "row_ts.h"
#include "row_mvcc.h"
#include "row_hekaton.h"
#include "row_occ.h"
#include "row_tictoc.h"
#include "row_silo.h"
#include "row_vll.h"
#include "utils/mem_alloc.h"
#include "utils/hawk_alloc.h"

class txn_man;

class row_t
{
public:

	using Manager_t =
#if CC_ALG == DL_DETECT || CC_ALG == NO_WAIT || CC_ALG == WAIT_DIE
		Row_lock;
#elif CC_ALG == TIMESTAMP
		Row_ts;
#elif CC_ALG == MVCC
		Row_mvcc;
#elif CC_ALG == HEKATON
		Row_hekaton;
#elif CC_ALG == OCC
		Row_occ;
#elif CC_ALG == TICTOC
		Row_tictoc;
#elif CC_ALG == SILO
		Row_silo;
#elif CC_ALG == VLL
		Row_vll;
#else
		void;
#endif

	// Initializes row_t, and allocates memory of tuple_size bytes.
	RC init(table_t* host_table, uint64_t part_id, uint64_t row_id, uint64_t tuple_size) {
		_row_id = row_id;
		_part_id = part_id;
		this->table = host_table;
#if MALLOC_TYPE == HAWK_ALLOC
		data = (char*)malloc_hawk(sizeof(char) * tuple_size);
#else
		data = (char*)_mm_malloc(sizeof(char) * tuple_size, 64);
#endif 
		return RCOK;
	}

	// Initialize with existing memory
	RC init(table_t* host_table, uint64_t part_id, uint64_t row_id, char* data) {
		_row_id = row_id;
		_part_id = part_id;
		this->table = host_table;
		this->data = data;
		return RCOK;
	}

	// Allocate memory of size size for storing the data.
	void init(int size)
	{
#if MALLOC_TYPE == HAWK_ALLOC
		data = (char*)malloc_hawk(size);
#else
		data = (char*)_mm_malloc(size, 64);
#endif
	}

	// Assign a different host_table / schema to this row.
	RC switch_schema(table_t* host_table) {
		this->table = host_table;
		return RCOK;
	}

	// Allocate memory for the CC managers and initialize them.
	void init_manager(row_t* row) {
#if MALLOC_TYPE == HAWK_ALLOC
		//manager = (Manager_t*) malloc_hawk(sizeof(Manager_t));
#else
#if CC_ALG == DL_DETECT || CC_ALG == NO_WAIT || CC_ALG == WAIT_DIE
		manager = (Row_lock*)mem_allocator.alloc(sizeof(Row_lock), _part_id);
#elif CC_ALG == TIMESTAMP
		manager = (Row_ts*)mem_allocator.alloc(sizeof(Row_ts), _part_id);
#elif CC_ALG == MVCC
		manager = (Row_mvcc*)_mm_malloc(sizeof(Row_mvcc), 64);
#elif CC_ALG == HEKATON
		manager = (Row_hekaton*)_mm_malloc(sizeof(Row_hekaton), 64);
#elif CC_ALG == OCC
		manager = (Row_occ*)mem_allocator.alloc(sizeof(Row_occ), _part_id);
#elif CC_ALG == TICTOC
		manager = (Row_tictoc*)_mm_malloc(sizeof(Row_tictoc), 64);
#elif CC_ALG == SILO
		manager = (Row_silo*)_mm_malloc(sizeof(Row_silo), 64);
#elif CC_ALG == VLL
		manager = (Row_vll*)mem_allocator.alloc(sizeof(Row_vll), _part_id);
#endif
#endif

#if CC_ALG != HSTORE
		getManager()->init(this);
#endif
	}

	table_t* get_table() {
		return table;
	}

	Catalog* get_schema() {
		return get_table()->get_schema();
	}

	const char* get_table_name() {
		return get_table()->get_table_name();
	};
	uint64_t get_tuple_size() {
		return get_schema()->get_tuple_size();
	}

	uint64_t get_field_cnt() {
		return get_schema()->field_cnt;
	}
	uint64_t get_row_id() { return _row_id; };

	// copy from the src to this
	void copy(row_t* src, uint64_t tuple_size) {
#if BYPASS_CATALOG == false
		tuple_size = src->get_tuple_size();
#endif  // BYPASS_CATALOG
		set_data(src->get_data(), tuple_size);
	}

	void 		set_primary_key(uint64_t key) { _primary_key = key; };
	uint64_t 	get_primary_key() { return _primary_key; };
	uint64_t 	get_part_id() { return _part_id; };

	// Stores data of size "datasize" stored in "ptr" at position "pos" in data. 
	// "col_id" parameter is used for datasize and position identification if BYPASS_CATALOG active.
	// "ptr" must be trivially copyable or void ptr.
	template<typename type>
	void set_value(int col_id, type* ptr, uint32_t pos, uint32_t datasize) {

		static_assert(std::is_trivially_copyable<type>::value || std::is_void<type>::value);

#if DEBUG_CATALOG
		assert(get_schema()->get_field_index(id) == pos);
		assert(get_schema()->get_field_size(id) == datasize);
#endif  // DEBUG_CATALOG

#if BYPASS_CATALOG == false
		datasize = get_schema()->get_field_size(col_id);
		pos = get_schema()->get_field_index(col_id);
#endif // BYPASS_CATALOG
		memcpy(&data[pos], ptr, datasize);
	}

#if BYPASS_CATALOG == false
	// Store value in columnt with col_id
	void set_value(int col_id, void* ptr, int size) {
		int pos = get_schema()->get_field_index(col_id);
		memcpy(&data[pos], ptr, size);
	}
#endif  // BYPASS_CATALOG

	// Store value in column with col_name
	void set_value(const char* col_name, void* ptr, uint32_t pos, uint32_t datasize) {
		uint64_t id;
#if BYPASS_CATALOG && !DEBUG_CATALOG
		id = -1;
#else
		id = get_schema()->get_field_id(col_name);
#endif  // BYPASS_CATALOG
		set_value(id, ptr, pos, datasize);
	}

#if BYPASS_CATALOG == false
	char* get_value(int id) {
		int pos = get_schema()->get_field_index(id);
		return &data[pos];
	}
#endif  // BYPASS_CATALOG

	template<typename type>
	void set_value(int col_id, type value, uint32_t pos, uint32_t field_size) {
		set_value(col_id, &value, pos, field_size);
	}

	template<typename type>
	void get_value(int col_id, type& value, uint32_t pos) {
#if DEBUG_CATALOG
		assert(get_schema()->get_field_index(col_id) == pos);
#endif  // DEBUG_CATALOG

#if BYPASS_CATALOG == false
		pos = get_schema()->get_field_index(col_id);
#endif  // BYPASS_CATALOG

		if constexpr (std::is_pointer<type>::value)
			value = reinterpret_cast<type>(data + pos);
		else
			value = *(type*)&data[pos];
	}

	void set_data(char* data, uint64_t size) {
		memcpy(this->data, data, size);
	}

	template<typename type>
	void set_data(type&& data)
	{
		*reinterpret_cast<type*>(this->data) = std::move(data);
	}

	char* get_data() { return data; }

	void free_row() {
		free(data);
	}

	// for concurrency control. can be lock, timestamp etc.
	RC get_row(access_t type, txn_man* txn, row_t*& row, uint64_t tuple_size);
	void return_row(access_t type, txn_man* txn, row_t* row, uint64_t tuple_size);

#if CC_ALG != HSTORE
	inline Manager_t* getManager() noexcept
	{
#if MALLOC_TYPE == HAWK_ALLOC
		return &manager;
#else
		return manager;
#endif  // THREAD_ALLOC_HAWK
	}
#endif  // CC != HSTORE

	char* data;
	table_t* table;
private:
#if CC_ALG != HSTORE
#if MALLOC_TYPE == HAWK_ALLOC
	Manager_t manager;
#else
	Manager_t* manager;
#endif  // THREAD_ALLOC_HAWK
#endif  // CC != HSTORE
	// primary key should be calculated from the data stored in the row.
	uint64_t 		_primary_key;
	uint64_t		_part_id;
	uint64_t 		_row_id;
};
