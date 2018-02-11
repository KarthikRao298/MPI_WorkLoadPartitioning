#!/bin/sh
# File Name       :plot_advnc.sh
# Description     :Script to plot speedup of advanced master-worker scheduler
# Author          :Karthik Rao
# Version         :0.1

. ./params_dynamic.sh

if [ ! -d ${RESULTDIR} ];
then
    mkdir ${RESULTDIR}
fi

#strong scaling

for INTENSITY in ${INTENSITIES};
do
    for N in ${NS};
    do
	FILE=${RESULTDIR}/sequential_${N}_${INTENSITY}
	if [ ! -f ${FILE} ]
	then
	    echo missing sequential result file "${FILE}". Have you run queue_sequential and waited for completion?
	fi

	seqtime=$(cat ${RESULTDIR}/sequential_${N}_${INTENSITY})
	
	for PROC in ${PROCS}
	do
	
	    FILE=${RESULTDIR}/advnc_${N}_${INTENSITY}_${PROC}
	    
	    if [ ! -f ${FILE} ]
	    then
		echo missing advnc result file "${FILE}". Have you run queue_advnc and waited for completion?
	    fi

	    partime=$(cat ${RESULTDIR}/advnc_${N}_${INTENSITY}_${PROC})
	    
	    echo ${PROC} ${seqtime} ${partime}
	done > ${RESULTDIR}/speedup_advnc_ni_${N}_${INTENSITY}


	GNUPLOTSTRONG="${GNUPLOTSTRONG} set title 'strong scaling. n=${N} i=${INTENSITY}'; plot '${RESULTDIR}/speedup_advnc_ni_${N}_${INTENSITY}' u 1:(\$2/\$3);"
    done
done

gnuplot <<EOF
set terminal pdf
set output 'advnc_sched_plots.pdf'

set style data linespoints

set key top left

set xlabel 'proc'
set ylabel 'speedup'

${GNUPLOTSTRONG}
EOF


