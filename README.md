<img src="logo/dbx1000.svg" alt="DBx1000 Logo" width="60%">

-----------------

__The Full Story of 1000 Cores: A Dissection of Concurrency Control on Real(ly) Large Multi-Socket Hardware__
```
...
```

__Archived measurements__:
- Archived: https://tudatalib.ulb.tu-darmstadt.de/handle/tudatalib/3299
- Browsable: https://github.com/DataManagementLab/VLDBJ_1000_cores_measurements

We would like to express our great gratitude for publishing the initial implementation of DBx1000 as open-source making our work possible: https://github.com/yxymit/DBx1000

Setup
------------

Install build dependencies via package manager:

    gcc (at leat version 9)
    cmake (at least version 3.10)
    libnuma-devel

Install dependencies (TBB) with Git Submodule. If an errors occurs with fetching repository try `git submodule update --remote` instead of `git submodule update`. If it's still not working try out adding submodule manually (`git submodule add --force --name libs/tbb https://github.com/oneapi-src/oneTBB.git  libs/tbb/`). Check that `libs/tbb` folder contains afterwards the current version of the [TBB Github repository](https://github.com/oneapi-src/oneTBB).

    git submodule init
    git submodule update

Create build folder, run cmake and build the database

    mkdir build
    cd build
    cmake ..
    make TARGET


Execute run script in main folder (i.e. lab20-DBx1000) and pass run configuration file e.g. `./run_tpcc.sh configurations/test.conf`. The results will be written to `results/yyyy-MM-dd_hh-mm-ss`. Be carefull that the corresponding cmake targets are build. If not make sure that the CMakeLists.txt file defines the target and the target is build in the `build` dir.

    ./run_tpcc.sh CONFIG-FILE

The run script configuration file should contain the following parameters:
Name | Definition | Example Value
--- | --- | ---
`WHs`|List of different warehouse numbers | `WHs="4 512 1024"`
`threads`|List of number of threads | `threads="1 2 4 8"`
`remote_news`|List|`remote_news="0 1 5 10"`
`CCs`|List of currency control algorithms|`CCs="WAIT_DIE NO_WAIT"`
`NO_HTs`|Run w/o or w/ hyperthreading (0=hyperthreading, 1=no hyperthreading)|`NO_HTs="0 1"`
`SMALLs`|Run with small relations|`SMALLs="0 1"`

## Configuration

---

DBMS configurations can be changed in the config.h file. The configuration file is set in the first lines of CMakeList.txt file.

Categories:
- Simulation & Hardware
- Concurrency Control
    - Specific Configuration for `INDEX`
    - Specific Configuration for `DL_DETECT`
    - Specific Configuration for `TIMESTAMP`
    - Specific Configuration for `MVCC`
    - Specific Configuration for `OCC`
    - Specific Configuration for `TICTOC`
    - Specific Configuration for `TICTOC` &  `SILO`
    - Specific Configuration for `OCC` & `TICTOC` &  `SILO`
    - Specific Configuration for `HSTORE`
    - Specific Configuration for `VLL`
- Logging
- Benchmark
    - Specific Configuration for `YCSB`
    - Specific Configuration for `TPCC`
- ~~Centralized CC Management - Should be ignored~~
- TestsCases
- Debug Info
- Spin Waiting
- Thread Alloc Hawk
- Misc


---

### Simulation & Hardware
Configuration | Definition | Values
--- | --- | ---
`THREAD_CNT			`|Number of worker threads running in the database| `int`	
`PART_CNT			`|Number of logical partitions in the system|	`int`	
`VIRTUAL_PART_CNT	`|Each transaction only accesses 1 virtual partition. But the lock/ts manager and index are not aware of such partitioning. VIRTUAL_PART_CNT describes the request distribution and is only used to generate queries. For HSTORE, VIRTUAL_PART_CNT should be the same as PART_CNT.|	`int`
`PAGE_SIZE			`|Memory page size |`int`		
`CL_SIZE			`|Cache Line size of hardware|`int`		
`CPU_FREQ 			`|CPU Frequency used to get accurate timing info |`int` (in GHz)		
`WARMUP				`|Number of transactions to run for warmup|`int`		
`WORKLOAD 			`|Supported workloads include YCSB and TPCC | `YCSB`<br>`TPCC`<br>`TEST`	
`PRT_LAT_DISTR		`|Print the transaction latency distribution|`true\|false`		
`STATS_ENABLE		`|Print statistics|	`true\|false`
`TIME_ENABLE		`|Use realtime for measurements, otherwise use fake time.|	`true\|false`	
`MEM_ALLIGN			`|Allocated blocks are aligned to MEM_ALLIGN bytes| `int` (in bytes)
`THREAD_ARENA_SIZE	`||	`int`	
`MEM_PAD 			`|Enable memory padding to avoid false sharing|	`true\|false`
`PART_ALLOC 		`||	`true\|false`
~~`MEM_SIZE			`~~|Deprecated (never used)|	~~`int`~~


---

### Memory Allocation
Configuration | Definition | Values
--- | --- | ---
`MALLOC_TYPE` | Memory Allocator to use | `MEM_ALLOC`<br>`HAWK_ALLOC`<br> `JE_MALLOC`(future)

#### Specific Configuration for `MEM_ALLOC`
Configuration | Definition | Values
--- | --- | ---
`THREAD_ALLOC		`|Per thread allocator. If `false` std::malloc is used.|	`true\|false`	
`NO_FREE			`|Free no memory |	`true\|false`

#### Specific Configuration for `HAWK_ALLOC`
Configuration | Definition | Values
--- | --- | ---
`THREAD_ALLOC_HAWK_INSERT`||`true\|false`
`HAWK_ALLOC_CAPACITY	 `| Hawked memory capacity. |`int`	

---

### Concurrency Control
Configuration | Definition | Values
--- | --- | ---
`CC_ALG 				`| Concurrency Control Algorithm |`NO_WAIT` No-wait two phase locking<br>`WAIT_DIE` Wait-and-die two phase locking<br>`DL_DETECT`<br>`TIMESTAMP` basic T/O<br>`MVCC` multi-version T/O<br>`HSTORE` H-Store<br>`OCC` optimistic concurrency control<br>`TICTOC`<br>`SILO`<br>`VLL`<br>`HEKATON`
`ISOLATION_LEVEL 		`|Isolation Level|`SERIALIZABLE`<br>`SNAPSHOT`<br>`REPEATABLE_READ`
`KEY_ORDER				`|All transactions acquire tuples according to the primary key order.|`true\|false`
`ROLL_BACK				`|Roll back the modifications if a transaction aborts|`true\|false`
`CENTRAL_MAN			`|Per-row lock/ts management or central lock/ts management|`true\|false`
`BUCKET_CNT				`||`int`
`ABORT_PENALTY 			`||`int`
`ABORT_BUFFER_SIZE		`||`int`
`ABORT_BUFFER_ENABLE	`||`true\|false`

#### Specific Configuration for `INDEX`
Configuration | Definition | Values
--- | --- | ---
`CENTRAL_INDEX			`|Centralized index structure (part count for index initialization is 1)|`true\|false`
`INDEX_STRUCT			`|Data structure for index|`IDX_HASH` <br>`IDX_BTREE`<br>`IDX_HASH_PERFECT`
`ENABLE_LATCH			`|Enable latching in B-tree index|`true\|false`
`BTREE_ORDER 			`|Fanout of each B-tree node|`int`
~~`CENTRAL_MANAGER 		`~~|Deprecated (never used) <br>Centralized lock/timestamp manager|~~`true\|false`~~
~~`INDEX_HASH_TYPE`~~        |Deprecated (redundancy to `INDEX_STRUCT`)|~~`IndexHash`~~
~~`INDEX_HASH_TAGGED_SIZE`~~ |Deprecated (Never used)|~~`int`~~

#### Specific Configuration for `DL_DETECT`
Configuration | Definition | Values
--- | --- | ---
`DL_LOOP_DETECT			`||`int`
`DL_LOOP_TRIAL			`||`int`
`NO_DL					`|Ignore deadlocks - no deadlock detection & handling|`true\|false`
`TIMEOUT				`||`int`

#### Specific Configuration for `TIMESTAMP`
Configuration | Definition | Values
--- | --- | ---
`TS_TWR					`|Enable Thomas Write Rule (TWR) in TIMESTAMP|`true\|false`
`TS_ALLOC				`|Timestamp allocation method|`TS_MUTEX`<br>`TS_CAS`<br>`TS_HW`<br>`TS_CLOCK`
`TS_BATCH_ALLOC			`||`true\|false`
`TS_BATCH_NUM			`||`int`

#### Specific Configuration for `MVCC`
Configuration | Definition | Values
--- | --- | ---
~~`HIS_RECYCLE_LEN`~~| ~~When read/write history is longer than HIS_RECYCLE_LEN the history should be recycled.~~|~~`int`~~
~~`MAX_PRE_REQ`~~||~~`int`~~
~~`MAX_READ_REQ`~~||~~`int`~~
`MIN_TS_INTVL			 `||`int` (in nanoseconds)

#### Specific Configuration for `OCC`
Configuration | Definition | Values
--- | --- | ---
`MAX_WRITE_SET			`|The max size of a write set in OCC|`int`
`PER_ROW_VALID			`||`true\|false`

#### Specific Configuration for `TICTOC`
Configuration | Definition | Values
--- | --- | ---
`WRITE_COPY_FORM		`||`"data" \| "ptr"`
`TICTOC_MV				`||`true\|false`
`WR_VALIDATION_SEPARATE	`||`true\|false`
`WRITE_PERMISSION_LOCK	`||`true\|false`
`ATOMIC_TIMESTAMP		`||`"false"`???

#### Specific Configuration for `TICTOC` &  `SILO`
Configuration | Definition | Values
--- | --- | ---
`VALIDATION_LOCK		`||`"no-wait" \| "waiting"`
`PRE_ABORT				`||`"true"`???
`ATOMIC_WORD			`||`true\|false`

#### Specific Configuration for `OCC` & `TICTOC` &  `SILO`
Configuration | Definition | Values
--- | --- | ---
`BOUNDED_SPINNING`|Deadlock prevention by spinning fixed number, no write-set ordering|`true\|false`

#### Specific Configuration for `HSTORE`
Configuration | Definition | Values
--- | --- | ---
`HSTORE_LOCAL_TS		`|When set to true, hstore will not access the global timestamp. This is fine for single partition transactions.|`true\|false`

#### Specific Configuration for `VLL`
Configuration | Definition | Values
--- | --- | ---
`TXN_QUEUE_SIZE_LIMIT	`||`int`

---

### Logging
Configuration | Definition | Values
--- | --- | ---
`LOG_COMMAND`||`true\|false`
`LOG_REDO`||`true\|false`
`LOG_BATCH_TIME`||`int` (in ms)

---

### Benchmark
Configuration | Definition | Values
--- | --- | ---
`MAX_ROW_PER_TXN	`|Max number of rows touched per transaction|`int`
`QUERY_INTVL 		`|The rate at which database queries come|`int`
`MAX_TXN_PER_PART 	`|Number of transactions to run per thread per partition|`int`
`FIRST_PART_LOCAL 	`|with this being true, the first tuoched partition is always the local partition.|`true\|false`
`MAX_TUPLE_SIZE     `||`int` (in bytes)

#### Specific Configuration for `YCSB`
Configuration | Definition | Values
--- | --- | ---
`INIT_PARALLELISM	`||`int`
`SYNTH_TABLE_SIZE 	`|Table size|`int`
`ZIPF_THETA 		`|Theta in zipfian distribution (rows acceessed follow zipfian distribution)|`float`
`READ_PERC 			`|percentage of read queries (read/write/scan shoult add up to 1)|`float`
`WRITE_PERC 		`|percentage of write queries (read/write/scan shoult add up to 1)|`float`
`SCAN_PERC 			`|percentage of scan queries (read/write/scan shoult add up to 1)|`float`
`SCAN_LEN			`|Number of rows touched per scan query|`int`
`PART_PER_TXN 		`|Number of logical partitions to touch per transaction|`int`
`PERC_MULTI_PART	`|Percentage of multi-partition transaction|`int`
`REQ_PER_QUERY		`|Number of queries per transaction|`int`
`FIELD_PER_TUPLE	`||`int`

#### Specific Configuration for `TPC-C`
Configuration | Definition | Values
--- | --- | ---
`TPCC_SMALL			`|For large warehouse count, the tables do not fit in memory. Small tpcc schemas shrink the table size.|`true\|false`
`TPCC_ALL_TXN       `|true: Use all transaction types, false: Use only New-Order and Payment Transaction Types | `true\|false`
`TPCC_ACCESS_ALL 	`|Some of the transactions read the data but never use them. If TPCC_ACCESS_ALL == false, then these parts of the transactions are not modeled.|`true\|false`
`WH_UPDATE			`||`true\|false`
`NUM_WH 			`|Number of warehouses being modeled|`int`	

*Txn Type Definition*
Configuration | Definition | Values
--- | --- | ---
`PERC_PAYMENT 	    `|Percentage of payment transactions|`float`
`FIRSTNAME_MINLEN   `||`int`
`FIRSTNAME_LEN 	    `|length of first name|`int`
`LASTNAME_LEN 	    `|length of last name|`int`
`DIST_PER_WARE	    `|number of districts in one warehouse|`int`

---

### ~~Centralized CC Management - Should be ignored~~

Configuration | Definition | Values
--- | --- | ---
`MAX_LOCK_CNT`||`int`
`TSTAB_SIZE  `||`int`
`TSTAB_FREE  `||`int`
`TSREQ_FREE  `||`int`
`MVHIS_FREE  `||`int`
`SPIN        `||`true\|false`

---

### TestCases

Configuration | Definition | Values
--- | --- | ---
`TEST_ALL`||`true\|false`

---

### Debug Info

Configuration | Definition | Values
--- | --- | ---
`WL_VERB		`||`true\|false`
`IDX_VERB		`||`true\|false`
`VERB_ALLOC		`||`true\|false`
`DEBUG_LOCK		`||`true\|false`
`DEBUG_TIMESTAMP`||`true\|false`
`DEBUG_SYNTH	`||`true\|false`
`DEBUG_ASSERT	`||`true\|false`
`DEBUG_CC		`||`true\|false`

---

### Spin Waiting

Configuration | Definition | Values
--- | --- | ---
`SPIN_WAIT_BACKOFF 	 `|| `true\|false`
`SPIN_WAIT_BACKOFF_MAX`|Longest back-off time|`int`

---

### Misc

Configuration | Definition | Values
--- | --- | ---
`NO_FREE`| | `true\|false`
`DEBUG_CATALOG`||`true\|false`
`BYPASS_CATALOG	`| Bypass the catalog and use function parameters instead. |`true\|false`
`REPLICATE_ITEM	`||`true\|false`
`MAX_SOCKETS	`||`int`
`ALLOW_READ_ONLY`||`true\|false`
`BULK_INSERT	`||`true\|false`
`BATCHED_ACCESS	`||`true\|false`
`LATCH_T	 `|Type of the latches|`L_PTHREAD`<br>`L_QUEUE`<br>`L_QUEUE_RW`<br>`L_SPIN`<br>`L_SPIN_RW`<br>`L_SPEC`<br>`L_SPEC_RW`		
`SCOPED_LATCH`|Use scoped latches.|`true\|false`	
`SCOPED_LATCH_T`|Specific type of the scoped latches.|`L_PTHREAD`<br>`L_QUEUE`<br>`L_QUEUE_RW`<br>`L_SPIN`<br>`L_SPIN_RW`<br>`L_SPEC`<br>`L_SPEC_RW`
`BOUNDED_SPINNING_CYCLES`|Number of spinning cycles for exponential back-off for bounded spinning|`int`

Outputs
-------

txn_cnt: The total number of committed transactions. This number is close to but smaller than THREAD_CNT * MAX_TXN_PER_PART. When any worker thread commits MAX_TXN_PER_PART transactions, all the other worker threads will be terminated. This way, we can measure the steady state throughput where all worker threads are busy.

abort_cnt: The total number of aborted transactions. A transaction may abort multiple times before committing. Therefore, abort_cnt can be greater than txn_cnt.

run_time: The aggregated transaction execution time (in seconds) across all threads. run_time is approximately the program execution time * THREAD_CNT. Therefore, the per-thread throughput is txn_cnt / run_time and the total throughput is txn_cnt / run_time * THREAD_CNT.

time_{wait, ts_alloc, man, index, cleanup, query}: Time spent on different components of DBx1000. All numbers are aggregated across all threads.

time_abort: The time spent on transaction executions that eventually aborted.

latency: Average latency of transactions.
