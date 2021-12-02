//#undef NDEBUG

#include <iostream>
#include <cassert>

#include "index_hash_perfect.h"


bool itemid_t::operator==(const itemid_t& other) const {
	return (type == other.type && location == other.location);
}

bool itemid_t::operator!=(const itemid_t& other) const {
	return !(*this == other);
}

void itemid_t::init() {
	valid = false;
	location = 0;
	next = NULL;
}

int main(int, char**) {
    uint64_t bucket_cnt = 2;
    int part_cnt = 1;

    IndexHashPerfect index;
    index.init(bucket_cnt, part_cnt);

    {
        auto item = new itemid_t;
        item->init();
        item->location = (void*) 1;
        item->valid = true;
        item->type = Data_type::DT_row;

        index.index_insert(1, item, 0);

        itemid_t *rItem;
        index.index_read(1, rItem, 0, 0);

        assert(item->location == rItem->location);
        assert(item->next == NULL);

        std::cout << (uintptr_t)rItem->location << std::endl;
    }

    {
        auto item = new itemid_t;
        item->init();
        item->location = (void*) 2;
        item->valid = true;
        item->type = Data_type::DT_row;

        index.index_insert(1, item, 0);

        itemid_t *rItem;
        index.index_read(1, rItem, 0, 0);

        assert(item->location == rItem->location);
        assert(rItem->next != NULL);
        assert(rItem->next->location == (void*)1);

        std::cout << (uintptr_t)rItem->next->location << std::endl;
    }

    {
        auto item = new itemid_t;
        item->init();
        item->location = (void*) 3;
        item->valid = true;
        item->type = Data_type::DT_row;

        index.index_insert(2, item, 0);

        itemid_t *rItem;
        index.index_read(2, rItem, 0, 0);

        assert(item->location == rItem->location);
        assert(item->next == NULL);
        std::cout << (uintptr_t)rItem->location << std::endl;

        index.index_read(1, rItem, 0, 0);
        assert(rItem->location == (void*)2);
        assert(rItem->next != NULL);
        assert(rItem->next->location == (void*)1);

        std::cout << (uintptr_t)rItem->location << std::endl;
    }

    {
        auto item = new itemid_t;
        item->init();
        item->location = (void*) 4;
        item->valid = true;
        item->type = Data_type::DT_row;

        index.index_insert(2, item, 0);

        itemid_t *rItem;
        index.index_read(2, rItem, 0, 0);

        assert(item->location == rItem->location);
        assert(rItem->next != NULL);
        assert(rItem->next->location == (void*)3);
        std::cout << (uintptr_t)rItem->next->location << std::endl;

        index.index_read(1, rItem, 0, 0);
        assert(rItem->location == (void*)2);
        assert(rItem->next != NULL);
        assert(rItem->next->location == (void*)1);

        std::cout << (uintptr_t)rItem->next->location << std::endl;
    }

    {
        itemid_t *item11, *item12, *item21, *item22;

        index.index_read(1, item11, 0, 0);
        item12 = item11->next;
        index.index_read(2, item21, 0, 0);
        item22 = item21->next;

        assert(item11->location == (void*)2);
        assert(item12->location == (void*)1);
        assert(item21->location == (void*)4);
        assert(item22->location == (void*)3);

        item12 = item11;
        index.index_delete(1, item11, NULL, 0, 0);

        itemid_t *rItem;
        index.index_read(1, rItem, 0, 0);
        assert(rItem->location == (void*)1);

        index.index_delete(1, item12, NULL, 0, 0);
        index.index_read(1, rItem, 0, 0);
        assert(rItem == NULL);

        index.index_delete(2, item22, item21, 0, 0);
        index.index_read(2, rItem, 0, 0);
        assert(rItem->location == (void*)4);
        assert(item21->location == (void*)4);
        assert(rItem->next == NULL);
        
        index.index_delete(2, item21, item21, 0, 0);
        index.index_read(2, rItem, 0, 0);
        assert(rItem == NULL);
    }
}