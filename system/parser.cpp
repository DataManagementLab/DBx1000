#include "global.h"
#include "helper.h"

void print_usage() {
	printf("[usage]:\n");
	printf("\t-pINT       ; PART_CNT\n");
	printf("\t-vINT       ; VIRTUAL_PART_CNT\n");
	printf("\t-tINT       ; THREAD_CNT\n");
	printf("\t-qINT       ; QUERY_INTVL\n");
	printf("\t-dINT       ; PRT_LAT_DISTR\n");
	printf("\t-aINT       ; PART_ALLOC (0 or 1)\n");
	printf("\t-mINT       ; MEM_PAD (0 or 1)\n");
	printf("\t-GaINT      ; ABORT_PENALTY (in ms)\n");
	printf("\t-GcINT      ; CENTRAL_MAN\n");
	printf("\t-GtINT      ; TS_ALLOC\n");
	printf("\t-GkINT      ; KEY_ORDER\n");
	printf("\t-GnINT      ; NO_DL\n");
	printf("\t-GoINT      ; TIMEOUT\n");
	printf("\t-GlINT      ; DL_LOOP_DETECT\n");	
	printf("\t-GbINT      ; TS_BATCH_ALLOC\n");
	printf("\t-GuINT      ; TS_BATCH_NUM\n");

	printf("\t-o STRING   ; output file\n\n");
	printf("  [YCSB]:\n");
	printf("\t-cINT       ; PART_PER_TXN\n");
	printf("\t-eINT       ; PERC_MULTI_PART\n");
	printf("\t-rFLOAT     ; READ_PERC\n");
	printf("\t-wFLOAT     ; WRITE_PERC\n");
	printf("\t-zFLOAT     ; ZIPF_THETA\n");
	printf("\t-sINT       ; SYNTH_TABLE_SIZE\n");
	printf("\t-RINT       ; REQ_PER_QUERY\n");
	printf("\t-fINT       ; FIELD_PER_TUPLE\n");
	printf("  [TPCC]:\n");
	printf("\t-nINT       ; NUM_WH\n");
	printf("\t-TpFLOAT    ; PERC_PAYMENT\n");
	printf("\t-TuINT      ; WH_UPDATE\n");
	printf("\t-TrNINT    ; PERC_REMOTE_NEW-ORDER\n");
	printf("\t-TrPINT    ; PERC_REMOTE_PAYMENT\n");
	printf("\t-TnDInt ; Enforce NUMA distance\n");
	printf("\t-TnMINT:[INT=INT,...]; Map socket to sockets with given distance\n");
	printf("\t-TnTINT;  Number of threads per socket");

	printf("  [TEST]:\n");
	printf("\t-Ar         ; Test READ_WRITE\n");
	printf("\t-Ac         ; Test CONFLIT\n");
}

void print_configuration() {
	printf("\t-pINT       ; PART_CNT:\t%u\n", g_part_cnt);
	printf("\t-vINT       ; VIRTUAL_PART_CNT:\t%u\n", g_virtual_part_cnt);
	printf("\t-tINT       ; THREAD_CNT:\t%u\n", g_thread_cnt);
	printf("\t-qINT       ; QUERY_INTVL:\t%lu\n", g_query_intvl);
	printf("\t-dINT       ; PRT_LAT_DISTR:\t%u\n", g_prt_lat_distr);
	printf("\t-aINT       ; PART_ALLOC:\t%u\n", g_part_alloc);
	printf("\t-mINT       ; MEM_PAD:\t%u\n", g_mem_pad);
	printf("\t-GaINT      ; ABORT_PENALTY:\t%lu\n", g_abort_penalty);
	printf("\t-GcINT      ; CENTRAL_MAN:\t%u\n", g_central_man);
	printf("\t-GtINT      ; TS_ALLOC:\t%u\n", g_ts_alloc);
	printf("\t-GkINT      ; KEY_ORDER:\t%u\n", g_key_order);
	printf("\t-GnINT      ; NO_DL:\t%u\n", g_no_dl);
	printf("\t-GoINT      ; TIMEOUT:\t%lu\n", g_timeout);
	printf("\t-GlINT      ; DL_LOOP_DETECT:\t%lu\n", g_dl_loop_detect);

	printf("\t-GbINT      ; TS_BATCH_ALLOC:\t%u\n", g_ts_batch_alloc);
	printf("\t-GuINT      ; TS_BATCH_NUM:\t%u\n", g_ts_batch_num);

	printf("\t            ; SPIN_WAIT_BACKOFF:\t%u\n", SPIN_WAIT_BACKOFF);
	printf("\t            ; SPIN_WAIT_BACKOFF_MAX:\t%u\n", SPIN_WAIT_BACKOFF_MAX);
	printf("\t            ; BOUNDED_SPINNING:\t%u\n", BOUNDED_SPINNING);
	printf("\t            ; BOUNDED_SPINNING_CYCLES:\t%u\n", BOUNDED_SPINNING_CYCLES);

	printf("\t-o STRING   ; output file:\t%s\n\n", output_file);
	printf("  [YCSB]:\n");
	printf("\t-cINT       ; PART_PER_TXN:\t%u\n", g_part_per_txn);
	printf("\t-eINT       ; PERC_MULTI_PART:\t%lf\n", g_perc_multi_part);
	printf("\t-rFLOAT     ; READ_PERC:\t%f\n", g_read_perc);
	printf("\t-wFLOAT     ; WRITE_PERC:\t%f\n", g_write_perc);
	printf("\t-zFLOAT     ; ZIPF_THETA:\t%f\n", g_zipf_theta);
	printf("\t-sINT       ; SYNTH_TABLE_SIZE:\t%lu\n", g_synth_table_size);
	printf("\t-RINT       ; REQ_PER_QUERY:\t%u\n", g_req_per_query);
	printf("\t-fINT       ; FIELD_PER_TUPLE:\t%u\n", g_field_per_tuple);
	printf("  [TPCC]:\n");
	printf("\t-nINT       ; NUM_WH:\t%u\n", g_num_wh);
	printf("\t-TpFLOAT    ; PERC_PAYMENT:\t%f\n", g_perc_payment);
	printf("\t-TuINT      ; WH_UPDATE:\t%u\n", g_wh_update);
	printf("\t-TrNINT    ; PERC_REMOTE_NEW-ORDER:\t%u\n", g_perc_remote_neworder);
	printf("\t-TrPINT    ; PERC_REMOTE_PAYMENT:\t%u\n", g_perc_remote_payment);
	printf("\t-TnDInt ; Enforce NUMA distance:\t%lu\n", g_numa_distance);
	printf("\t-TnR ; Enforce NUMA distance only for remote transactions:\t%lu\n", g_enforce_numa_distance_remote_only);
	printf("\t-TnMINT:[INT=INT,...]; Map socket to sockets with given distance:\n");
	for(const auto& node : g_numa_dist_matrix) {
		printf("\t\t%u:", node.first);
		for(const auto& dist : node.second) {
			printf("%u=%u,", dist.first, dist.second);
		}
		printf("\n");
	}
	printf("\t-TnTINT;  Number of threads per socket:\t%lu\n", g_threads_per_socket);
	printf("  [TEST]:\n");
	printf("\t-Ar         ; Test READ_WRITE:\t%u\n", g_test_case == READ_WRITE);
	printf("\t-Ac         ; Test CONFLIT:\t%u\n", g_test_case == CONFLICT);
}

void parser(int argc, char* argv[]) {
	g_params["abort_buffer_enable"] = ABORT_BUFFER_ENABLE ? "true" : "false";
	g_params["write_copy_form"] = WRITE_COPY_FORM;
	g_params["validation_lock"] = VALIDATION_LOCK;
	g_params["pre_abort"] = PRE_ABORT;
	g_params["atomic_timestamp"] = ATOMIC_TIMESTAMP;

	for (int i = 1; i < argc; i++) {
		assert(argv[i][0] == '-');
		if (argv[i][1] == 'a')
			g_part_alloc = atoi(&argv[i][2]);
		else if (argv[i][1] == 'm')
			g_mem_pad = atoi(&argv[i][2]);
		else if (argv[i][1] == 'q')
			g_query_intvl = atoi(&argv[i][2]);
		else if (argv[i][1] == 'c')
			g_part_per_txn = atoi(&argv[i][2]);
		else if (argv[i][1] == 'e')
			g_perc_multi_part = atof(&argv[i][2]);
		else if (argv[i][1] == 'r')
			g_read_perc = atof(&argv[i][2]);
		else if (argv[i][1] == 'w')
			g_write_perc = atof(&argv[i][2]);
		else if (argv[i][1] == 'z')
			g_zipf_theta = atof(&argv[i][2]);
		else if (argv[i][1] == 'd')
			g_prt_lat_distr = atoi(&argv[i][2]);
		/* else if (argv[i][1] == 'p')
			g_part_cnt = atoi( &argv[i][2] ); */
		else if (argv[i][1] == 'v')
			g_virtual_part_cnt = atoi(&argv[i][2]);
		else if (argv[i][1] == 't')
			g_thread_cnt = atoi(&argv[i][2]);
		else if (argv[i][1] == 's')
			g_synth_table_size = atoi(&argv[i][2]);
		else if (argv[i][1] == 'R')
			g_req_per_query = atoi(&argv[i][2]);
		else if (argv[i][1] == 'f')
			g_field_per_tuple = atoi(&argv[i][2]);
		else if (argv[i][1] == 'n')
			g_num_wh = atoi(&argv[i][2]);
		else if (argv[i][1] == 'G') {
			if (argv[i][2] == 'a')
				g_abort_penalty = atoi(&argv[i][3]);
			else if (argv[i][2] == 'c')
				g_central_man = atoi(&argv[i][3]);
			else if (argv[i][2] == 't')
				g_ts_alloc = atoi(&argv[i][3]);
			else if (argv[i][2] == 'k')
				g_key_order = atoi(&argv[i][3]);
			else if (argv[i][2] == 'n')
				g_no_dl = atoi(&argv[i][3]);
			else if (argv[i][2] == 'o')
				g_timeout = atol(&argv[i][3]);
			else if (argv[i][2] == 'l')
				g_dl_loop_detect = atoi(&argv[i][3]);
			else if (argv[i][2] == 'b')
				g_ts_batch_alloc = atoi(&argv[i][3]);
			else if (argv[i][2] == 'u')
				g_ts_batch_num = atoi(&argv[i][3]);
		}
		else if (argv[i][1] == 'T') {
			if (argv[i][2] == 'p')
				g_perc_payment = atof(&argv[i][3]);
			if (argv[i][2] == 'u')
				g_wh_update = atoi(&argv[i][3]);
			if (argv[i][2] == 'r')
				if (argv[i][3] == 'P')
					g_perc_remote_payment = atoi(&argv[i][4]);
				else if (argv[i][3] == 'N')
					g_perc_remote_neworder = atoi(&argv[i][4]);
			if (argv[i][2] == 'n') {
				if (argv[i][3] == 'D') {
					g_numa_distance = atoi(&argv[i][4]);
					g_enforce_numa_distance = true;
				}
				if (argv[i][3] == 'R') {
					g_enforce_numa_distance_remote_only = argv[i][4] != '0';
				}
				if (argv[i][3] == 'M') {
					char *tok = strtok(&argv[i][4], ":");
					UInt32 node = atoi(tok);
					auto&& node_map = g_numa_dist_matrix[node];
					tok = strtok(NULL, "=");
					while(tok) {
						UInt32 key = atoi(tok);
						tok = strtok(NULL, ",");
						if(tok == NULL) {
							printf("Expecting numa node!");
							throw std::runtime_error("Expecting numa distance!");
						}
						UInt32 val = atoi(tok);

						node_map[key] = val;

						tok = strtok(NULL, "=");
					}
				}
				if (argv[i][3] == 'T') {
					g_threads_per_socket = atoi(&argv[i][4]);
				}
			}
			if (argv[i][2] == 's') {
				g_calc_size_only = true;
			}
		}
		else if (argv[i][1] == 'A') {
			if (argv[i][2] == 'r')
				g_test_case = READ_WRITE;
			if (argv[i][2] == 'c')
				g_test_case = CONFLICT;
		}
		else if (argv[i][1] == 'o') {
			i++;
			output_file = argv[i];
		}
		else if (argv[i][1] == 'h') {
			print_usage();
			exit(0);
		}
		else if (argv[i][1] == '-') {
			string line(&argv[i][2]);
			size_t pos = line.find("=");
			assert(pos != string::npos);
			string name = line.substr(0, pos);
			string value = line.substr(pos + 1, line.length());
			assert(g_params.find(name) != g_params.end());
			g_params[name] = value;
		}
		else
			assert(false);
	}
	if (g_thread_cnt < g_init_parallelism)
		g_init_parallelism = g_thread_cnt;

	print_configuration();
}
