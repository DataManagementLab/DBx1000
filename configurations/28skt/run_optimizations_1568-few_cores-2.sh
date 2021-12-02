#!/bin/bash

script="run_tpcc.sh"
conf="configurations/28skt/run_optimizations_1568-few_cores-2.sh"
array=("TICTOC HSTORE WAIT_DIE MVCC" "SILO DL_DETECT NO_WAIT OCC")

sockets=14
x=0
for CCs in "${array[@]}"; do   # The quotes are necessary here
	let "i=x*sockets"
	let "j=i+sockets-1"
	dbx1000_build_dir="build_${x}" CCs="${CCs}" numactl -N${i}-${j} -m${i}-${j} bash -x ${script} ${conf} &> /dev/null &
	let "x=x+1"
	sleep 1
done

wait
