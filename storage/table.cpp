#include "global.h"
#include "helper.h"
#include "table.h"
#include "catalog.h"
#include "row.h"
#include "utils/mem_alloc.h"
#include "utils/hawk_alloc.h"

void table_t::init(Catalog * schema) {
	this->table_name = schema->table_name;
	this->schema = schema;
}

/*RC table_t::get_new_row(row_t *& row) {
	// this function is obsolete. 
	assert(false);
	return RCOK;
}*/

// the row is not stored locally. the pointer must be maintained by index structure.
RC table_t::get_new_row(row_t *& row, uint64_t part_id, uint64_t &row_id, uint64_t tuple_size) {
	RC rc = RCOK;
	//cur_tab_size ++;
	
#if MALLOC_TYPE == HAWK_ALLOC || THREAD_ALLOC_HAWK_INSERT
	row = (row_t *) malloc_hawk(sizeof(row_t));
	//char* data = (char*) malloc_hawk(tuple_size);
	rc = row->init(this, part_id, row_id, tuple_size);
#else
	row = (row_t *) _mm_malloc(sizeof(row_t), 64);
	rc = row->init(this, part_id, row_id, tuple_size);
#endif  // THREAD_ALLOC_HAWK
	row->init_manager(row);

	return rc;
}

// the row is not stored locally. the pointer must be maintained by index structure.
RC table_t::get_new_row(row_t *& row, uint64_t part_id, uint64_t &row_id, uint64_t tuple_size, uint64_t count) {
	RC rc = RCOK;
	//cur_tab_size += count;
	
#if MALLOC_TYPE == HAWK_ALLOC || THREAD_ALLOC_HAWK_INSERT
	row = (row_t *) malloc_hawk(sizeof(row_t)*count);
	for(uint64_t i=0; i < count; ++i)
	{
		//char* data = (char*)malloc_hawk(tuple_size);
		rc = row[i].init(this, part_id, row_id, tuple_size);
		row[i].init_manager(row+i);
	}
#else
	row = (row_t *) _mm_malloc(sizeof(row_t)*count, 64);
	for(uint64_t i=0; i < count; ++i)
	{
		rc = row[i].init(this, part_id, row_id, tuple_size);
		row[i].init_manager(row+i);
	}
#endif  // THREAD_ALLOC_HAWK

	return rc;
}
