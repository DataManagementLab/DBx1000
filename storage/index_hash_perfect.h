#pragma once

#include "global.h"
#include "helper.h"
//#include "index_base.h"
#include "utils/mem_alloc.h"
#include "utils/hawk_alloc.h"
#include "table.h"


// TODO Hash index does not support partition yet.
// Hash with unique keys (w/o hash collision)
class IndexHashPerfect
{
public:
#if defined(SCOPED_LATCH_T) && SCOPED_LATCH
	using Latch = SCOPED_LATCH_T;
#elif defined(LATCH_T)
	using Latch = LATCH_T;
#endif

	RC init(uint64_t bucket_cnt, int part_cnt)
	{
		_bucket_cnt = bucket_cnt;
		_bucket_cnt_per_part = bucket_cnt / part_cnt;
		_buckets = new itemid_t *[part_cnt];
#if TPCC_ALL_TXN
		_bucket_tails = new itemid_t **[part_cnt];
#endif  // TPCC_ALL_TXN
		_latch = new Latch *[part_cnt];

		for(uint64_t i = 0; i < part_cnt; ++i)
		{
			_buckets[i] = nullptr;
#if TPCC_ALL_TXN
			_bucket_tails[i] = nullptr;
#endif  // TPCC_ALL_TXN
			_latch[i] = nullptr;
		}
		return RCOK;
	}

	RC
	init(int part_cnt, table_t *table, uint64_t bucket_cnt)
	{
		init(bucket_cnt, part_cnt);
		//this->table = table;
		return RCOK;
	}

	bool index_exist(idx_key_t key)
	{
		assert(false);
	}

	// XXX Not thread safe!
	RC index_insert(idx_key_t key, itemid_t *item, int part_id, bool no_collision = true)
	{
		RC rc = RCOK;
		uint64_t bkt_idx = hash(key);
		assert(bkt_idx < _bucket_cnt_per_part);

		if(_latch[part_id] == nullptr)
		{
			auto latch = new Latch[_bucket_cnt_per_part];

			auto &&bkt_latch = latch[0];
#if SCOPED_LATCH
			SCOPED_LATCH_T::scoped_lock lock;
			lock.acquire(bkt_latch);
#else
			bkt_latch.lock();
#endif  // SCOPED_LATCH

			if(ATOM_CAS(_latch[part_id], NULL, latch)) {

				_buckets[part_id] =  new itemid_t[_bucket_cnt_per_part];
#if TPCC_ALL_TXN
				_bucket_tails[part_id] =  new itemid_t*[_bucket_cnt_per_part];
#endif  // TPCC_ALL_TXN
				for(uint64_t i = 0; i < _bucket_cnt_per_part; ++i)
				{
					_buckets[part_id][i].init();
#if TPCC_ALL_TXN
					_bucket_tails[part_id][i] = nullptr;
#endif  // TPCC_ALL_TXN
				}

			}
			else {
				delete[] latch;

				auto &&bkt_latch = _latch[part_id][0];
#if SCOPED_LATCH
				lock.release();
				lock.acquire(bkt_latch);
#else
				bkt_latch.lock();
#endif  // SCOPED_LATCH
			}
#if !SCOPED_LATCH
			bkt_latch.unlock();
	#endif  // !SCOPED_LATCH
		}

		auto &&head = _buckets[part_id][bkt_idx];
		if (head.valid) {
			assert(no_collision == false);
#if MALLOC_TYPE == HAWK_ALLOC
			auto next = (itemid_t*) malloc_hawk(sizeof(itemid_t));
#else
			auto next = (itemid_t*) mem_allocator.alloc(sizeof(itemid_t), part_id);
#endif  // MALLOC_TYPE
			*next = head;
			item->next = next;
			head = *item;
#if TPCC_ALL_TXN
			next->prev = &head;

			if (next->next == NULL) {
				_bucket_tails[part_id][bkt_idx] = next;
			}
			else {
				next->next->prev = next;
			}
#endif // TPCC_ALL_TXN
		}
		else {
			assert(_buckets[part_id][bkt_idx].valid == false);
			_buckets[part_id][bkt_idx] = *item;
#if TPCC_ALL_TXN
			_bucket_tails[part_id][bkt_idx] =  &(_buckets[part_id][bkt_idx]);
#endif  // TPCC_ALL_TXN
		}

		return rc;
	}

	RC index_insert_safe(idx_key_t key, itemid_t *item, int part_id, bool no_collision = true)
	{
		RC rc = RCOK;
		uint64_t bkt_idx = hash(key);
		assert(bkt_idx < _bucket_cnt_per_part);

		if(_latch[part_id] == nullptr)
		{
			auto latch = new Latch[_bucket_cnt_per_part];

			auto &&bkt_latch = latch[0];
#if SCOPED_LATCH
			SCOPED_LATCH_T::scoped_lock lock;
			lock.acquire(bkt_latch);
#else
			bkt_latch.lock();
#endif  // SCOPED_LATCH

			if(ATOM_CAS(_latch[part_id], NULL, latch)) {

				_buckets[part_id] =  new itemid_t[_bucket_cnt_per_part];
#if TPCC_ALL_TXN
				_bucket_tails[part_id] =  new itemid_t*[_bucket_cnt_per_part];
#endif  // TPCC_ALL_TXN
				for(uint64_t i = 0; i < _bucket_cnt_per_part; ++i)
				{
					_buckets[part_id][i].init();
#if TPCC_ALL_TXN
					_bucket_tails[part_id][i] = nullptr;
#endif  // TPCC_ALL_TXN
				}

			}
			else {
				delete[] latch;

				auto &&bkt_latch = _latch[part_id][0];
#if SCOPED_LATCH
				lock.release();
				lock.acquire(bkt_latch);
#else
				bkt_latch.lock();
#endif  // SCOPED_LATCH
			}
#if !SCOPED_LATCH
			bkt_latch.unlock();
	#endif  // !SCOPED_LATCH
		}

		auto &&head = _buckets[part_id][bkt_idx];

		{
			auto &&bkt_latch = _latch[part_id][bkt_idx];
#if SCOPED_LATCH
			SCOPED_LATCH_T::scoped_lock lock;
			lock.acquire(bkt_latch);
#else
			bkt_latch.lock();
#endif  // SCOPED_LATCH

			if (head.valid) {
				assert(no_collision == false);
	#if MALLOC_TYPE == HAWK_ALLOC
				auto next = (itemid_t*) malloc_hawk(sizeof(itemid_t));
	#else
				auto next = (itemid_t*) mem_allocator.alloc(sizeof(itemid_t), part_id);
	#endif  // MALLOC_TYPE
				*next = head;
				item->next = next;
				head = *item;
#if TPCC_ALL_TXN
				next->prev = &head;

				if (next->next == NULL) {
					_bucket_tails[part_id][bkt_idx] = next;
				}
				else {
					next->next->prev = next;
				}
#endif // TPCC_ALL_TXN
			}
			else {
				assert(_buckets[part_id][bkt_idx].valid == false);
				_buckets[part_id][bkt_idx] = *item;
#if TPCC_ALL_TXN
				_bucket_tails[part_id][bkt_idx] =  &(_buckets[part_id][bkt_idx]);
#endif  // TPCC_ALL_TXN
			}

	#if !SCOPED_LATCH
			bkt_latch.unlock();
	#endif  // !SCOPED_LATCH
		}


		return rc;
	}

	/* RC index_read(idx_key_t key, itemid_t *&item, int part_id = -1)
	{
		uint64_t bkt_idx = hash(key);
		assert(bkt_idx < _bucket_cnt_per_part);
		if (_buckets[part_id][bkt_idx].valid) {
			item = &_buckets[part_id][bkt_idx];
		}
		else {
			item = NULL;
		}

		RC rc = RCOK;
		return rc;
	} */

	RC index_read(idx_key_t key, itemid_t *&item,
				  int part_id = -1, int thd_id = 0, bool back = false)
	{
		uint64_t bkt_idx = hash(key);
		assert(bkt_idx < _bucket_cnt_per_part);
#if TPCC_ALL_TXN
		if (!back) {
#endif  // TPCC_ALL_TXN
			if (_buckets[part_id] && _buckets[part_id][bkt_idx].valid) {
				item = &_buckets[part_id][bkt_idx];
			}
			else {
				item = NULL;
			}
#if TPCC_ALL_TXN
		}
		else {
			item = _bucket_tails[part_id][bkt_idx];
		}
#endif  // TPCC_ALL_TXN

		RC rc = RCOK;
		return rc;
	}

	RC index_read_safe(idx_key_t key, itemid_t &item,
				  int part_id = -1, int thd_id = 0, bool back = false)
	{
		RC rc = RCOK;
		uint64_t bkt_idx = hash(key);
		assert(bkt_idx < _bucket_cnt_per_part);
		if (_buckets[part_id]) {
			auto &&bkt_latch = _latch[part_id][bkt_idx];
#if SCOPED_LATCH
			SCOPED_LATCH_T::scoped_lock lock;
			lock.acquire(bkt_latch);
#else
			bkt_latch.lock();
#endif  // SCOPED_LATCH

#if TPCC_ALL_TXN
			if (!back) {
#endif  // TPCC_ALL_TXN
				if (_buckets[part_id][bkt_idx].valid) {
					item = _buckets[part_id][bkt_idx];
					rc = RCOK;
				}
				else {
					//item = NULL;
					rc = RC::ERROR;
				}
#if TPCC_ALL_TXN
			}
			else {
				if (_bucket_tails[part_id][bkt_idx]->valid) {

					item = *_bucket_tails[part_id][bkt_idx];
					rc = RCOK;
				}
				else {
					rc = RC::ERROR;
				}
			}
#endif  // TPCC_ALL_TXN
#if !SCOPED_LATCH
			bkt_latch.unlock();
	#endif  // !SCOPED_LATCH
		}
		else {
			//item = NULL;
			rc = RC::ERROR;
		}

		return rc;
	}

	// XXX Not thread safe!
	RC index_delete(idx_key_t key, itemid_t* item, itemid_t *hint, int part_id, int thd_id = 0) {

#if TPCC_ALL_TXN
		itemid_t *prev = item->prev;
		itemid_t *next = item->next;

		if(prev != NULL) {
			// Not head

			// Delete non-head of collision chain
			prev->next = next;

			if (next) {
				next->prev = prev;
			}
			else {
				// New tail
				_bucket_tails[part_id][hash(key)] = prev;
			}

#if MALLOC_TYPE == HAWK_ALLOC
#else
				mem_allocator.free(item, sizeof(itemid_t));
#endif  // MALLOC_TYPE

		} else {
			// Head of linked list
			if(next) {
				*item = *next;
				if(item->next == NULL) {
					_bucket_tails[part_id][hash(key)] = item;
				}
			}
			else {
				item->init();
				_bucket_tails[part_id][hash(key)] = item;
			}
		}
#else

		itemid_t *prev = hint;

		if(prev == NULL) {
			index_read(key, prev, part_id, thd_id);

			// Item is not head
			if (prev != item) {
				// Search for item
				while(prev->next != item && prev->next != NULL) {
					prev = prev->next;
				}
			}
		}

		if(prev != NULL) {
			if(prev != item) {
				// Delete non-head of collision chain
				prev->next = item->next;

#if MALLOC_TYPE == HAWK_ALLOC
#else
				mem_allocator.free(item, sizeof(itemid_t));
#endif  // MALLOC_TYPE
				
			} else {
				// Delete head of collision chain
				if(item->next) {
					*prev = *item->next;
				}
				else {
					prev->init();
				}
			}
		}
#endif  // TPCC_ALL_TXN

		return RCOK;
	};

	RC index_delete_safe(idx_key_t key, itemid_t* item, itemid_t *hint, int part_id, int thd_id = 0) {

		auto bkt_idx = hash(key);
auto &&bkt_latch = _latch[part_id][bkt_idx];
#if SCOPED_LATCH
		SCOPED_LATCH_T::scoped_lock lock;
		lock.acquire(bkt_latch);
#else
		bkt_latch.lock();
#endif  // SCOPED_LATCH

#if TPCC_ALL_TXN

		item = _bucket_tails[part_id][hash(key)];
		itemid_t *prev = item->prev;
		itemid_t *next = item->next;

		if(prev != NULL) {
			// Not head

			// Delete non-head of collision chain
			prev->next = next;

			if (next) {
				next->prev = prev;
			}
			else {
				// New tail
				_bucket_tails[part_id][hash(key)] = prev;
			}

#if MALLOC_TYPE == HAWK_ALLOC
#else
				mem_allocator.free(item, sizeof(itemid_t));
#endif  // MALLOC_TYPE

		} else {
			// Head of linked list
			if(next) {
				*item = *next;
				if(item->next == NULL) {
					_bucket_tails[part_id][hash(key)] = item;
				}
			}
			else {
				item->init();
				_bucket_tails[part_id][hash(key)] = item;
			}
		}
#else
		itemid_t *prev = hint;

		if(prev == NULL) {
			index_read(key, prev, part_id, thd_id);

			// Item is not head
			if (prev != item) {
				// Search for item
				while(prev->next != item && prev->next != NULL) {
					prev = prev->next;
				}
			}
		}

		if(prev != NULL) {
			if(prev != item) {
				// Delete non-head of collision chain
				prev->next = item->next;

#if MALLOC_TYPE == HAWK_ALLOC
#else
				mem_allocator.free(item, sizeof(itemid_t));
#endif  // MALLOC_TYPE
				
			} else {
				// Delete head of collision chain
				if(item->next) {
					*prev = *item->next;
				}
				else {
					prev->init();
				}
			}
		}
#endif  // TPCC_ALL_TXN

#if !SCOPED_LATCH
		bkt_latch.unlock();
	#endif  // !SCOPED_LATCH

		return RCOK;
	};

private:

	// TODO implement more complex hash function
	uint64_t hash(idx_key_t key) { return key % _bucket_cnt_per_part; }

	itemid_t **_buckets;
	itemid_t ***_bucket_tails;
	// Latch per item chain
	Latch **_latch;
	uint64_t _bucket_cnt;
	uint64_t _bucket_cnt_per_part;
};
