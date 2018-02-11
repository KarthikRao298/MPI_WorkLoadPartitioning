#!/bin/sh
# File Name       :run_advnc.sh
# Description     :Script to execute advanced master-worker scheduling
# Author          :Karthik Rao
# Version         :0.1

RESULTDIR=result/
h=`hostname`

if [ "$h" = "mba-i1.uncc.edu"  ];
then
    echo Do not run this on the headnode of the cluster, use qsub!
    exit 1
fi

if [ ! -d ${RESULTDIR} ];
then
    mkdir ${RESULTDIR}
fi
    
mpirun ./advnc_sched ${FID} ${A} ${B} ${N} ${INTENSITY} 2> ${RESULTDIR}/advnc_${N}_${INTENSITY}_${PROC}  >/dev/null

