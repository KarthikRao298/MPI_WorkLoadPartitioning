#!/bin/sh
# File Name       :queue_static.sh
# Description     :Script to execute strong & weak scaling of static scheduling on cluster
# Author          :Karthik Rao
# Version         :0.1

. ./params.sh

if [ ! -d ${RESULTDIR} ];
then
    mkdir ${RESULTDIR}
fi

#strong scaling

for INTENSITY in ${INTENSITIES};
do
    for N in ${NS};
    do	

	for PROC in ${PROCS}
	do
	
	    FILE=${RESULTDIR}/static_${N}_${INTENSITY}_${PROC}
	    
	    if [ ! -f ${FILE} ]
	    then
		qsub -d $(pwd) -q mamba -l procs=${PROC} -v FID=1,A=0,B=10,N=${N},INTENSITY=${INTENSITY},PROC=${PROC} ./run_static.sh
	    fi

	done

    done
done


#weak scaling

for INTENSITY in ${INTENSITIES};
do
    for N in ${NS};
    do	

	for PROC in ${PROCS}
	do
	    
	    REALN=$( echo ${N} \* ${PROC}  | bc)
	    
	    FILE=${RESULTDIR}/static_${REALN}_${INTENSITY}_${PROC}
	    
	    if [ ! -f ${FILE} ]
	    then
			qsub  -d $(pwd) -q mamba -l procs=${PROC} -v FID=1,A=0,B=10,N=${REALN},INTENSITY=${INTENSITY},PROC=${PROC} ./run_static.sh
	    fi

	done

    done
done


