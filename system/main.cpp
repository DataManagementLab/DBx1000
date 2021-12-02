//#define USE_CACHE_METER
//#define USE_CACHE_METER_TMAM
//#define USE_HUBSTATS

//#define USE_VTUNE

//#define USE_WAIT_ENTER

#include <omp.h>

#include "global.h"
#include "ycsb.h"
#include "tpcc.h"
#include "test.h"
#include "thread.h"
#include "manager.h"
#include "mem_alloc.h"
#include "query.h"
#include "plock.h"
#include "occ.h"
#include "vll.h"

#ifdef USE_CACHE_METER
#include "CacheMeter.hpp"
#endif  // USE_CACHE_METER
#include "HubStats.hpp"

#ifdef USE_VTUNE
#include "ittnotify.h"
#endif  // USE_VTUNE

void* f(void*);

thread_t** m_thds;

// defined in parser.cpp
void parser(int argc, char* argv[]);

int main(int argc, char* argv[])
{
#ifdef USE_HUBSTATS
	HubStats hub;
#endif  // USE_HUBSTATS

	parser(argc, argv);

	uint64_t thd_cnt = g_thread_cnt;
	omp_set_num_threads(thd_cnt);

	mem_allocator.init(g_part_cnt, MEM_SIZE / g_part_cnt);
	stats.init();
	glob_manager = (Manager*)_mm_malloc(sizeof(Manager), 64);
	glob_manager->init();
	
	printf("mem_allocator initialized!\n");
	workload* m_wl;
	switch (WORKLOAD) {
	case YCSB:
		m_wl = new ycsb_wl;
		break;
	case TPCC:
		m_wl = new tpcc_wl;
		break;
	case TEST:
		m_wl = new TestWorkload;
		((TestWorkload*)m_wl)->tick();
		break;
	default:
		assert(false);
	}
	m_wl->init();
	printf("workload initialized!\n");

	//pthread_t p_thds[thd_cnt - 1];
	m_thds = new thread_t * [thd_cnt];
	for (uint32_t i = 0; i < thd_cnt; i++)
		m_thds[i] = (thread_t*)_mm_malloc(sizeof(thread_t), 64);
	// query_queue should be the last one to be initialized!!!
	// because it collects txn latency
	query_queue = (Query_queue*)_mm_malloc(sizeof(Query_queue), 64);
	if (WORKLOAD != TEST)
		query_queue->init(m_wl);
	pthread_barrier_init(&warmup_bar, NULL, g_thread_cnt);
	printf("query_queue initialized!\n");
	
	// initialize CC algorithms
#if CC_ALG == HSTORE
	part_lock_man.init();
#elif CC_ALG == OCC
	occ_man.init();
#elif CC_ALG == VLL
	vll_man.init();
#elif CC_ALG == DL_DETECT
	dl_detector.init();
#endif

	/* for (uint32_t i = 0; i < thd_cnt; i++)
		m_thds[i]->init(i, m_wl); */

#ifdef USE_CACHE_METER
	std::string cache_meter_filename = "";
    if (std::getenv("CACHE_METER_RESULT_FILE") != NULL)
    {
	    cache_meter_filename = std::string(std::getenv("CACHE_METER_RESULT_FILE"));
    }
	PCMMeter::CacheMeter *cm;
	std::bitset<MAX_CORES> ycores;
#ifdef USE_CACHE_METER_STALLS
    cm = new PCMMeter::CacheMeter(PCMMeter::MeasurementType::Stalls, cache_meter_filename);
#elif defined(USE_CACHE_METER_TMAM)
    cm = new PCMMeter::CacheMeter(PCMMeter::MeasurementType::TMAM, cache_meter_filename);
#else
    cm = new PCMMeter::CacheMeter(cache_meter_filename);
#endif // USE_CACHE_METER_STALLS
#endif  // USE_CACHE_METER

#pragma omp parallel
	{
		int tId = omp_get_thread_num();

		query_queue->init_per_thread(tId);
		printf("query_queue initialized!\n");
#pragma omp barrier

		m_thds[tId]->init(tId, m_wl);

#ifdef USE_CACHE_METER

// Figure out the cores the benchmark is running on, assuming initialization sets the placement correctly!
#pragma omp critical
        {
            int cpu = sched_getcpu();

            ycores.set(cpu);
        }
#pragma omp barrier
#pragma omp single
        cm->filterCores(ycores);
#endif

		printf("%d: Starting\n", tId);
		if (WARMUP > 0) {
			/*
			   printf("WARMUP start!\n");
			   for (uint32_t i = 0; i < thd_cnt - 1; i++) {
			   uint64_t vid = i;
			   pthread_create(&p_thds[i], NULL, f, (void *)vid);
			   }
			   f((void *)(thd_cnt - 1));
			   for (uint32_t i = 0; i < thd_cnt - 1; i++)
			   pthread_join(p_thds[i], NULL);
			   */
			f((void*)(static_cast<uint64_t>(omp_get_thread_num())));
#pragma omp barrier
#pragma omp master
			{
				printf("WARMUP finished!\n");

#ifdef USE_WAIT_ENTER
				std::cout << "Press enter to continue..." << std::endl;
				std::cin.get();
#endif  // USE_WAIT_ENTER

				// Resume Vtune
#ifdef USE_VTUNE
				__itt_resume();
#endif  // USE_VTUNE

#ifdef USE_CACHE_METER
                cm->Start();
#endif  // USE_CACHE_METER
			}
		}
#pragma omp master
		warmup_finish = true;
		//pthread_barrier_init( &warmup_bar, NULL, g_thread_cnt );
#ifndef NOGRAPHITE
		CarbonBarrierInit(&enable_barrier, g_thread_cnt);
#endif
		//pthread_barrier_init( &warmup_bar, NULL, g_thread_cnt );

		// spawn and run txns again.
		int64_t starttime;
#pragma omp master
		{

#ifdef USE_HUBSTATS
			hub.Start();
#endif  // USE_HUBSTATS
			starttime = get_server_clock();
		}
#pragma omp barrier
		/*
		   for (uint32_t i = 0; i < thd_cnt - 1; i++) {
		   uint64_t vid = i;
		   pthread_create(&p_thds[i], NULL, f, (void *)vid);
		   }
		   f((void *)(thd_cnt - 1));
		   for (uint32_t i = 0; i < thd_cnt - 1; i++)
		   pthread_join(p_thds[i], NULL);
		   */

		f((void*)(static_cast<uint64_t>(omp_get_thread_num())));
#pragma omp barrier
#pragma omp master
		{
			int64_t endtime = get_server_clock();


#ifdef USE_CACHE_METER
			cm->Stop(true);
#endif // USE_CACHE_METER

#ifdef USE_HUBSTATS
			hub.Stop();
#endif  // USE_HUBSTATS

			// Pause VTune
#ifdef USE_VTUNE
			__itt_pause();
#endif  // USE_VTUNE
#ifdef USE_WAIT_ENTER
				std::cout << "Press enter to continue..." << std::endl;
				std::cin.get();
#endif  // USE_WAIT_ENTER

			if (WORKLOAD != TEST) {
				printf("PASS! SimTime = %ld\n", endtime - starttime);
				if (STATS_ENABLE)
					stats.print();
			}
			else {
				((TestWorkload*)m_wl)->summarize();
			}
		}
	}
	return 0;
}

// process task queue
void* f(void* id) {
	uint64_t tid = (uint64_t)id;
	m_thds[tid]->run();
	return NULL;
}
