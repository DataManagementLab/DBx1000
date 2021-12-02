#include "global.h"
#include "helper.h"
#include "wl.h"
#include "row.h"
#include "table.h"
#include "index_hash.h"
#include "index_btree.h"
#include "catalog.h"
#include "utils/mem_alloc.h"

RC workload::init() {
	sim_done = false;
	return RCOK;
}

RC workload::init_schema(string schema_file) {
	assert(sizeof(uint64_t) == 8);
	assert(sizeof(double) == 8);
	string line;
	

	string paths[] = { "./benchmarks/tpcc/","../benchmarks/tpcc/","../../../src/benchmarks/tpcc/", "../../benchmarks/tpcc/" };
	ifstream fin;
	for (int i = 0; i < 4; i++) {
		string filePath = paths[i] + schema_file;
		fin = ifstream(filePath);

		if (fin.is_open())
			break;
	}

	if (!fin.is_open()){
		std::cerr << "Schema configuration file " << schema_file<<" not found. Path is invalid!"<<std::endl;
		std::abort();
	}

	Catalog* schema;
	while (getline(fin, line)) {
		if (line.compare(0, 6, "TABLE=") == 0) {
			string tname;
			tname = &line[6];
			schema = (Catalog*)_mm_malloc(sizeof(Catalog), CL_SIZE);
			getline(fin, line);
			int col_count = 0;
			// Read all fields for this table.
			vector<string> lines;
			while (line.length() > 1) {
				lines.push_back(line);
				getline(fin, line);
			}

			schema->init((new std::string(tname))->c_str(), lines.size());
			assert(schema->table_name == tname);
			for (UInt32 i = 0; i < lines.size(); i++) {
				string line = lines[i];
				size_t pos = 0;
				string token;
				int elem_num = 0;
				int size = 0;
				string type;
				string name;
				while (line.length() != 0) {
					pos = line.find(",");
					if (pos == string::npos)
						pos = line.length();
					token = line.substr(0, pos);
					line.erase(0, pos + 1);
					switch (elem_num) {
					case 0: size = atoi(token.c_str()); break;
					case 1: type = token; break;
					case 2: name = token; break;
					default: assert(false);
					}
					elem_num++;
				}
				assert(elem_num == 3);
				schema->add_col((char*)name.c_str(), size, (char*)type.c_str());
				col_count++;
			}
			table_t* cur_tab = (table_t*)_mm_malloc(sizeof(table_t), CL_SIZE);
			cur_tab->init(schema);
			tables[tname] = cur_tab;
		}
		else if (!line.compare(0, 6, "INDEX=")) {
			string iname;
			iname = &line[6];
			getline(fin, line);

			vector<string> items;
			string token;
			size_t pos;
			while (line.length() != 0) {
				pos = line.find(",");
				if (pos == string::npos)
					pos = line.length();

				token = line.substr(0, pos);
				items.push_back(token);
				line.erase(0, pos + 1);
			}

			string tname(items[0]);
#if INDEX_STRUCT == IDX_HASH_PERFECT
			if (iname == "CUSTOMER_LAST_IDX")
			{
				IndexHash* index = (IndexHash*)_mm_malloc(sizeof(IndexHash), 64);
				new(index) IndexHash();
				int part_cnt = (CENTRAL_INDEX) ? 1 : g_part_cnt;
#if TPCC_ALL_TXN
				part_cnt *= DIST_PER_WARE;
#endif  // TPCC_ALL_TXN
#if WORKLOAD == YCSB
				index->init(part_cnt, tables[tname], g_synth_table_size * 2);
#elif WORKLOAD == TPCC
				assert(tables[tname] != NULL);
				index->init(part_cnt, tables[tname], stoull(items[1]) * part_cnt);
#endif
				indexes[iname] = reinterpret_cast<INDEX*>(index);

				continue;

			}
#endif
			INDEX* index = (INDEX*)_mm_malloc(sizeof(INDEX), 64);
			new(index) INDEX();
			int part_cnt = (CENTRAL_INDEX) ? 1 : g_part_cnt;
//#if TPCC_ALL_TXN
			if (iname == "ORDER-LINE_IDX" || iname == "ORDER_IDX")
				part_cnt *= DIST_PER_WARE;
//#endif  // TPCC_ALL_TXN
			if (tname == "ITEM")
#if REPLICATE_ITEM
				part_cnt = MAX_SOCKETS;
#else
				part_cnt = 1;
#endif  // REPLICATE_ITEM
#if INDEX_STRUCT == IDX_HASH || INDEX_STRUCT == IDX_HASH_PERFECT
#if WORKLOAD == YCSB
			index->init(part_cnt, tables[tname], g_synth_table_size * 2);
#elif WORKLOAD == TPCC
			assert(tables[tname] != NULL);
			index->init(part_cnt, tables[tname], stoull(items[1]) * part_cnt);
#endif

#else
			index->init(part_cnt, tables[tname]);
#endif
			indexes[iname] = index;
		}
	}

	fin.close();
	return RCOK;
}

void workload::index_insert(string index_name, uint64_t key, row_t* row, bool no_collision) {
	assert(false);
	INDEX* index = (INDEX*)indexes[index_name];
	index_insert(index, key, row, no_collision);
}

void workload::index_insert(INDEX* index, uint64_t key, row_t* row, int64_t part_id, bool no_collision) {
	uint64_t pid = part_id;
	if (part_id == -1)
		pid = get_part_id(row);
	itemid_t* m_item =
		(itemid_t*)mem_allocator.alloc(sizeof(itemid_t), pid);
	m_item->init();
	m_item->type = DT_row;
	m_item->location = row;
	m_item->valid = true;

	RC rc_value = index->index_insert(key, m_item, pid, no_collision);
	assert(rc_value == RCOK);

#if INDEX_STRUCT == IDX_HASH_PERFECT
	mem_allocator.free(m_item, sizeof(m_item));
#endif  // INDEX_STRUCT
}

#if INDEX_STRUCT == IDX_HASH_PERFECT
void workload::index_insert(IndexHash* index, uint64_t key, row_t* row, int64_t part_id, bool no_collision) {
	uint64_t pid = part_id;
	if (part_id == -1)
		pid = get_part_id(row);
	itemid_t* m_item =
		(itemid_t*)mem_allocator.alloc(sizeof(itemid_t), pid);
	m_item->init();
	m_item->type = DT_row;
	m_item->location = row;
	m_item->valid = true;

	RC rc_value = index->index_insert(key, m_item, pid);
	assert(rc_value == RCOK);
}
#endif  // IDX_HASH_PERFECT

