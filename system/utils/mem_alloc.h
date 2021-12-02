#ifndef _MEM_ALLOC_H_
#define _MEM_ALLOC_H_

#include "global.h"
#include <map>

const int SizeNum = 4; //specifies nr of different BlockSizes used. [assert(SizeNum <= length(BlockSizes))]
const UInt32 BlockSizes[] = { 32, 64, 256, 1024 };


typedef struct free_block {
	int size;
	struct free_block* next;
} FreeBlock;

class Arena {
public:
	void init(int arena_id, int size); // store parameters and point ptr to NULL
	void* alloc(); //allocate storage for Arena (either reclaim memory from _head or use buffer). If size exceeds remaining buffer capacity, buffer size will be increased autmatically by: _block_size * 40960
	void free(void* ptr); //add memory to reclamation list (entry node: _head)

private:
	char* _buffer;
	int 		_size_in_buffer;
	int 		_arena_id;
	int 		_block_size;
	FreeBlock* _head;
	char 		_pad[128 - sizeof(int) * 3 - sizeof(void*) * 2 - 8];
};

class mem_alloc {
public:
	void init(uint64_t part_cnt, uint64_t bytes_per_part);

	void register_thread(int thd_id); //register this thread und thd_id in pid_arena
	void unregister(); //unregister all threads from pid_arena

	void* alloc(uint64_t size, uint64_t part_id);
	void free(void* block, uint64_t size);
	int get_arena_id(); // get arena id of this thread (0 if not found)

private:
	void init_thread_arena();
	int get_size_id(UInt32 size); //get min blockSize larger than size. size parameter should not exceed largest BlockSize

	// each thread has several arenas for different block size
	Arena** _arenas;
	int _bucket_cnt;
	std::pair<pthread_t, int>* pid_arena;
	pthread_mutex_t         pid_arena_lock; // synchronizes accesses to pid_arena
};

#endif
