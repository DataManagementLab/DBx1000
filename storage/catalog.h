#pragma once 

#include <map>
#include <vector>
#include "global.h"
#include "helper.h"

class Column {
public:
	Column() {
		this->type = new char[80];
		this->name = new char[80];
	}
	Column(uint64_t size, char* type, char* name,
		uint64_t id, uint64_t index)
	{
		this->size = size;
		this->id = id;
		this->index = index;
		this->type = new char[80];
		this->name = new char[80];
		strcpy(this->type, type);
		strcpy(this->name, name);
	};

	UInt64 id;
	UInt32 size;
	UInt32 index; // mem offset in the tuple (in bytes)
	char* type;
	char* name;

	char pad[CL_SIZE - sizeof(uint64_t) * 3 - sizeof(char*) * 2]; //fill 1 cacheline completely
};

class Catalog {
public:

	void init(const char* table_name, int field_cnt); // initialize catalog and reserve field_cnt columns.
	void add_col(char* col_name, uint64_t size, char* type); 	// add column with given name, size and type to catalog

	UInt32 			field_cnt;
	UInt32			max_field_cnt;
	const char* table_name;

	UInt32 			get_tuple_size() { return tuple_size; };

	uint64_t 		get_field_cnt() { return field_cnt; };
	uint64_t 		get_field_size(int id) { return _columns[id].size; };
	uint64_t 		get_field_index(int id) { return _columns[id].index; };
	char* 			get_field_type(uint64_t id);
	char* 			get_field_name(uint64_t id);

	uint64_t 		get_field_id(const char* name); // lookup field id for given name. name must exists (otherthise assert fails).
	char* 			get_field_type(char* name);
	uint64_t 		get_field_index(char* name);

	void 			print_schema();
	Column* _columns;
	UInt32 			tuple_size; //size of all tuples. Grows by adding futher columns.
};

