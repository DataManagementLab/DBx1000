#!/bin/bash

if [ "$#" -lt 1 ]; then
    echo "Please provide path to hcmtsetup.zip"
    echo -e "See for Power:\n/sapmnt/production2/makeresults/HANA/views/classical/HANA/MT/CO_GCC9/hana2sp05/rel/linuxppc64le/502699/hwcct/hcmtsetup.zip \nFor x86:\n/sapmnt/production2/makeresults/HANA/views/classical/HANA/MT/CO_GCC9/hana2sp05/rel/linuxx86_64/502699/hwcct/hcmtsetup.zip"
    exit 1
fi

hcmtSetupDir="${1}"
plan="`pwd`/benchmarks/hardware/numa.json"
#plan="`pwd`/benchmarks/hardware/test.json"
#plan="`pwd`/benchmarks/hardware/bw.json"
#plan="`pwd`/benchmarks/hardware/lat.json"

echo "Using hcmtsetup from: ${hcmtSetupDir}"

curDate=`date +"%F_%H-%M-%S"`
resdir="`pwd`/results/${curDate}"
mkdir results
mkdir ${resdir}

tmpdir="/dev/shm/${curDate}/hcmt"
hcmt="${tmpdir}/hcmt"

{
    cp ${0} ${resdir}
    cp ${plan} ${resdir}

    pre_exec="nice --18"

    export SEGFAULT_SIGNALS="all"
	pre_exec="catchsegv ${pre_exec}"

    echo "Measuring only from CPU node 0"
    pre_exec="${pre_exec} numactl -N0"

    mkdir -p "${tmpdir}"

    unzip "${hcmtSetupDir}" -d "${tmpdir}"

    retVal=$?
    if [ ${retVal} -ne 0 ]; then
        echo "Unzip of hcmt FAILED! ${retVal}"
        exit ${retVal}
    fi

    chmod +x "${hcmt}"

    ${pre_exec} ${hcmt} -v -p ${plan} -q -y |& tee ${resdir}/hcmt.log

    unzip "${tmpdir}/hcmtresult-*.zip" -d ${resdir}

    rm -v -r "${tmpdir}"

    chmod -R a+rw ${resdir}

    tail -n +29 "${resdir}/Results/36324424-F9CC-44DF-820752124C6A5652.json" | head -n20
    tail -n +29 "${resdir}/Results/113FE950-77A4-41CF-884BAF25F4EB27F0.json" | head -n20

} |& tee ${resdir}/output.log
