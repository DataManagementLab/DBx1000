#!/bin/bash

trap "trap - SIGTERM && kill -- -$$" SIGINT SIGTERM EXIT

# Disable core dump!
ulimit -S -c 0

# Ensure sufficiently many files can be opened (also for sys info under /sys)
ulimit -n 100000

# Default values
WHs="1024"
threads="1 2 4"
remote_news="1" # Default TPC-C (implies 15 perc. remote payment)
if [ -z ${CCs+x} ]; then
	CCs="WAIT_DIE NO_WAIT DL_DETECT MVCC HSTORE OCC TICTOC SILO"
fi
NO_HTs="0" # Only with hyperthreading
SMALLs="1" # Run only small relations
BACKOFFs="0" # Only original waiting/deadlock detection
CONFIG_FILE="config-2optimised.h"
DYNAMIC_SMT="0" # Do not try dynamic SMT by default, it only exists on power
reps=3
numaDistances="-1"
numaMap=""
remoteNumaOnlys="0"
use_cache_meter="0"
use_wait_for_input="0"

declare -A HAWK_ALLOC_CAPACITY_MAP
declare -A HAWK_ALLOC_CAPACITY_MAP_MVCC

curDate=`date +"%F_%H-%M-%S"`
resdir="results/${curDate}"
mkdir results
mkdir ${resdir}

# Load custom config values from config file
if [ -r "${1}" ];then
	source "${1}"
	cp "${1}" "${resdir}"
else
	echo "Provide configuration file!"
	echo -e "\a"
	exit 1
fi

# Check for list of config files
if [ -z ${CONFIG_FILES+x} ]; then
	CONFIG_FILES="${CONFIG_FILE}"
fi

if [ ${DYNAMIC_SMT} -eq 1 ]; then
	SMT_PREV=`ppc64_cpu --smt`
fi

cp ${0} ${resdir}
{
	# Disable core dump!
	ulimit -S -c 0


	export OMP_DISPLAY_ENV="true"

	#pre_exec="timeout 4h nice --18 numactl -l"
	pre_exec="timeout 4h nice --18"

	if [ ${use_wait_for_input} -eq "1" ]; then
		pre_exec="${pre_exec}"
	else
		export SEGFAULT_SIGNALS="all"
		pre_exec="catchsegv ${pre_exec}"
	fi

	if [ "${use_cache_meter}" -eq 1 ]; then

		# Deactivate Watch Dog to free performance counter for CacheMeter
		su -c "echo 0 > /proc/sys/kernel/nmi_watchdog"

		pre_exec="sudo ${pre_exec}"
	fi

	coresPerSocket=`lscpu | grep 'Core(s) per socket:' | grep -oP '[0-9]+'`
	threadsPerCore=`lscpu | grep 'Thread(s) per core:' |  grep -oP '[0-9]+'`
	let "numaThreadPerSocket=coresPerSocket*threadsPerCore"

	# Assemble arguments for numa map
	numaMapArgs=""
	for a in ${numaMap}; do
		numaMapArgs="${numaMapArgs} -TnM${a}"
	done

	for CONFIG_FILE in ${CONFIG_FILES}; do

	cp "variant_config/${CONFIG_FILE}" "${resdir}"

	num_config_files=`echo ${CONFIG_FILES} | wc -w`
	if [ "${num_config_files}" -gt 1 ] || [ "$#" -le 1 ] || [ "$2" -gt 0 ]; then
		# build resources

		if [ -z ${dbx1000_build_dir+x} ]; then
			build_path="build"
		else
			build_path="${dbx1000_build_dir}"
		fi

		rm -r "${build_path}"
		mkdir "${build_path}"
		pushd "${build_path}"

		WHS=`echo ${WHs} | sed -r 's/ /\;/g'`
		CCS=`echo ${CCs} | sed -r 's/ /\;/g'`
		TPCC_SIZES=`echo ${SMALLs} | sed -r 's/1/SMALL/g;s/0/FULL/g;s/ /\;/g'`
		BACKOFF_DELAYS=`echo ${BACKOFFs} | sed -r 's/ /\;/g'`

		cmake .. -DCMAKE_BUILD_TYPE=Release -DCONFIG_FILE=${CONFIG_FILE} -DWHS=${WHS} -DCCS=${CCS} -DTPCC_SIZES=${TPCC_SIZES} -DBACKOFF_DELAYS=${BACKOFF_DELAYS} -DUSE_CACHE_METER=${use_cache_meter} -DUSE_WAIT_FOR_INPUT=${use_wait_for_input}
		#cmake .. -DCMAKE_BUILD_TYPE=Debug -DCONFIG_FILE=${CONFIG_FILE} -DWHS=${WHS} -DCCS=${CCS} -DTPCC_SIZES=${TPCC_SIZES} -DBACKOFF_DELAYS=${BACKOFF_DELAYS}
		#cmake .. -DCMAKE_BUILD_TYPE=RELWITHDEBINFO -DCONFIG_FILE=${CONFIG_FILE} -DWHS=${WHS} -DCCS=${CCS} -DTPCC_SIZES=${TPCC_SIZES} -DBACKOFF_DELAYS=${BACKOFF_DELAYS} -DUSE_CACHE_METER=${use_cache_meter}

		make -j rundb_all
		retVal=$?
		if [ ${retVal} -ne 0 ]; then
			echo "Build failed! exit=${retVal}"
			break
		fi

		popd
	fi

	if [ "$#" -gt 1 ] && [ "$2" -eq 1 ]; then
		echo "Only building."
		if [ "${num_config_files}" -gt 1 ]; then
			continue
		else
			exit 0
		fi
	fi

	for BACKOFF in ${BACKOFFs}; do
		for NO_HT in ${NO_HTs}; do

			if [ ${DYNAMIC_SMT} -eq 1 ]; then
				ppc64_cpu --smt="${NO_HT}"
			elif [ ${NO_HT} -eq 1 ]; then
				cpus=`nproc`
				let "cpus=cpus/2-1"
				pre_exec="${pre_exec} numactl -C0-${cpus}"
			fi

			for SMALL in ${SMALLs}; do
				for WH in ${WHs}; do
					for numaDist in ${numaDistances}; do

						numaArgs=""
						if [ ${numaDist} -ge 0 ]; then
							numaArgs="-TnD${numaDist} -TnT${numaThreadPerSocket} ${numaMapArgs}"
						fi

						for remoteNumaOnly in ${remoteNumaOnlys}; do

						for remote_new in ${remote_news}; do

							# Remote payment semi proportional to remote neworder
							if [ ${remote_new} -eq 0 ]; then
								remote_pay=0
							else
								let "remote_pay=15+remote_new*85/100"
							fi
							for CC in ${CCs}; do

								#sed -ir "s/#define CC_ALG.*/#define CC_ALG ${CC}/" config.h
								#make -j50

								for thread in ${threads}; do

									
									if [ ${DYNAMIC_SMT} -eq 1 ]; then
										let "thread=thread*NO_HT"
									elif [ ${NO_HT} -eq 1 ]; then
										let "thread=thread/2"
									fi

									for rep in `seq 1 ${reps}`; do

									run="./${build_path}/rundb_${WH}_${CC}"

									if [ ${SMALL} -eq 1 ]; then
										run="${run}_SMALL"
									else
										run="${run}_FULL"
									fi

									run="${run}_${BACKOFF}"

									CONFIG_FILE_ESC=`echo ${CONFIG_FILE} | sed -r 's/\.h//g;s/\//-/g'`
									echo "WH=$WH CC=$CC T=$thread SMALL=$SMALL NO_HT=$NO_HT RP=$remote_pay RN=$remote_new BACKOFF=${BACKOFF} CONFIG_FILE=${CONFIG_FILE_ESC}"

									out_file="${resdir}/${WH}_${CC}_${thread}_${SMALL}_${NO_HT}_${remote_pay}_${remote_new}_${BACKOFF}_${numaDist}_${remoteNumaOnly}_${CONFIG_FILE_ESC}"

									# MVCC requires more memory
									if [ "${CC}" == "MVCC" ] || [ "${CC}" == "OCC" ]; then
										EXPO="${HAWK_ALLOC_CAPACITY_MAP_MVCC[$thread]}"
									else
										EXPO="${HAWK_ALLOC_CAPACITY_MAP[$thread]}"
									fi

									if [ -z "${EXPO}" ]; then
										# Unset
										HAWK_ALLOC_CAPACITY=$((1<<31))
									else
										HAWK_ALLOC_CAPACITY=$((1<<${EXPO}))
									fi
									
									if [ "${CC}" == "MVCC" ] || [ "${CC}" == "OCC" ]; then
										if [ ${thread} -eq 704 ]; then
                                    							#let "HAWK_ALLOC_CAPACITY=HAWK_ALLOC_CAPACITY+HAWK_ALLOC_CAPACITY/2"
											echo "skip"
										elif [ ${thread} -eq 752 ]; then
                                    							#let "HAWK_ALLOC_CAPACITY=HAWK_ALLOC_CAPACITY+HAWK_ALLOC_CAPACITY/10"
                                    							let "HAWK_ALLOC_CAPACITY=HAWK_ALLOC_CAPACITY"
										fi
									fi

									if [ ${use_wait_for_input} -eq "1" ]; then
										echo "HAWK_ALLOC_CAPACITY=${HAWK_ALLOC_CAPACITY} OMP_PROC_BIND=close HUBSTATS_FILE=${out_file}.hubstats.out CACHE_METER_RESULT_FILE=${out_file}.cpu.csv ${pre_exec} ${run} -t${thread} -TrP${remote_pay} -TrN${remote_new} -TnR${remoteNumaOnly} -o ${out_file}.result.csv ${numaArgs} |& tee -a ${out_file}.log.out" >> ${resdir}/commands.sh
									else

									HAWK_ALLOC_CAPACITY=${HAWK_ALLOC_CAPACITY} \
									OMP_PROC_BIND=close \
									HUBSTATS_FILE=${out_file}.hubstats.out \
									CACHE_METER_RESULT_FILE=${out_file}.cpu.csv \
									${pre_exec} ${run} -t${thread} -TrP${remote_pay} -TrN${remote_new} -TnR${remoteNumaOnly} -o ${out_file}.result.csv ${numaArgs} &>> ${out_file}.log.out

									retVal=$?
									if [ ${retVal} -ne 0 ]; then
										echo "!! RUN FAILED !!"
									fi

									sleep 30s

									if [ -e sdstats_registers.dat ]; then
										mv sdstats_registers.dat "${out_file}.hubstats.dat"
									elif [ -e uvstats_registers.dat ]; then
										mv uvstats_registers.dat "${out_file}.hubstats.dat"
									else
										echo "WARN: No hubstats data found"
									fi

									fi # Use Wait for Input
									
									done # reps
								done # threads
							done # CC
						done # remote_new
						done # remoteNumaOnly
					done # numaDist
				done # Warehouses
			done # SMALL and FULL
		done # NO_HT
	done # BACKOFFs

	done # CONFIG_FILES

	if [ "${use_cache_meter}" -eq 1 ]; then
		# Activate Watch Dog again
		su -c "echo 1 > /proc/sys/kernel/nmi_watchdog"
	fi

	if [ -z ${dbx1000_build_dir+x} ]; then
		# Nothing
		echo "" > /dev/null
	else
		rm -r "${build_path}"
	fi

} |& tee ${resdir}/output.log

if [ ${DYNAMIC_SMT} -eq 1 ]; then
	ppc64_cpu --smt="${SMT_PREV}"
fi
