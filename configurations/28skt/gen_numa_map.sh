#!/bin/bash

numaMap=""

for n in `seq 0 27`; do
        let "p=(n/4)"
        let "s=(n%4)"

        if [ ${s} -eq 0 ]; then
                let "d1=n+2"
        elif [ ${s} -eq 1 ]; then
                let "d1=n-1"
        elif [ ${s} -eq 2 ]; then
                let "d1=n+1"
        elif [ ${s} -eq 3 ]; then
                let "d1=n-2"
        else
                let "d1=n-3"
        fi

        if [ ${s} -eq 0 ]; then
                let "d2=n+3"
        elif [ ${s} -eq 1 ]; then
                let "d2=n+1"
        elif [ ${s} -eq 2 ]; then
                let "d2=n-1"
        elif [ ${s} -eq 3 ]; then
                let "d2=n-3"
        fi

        let "d3=(n+5)%28"

        m="${n}:1=${d1},2=${d2},3=${d3}"
        echo ${m}
        numaMap="${numaMap} ${m}"
done
echo ${numaMap}
