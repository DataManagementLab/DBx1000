#include "global.h"
#include "utils/mem_alloc.h"
#include "stats.h"
#include "dl_detect.h"
#include "manager.h"
#include "query.h"
#include "plock.h"
#include "occ.h"
#include "vll.h"

mem_alloc mem_allocator;
Stats stats;
Manager* glob_manager;
Query_queue* query_queue;

#if CC_ALG == HSTORE
Plock part_lock_man;
#elif CC_ALG == DL_DETECT
DL_detect dl_detector;
#elif CC_ALG == OCC
OptCC occ_man;
#elif CC_ALG == VLL
VLLMan vll_man;
#endif 

std::atomic_bool warmup_finish = false;
std::atomic_bool enable_thread_mem_pool = false;
pthread_barrier_t warmup_bar;
#ifndef NOGRAPHITE
carbon_barrier_t enable_barrier;
#endif

//retrieve configuration values from parameters specifiec in selected config file (variant_config)

ts_t g_abort_penalty = ABORT_PENALTY;
bool g_central_man = CENTRAL_MAN;
UInt32 g_ts_alloc = TS_ALLOC;
bool g_key_order = KEY_ORDER;
bool g_no_dl = NO_DL;
ts_t g_timeout = TIMEOUT;
ts_t g_dl_loop_detect = DL_LOOP_DETECT;
bool g_ts_batch_alloc = TS_BATCH_ALLOC;
UInt32 g_ts_batch_num = TS_BATCH_NUM;

bool g_part_alloc = PART_ALLOC;
bool g_mem_pad = MEM_PAD;

ts_t g_query_intvl = QUERY_INTVL;
UInt32 g_part_per_txn = PART_PER_TXN;
double g_perc_multi_part = PERC_MULTI_PART;
double g_read_perc = READ_PERC;
double g_write_perc = WRITE_PERC;
double g_zipf_theta = ZIPF_THETA;
bool g_prt_lat_distr = PRT_LAT_DISTR;
//UInt32 g_part_cnt = PART_CNT;

UInt32 g_virtual_part_cnt = VIRTUAL_PART_CNT;
UInt32 g_thread_cnt = THREAD_CNT;
UInt64 g_synth_table_size = SYNTH_TABLE_SIZE;
UInt32 g_req_per_query = REQ_PER_QUERY;
UInt32 g_field_per_tuple = FIELD_PER_TUPLE;
UInt32 g_init_parallelism = INIT_PARALLELISM;

UInt32 g_num_wh = NUM_WH;
double g_perc_payment = PERC_PAYMENT;
bool g_wh_update = WH_UPDATE;
char* output_file = NULL;
UInt32 g_perc_remote_payment = 15;
UInt32 g_perc_remote_neworder = 1;
bool g_enforce_numa_distance = false;
bool g_enforce_numa_distance_remote_only = true;
UInt64 g_numa_distance = 0;
std::map<UInt32,std::map<UInt32, UInt32>> g_numa_dist_matrix;
UInt64 g_threads_per_socket = 0;

bool g_calc_size_only = false;

map<string, string> g_params;
