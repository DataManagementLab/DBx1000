#!/bin/bash

trap "trap - SIGTERM && kill -- -$$" SIGINT SIGTERM EXIT

script="run_tpcc.sh"
conf="configurations/28skt/optimizations_4WH-2.conf"
array=("SILO TICTOC" "HSTORE" "WAIT_DIE" "MVCC" "DL_DETECT" "NO_WAIT" "OCC")

x=0
for CCs in "${array[@]}"; do   # The quotes are necessary here
	let "i=x*4"
	let "j=i+3"
	dbx1000_build_dir="build_${x}" CCs="${CCs}" numactl -N${i}-${j} -m${i}-${j} bash -x ${script} ${conf} &> /dev/null &
	let "x=x+1"
	sleep 1
done


wait
