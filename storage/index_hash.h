#pragma once

#include "global.h"
#include "helper.h"
#include "index_base.h"
#include "utils/mem_alloc.h"
#include "table.h"
#include "index_hash_perfect.h"

//TODO make proper variables private
// each BucketNode contains items sharing the same key
class BucketNode
{
public:
	BucketNode(idx_key_t key) { init(key); };
	void init(idx_key_t key)
	{
		this->key = key;
		next = NULL;
		items = NULL;
	}
	idx_key_t key;
	// The node for the next key
	BucketNode* next;
	// NOTE. The items can be a list of items connected by the next pointer.
	itemid_t* items;
};

// BucketHeader does concurrency control of Hash
class BucketHeader
{
public:
	void init()
	{
		node_cnt = 0;
		first_node = NULL;
		locked = false;
	}

	void insert_item(idx_key_t key,
		itemid_t* item,
		int part_id)
	{
		BucketNode* cur_node = first_node;
		BucketNode* prev_node = NULL;
		while (cur_node != NULL)
		{
			if (cur_node->key == key)
				break;
			prev_node = cur_node;
			cur_node = cur_node->next;
		}
		if (cur_node == NULL)
		{
			BucketNode* new_node = (BucketNode*)
				mem_allocator.alloc(sizeof(BucketNode), part_id);
			new_node->init(key);
			new_node->items = item;
			if (prev_node != NULL)
			{
				new_node->next = prev_node->next;
				prev_node->next = new_node;
			}
			else
			{
				new_node->next = first_node;
				first_node = new_node;
			}
		}
		else
		{
			item->next = cur_node->items;
			cur_node->items = item;
		}
	}

	void read_item(idx_key_t key, itemid_t*& item)
	{
		BucketNode* cur_node = first_node;
		while (cur_node != NULL)
		{
			if (cur_node->key == key)
				break;
			cur_node = cur_node->next;
		}
		M_ASSERT(cur_node->key == key, "Key does not exist!");
		item = cur_node->items;
	}
	BucketNode* first_node;
	uint64_t node_cnt;
	bool locked;
};

// TODO Hash index does not support partition yet.
class IndexHash //: public index_base
{
public:
	RC init(uint64_t bucket_cnt, int part_cnt)
	{
		_bucket_cnt = bucket_cnt;
		_bucket_cnt_per_part = bucket_cnt / part_cnt;
		_buckets = new BucketHeader * [part_cnt];
		for (int i = 0; i < part_cnt; i++)
		{
			_buckets[i] = (BucketHeader*)_mm_malloc(sizeof(BucketHeader) * _bucket_cnt_per_part, 64);
			for (uint32_t n = 0; n < _bucket_cnt_per_part; n++)
				_buckets[i][n].init();
		}
		return RCOK;
	}

	RC
		init(int part_cnt, table_t* table, uint64_t bucket_cnt)
	{
		init(bucket_cnt, part_cnt);
		//this->table = table;
		return RCOK;
	}

	bool index_exist(idx_key_t key)
	{
		assert(false);
	}

	RC index_insert(idx_key_t key, itemid_t* item, int part_id, bool /* no_collision */ = true)
	{
		RC rc = RCOK;
		uint64_t bkt_idx = hash(key);
		assert(bkt_idx < _bucket_cnt_per_part);
		BucketHeader* cur_bkt = &_buckets[part_id][bkt_idx];
		// 1. get the ex latch
		get_latch(cur_bkt);

		// 2. update the latch list
		cur_bkt->insert_item(key, item, part_id);

		// 3. release the latch
		release_latch(cur_bkt);
		return rc;
	}

	RC index_read(idx_key_t key, itemid_t*& item, int part_id = -1)
	{
		uint64_t bkt_idx = hash(key);
		assert(bkt_idx < _bucket_cnt_per_part);
		BucketHeader* cur_bkt = &_buckets[part_id][bkt_idx];
		RC rc = RCOK;
		// 1. get the sh latch
		//	get_latch(cur_bkt);
		cur_bkt->read_item(key, item);
		// 3. release the latch
		//	release_latch(cur_bkt);
		return rc;
	}

	RC index_read(idx_key_t key, itemid_t*& item,
		int part_id = -1, int thd_id = 0)
	{
		uint64_t bkt_idx = hash(key);
		assert(bkt_idx < _bucket_cnt_per_part);
		BucketHeader* cur_bkt = &_buckets[part_id][bkt_idx];
		RC rc = RCOK;
		// 1. get the sh latch
		//	get_latch(cur_bkt);
		cur_bkt->read_item(key, item);
		// 3. release the latch
		//	release_latch(cur_bkt);
		return rc;
	}

private:
	void
		get_latch(BucketHeader* bucket)
	{
		while (!ATOM_CAS(bucket->locked, false, true))
		{
		}
	}

	void
		release_latch(BucketHeader* bucket)
	{
		bool ok = ATOM_CAS(bucket->locked, true, false);
		assert(ok);
	}

	// TODO implement more complex hash function
	uint64_t hash(idx_key_t key) { return key % _bucket_cnt_per_part; }

	BucketHeader** _buckets;
	uint64_t _bucket_cnt;
	uint64_t _bucket_cnt_per_part;
};
