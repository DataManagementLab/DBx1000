
DBMS BENCHMARK
------------------------------
== General Features ==

  dbms is a OLTP database benchmark with the following features.
  
  1. Seven different concurrency control algorithms are supported.
    DL_DETECT[1]	: deadlock detection 
	NO_WAIT[1]		: no wait two phase locking
	WAIT_DIE[1]		: wait and die two phase locking
	TIMESTAMP[1]	: basic T/O
	MVCC[1]			: multi-version T/O
	HSTORE[3]		: H-STORE
	OCC[2]			: optimistic concurrency control

  [1] Phlip Bernstein, Nathan Goodman, "Concurrency Control in Distributed Database Systems", Computing Surveys, June 1981
  [2] H.T. Kung, John Robinson, "On Optimistic Methods for Concurrency Control", Transactions on Database Systems, June 1981
  [3] R. Kallman et al, "H-Store: a High-Performance, Distributed Main Memory Transaction Processing System,", VLDB 2008
	
  2. Two benchmarks are supported. 
    2.1 YCSB[4]
	2.2 TPCC[5] 
	  Only Payment and New Order transactions are modeled. 
	
  [4] B. Cooper et al, "Benchmarking Cloud Serving Systems with YCSB", SoCC 201
  [5] http://www.tpc.org/tpcc/ 

== Config Parameters ==

dbms benchmark has the following parameters in the config file. Parameters with a * sign should not be changed. 
Variant cofiguration based on listed preprocessor directives.

  CORE_CNT		: number of cores modeled in the system.
  PART_CNT		: number of logical partitions in the system
  THREAD_CNT	: number of threads running at the same time
  PAGE_SIZE		: memory page size
  CL_SIZE		: cache line size
  WARMUP		: number of transactions to run for warmup

  WORKLOAD		: workload supported (TPCC or YCSB)
  
  THREAD_ALLOC	: per thread allocator. 
  * MEM_PAD		: enable memory padding to avoid false sharing.
  MEM_ALLIGN	: allocated blocks are alligned to MEM_ALLIGN bytes

  PRT_LAT_DISTR	: print out latency distribution of transactions

  CC_ALG		: concurrency control algorithm
  * ROLL_BACK		: roll back the modifications if a transaction aborts.
  
  ENABLE_LATCH  : enable latching in btree index
  * CENTRAL_INDEX : centralized index structure
  * CENTRAL_MANAGER	: centralized lock/timestamp manager
  INDEX_STRCT	: data structure for index. 
  BTREE_ORDER	: fanout of each B-tree node

  DL_TIMEOUT_LOOP	: the max waiting time in DL_DETECT. after timeout, deadlock will be detected.
  TS_TWR		: enable Thomas Write Rule (TWR) in TIMESTAMP
  HIS_RECYCLE_LEN	: in MVCC, history will be recycled if they are too long.
  MAX_WRITE_SET	: the max size of a write set in OCC.

  MAX_ROW_PER_TXN	: max number of rows touched per transaction.
  QUERY_INTVL	: the rate at which database queries come
  MAX_TXN_PER_PART	: maximum transactions to run per partition.
  
  // for YCSB Benchmark
  SYNTH_TABLE_SIZE	: table size
  ZIPF_THETA	: theta in zipfian distribution (rows accessed follow zipfian distribution)
  READ_PERC		:
  WRITE_PERC	:
  SCAN_PERC		: percentage of read/write/scan queries. they should add up to 1.
  SCAN_LEN		: number of rows touched per scan query.
  PART_PER_TXN	: number of logical partitions to touch per transaction
  PERC_MULTI_PART	: percentage of multi-partition transactions
  REQ_PER_QUERY	: number of queries per transaction
  FIRST_PART_LOCAL	: with this being true, the first touched partition is always the local partition.
  
  // for TPCC Benchmark
  NUM_HW		: number of warehouses being modeled.
  PERC_PAYMENT	: percentage of payment transactions.
  DIST_PER_WARE	: number of districts in one warehouse
  MAXITEMS		: number of items modeled.
  CUST_PER_DIST	: number of customers per district
  ORD_PER_DIST	: number of orders per district
  FIRSTNAME_LEN	: length of first name
  MIDDLE_LEN	: length of middle name
  LASTNAME_LEN	: length of last name

  // !! centralized CC management should be ignored.
