#!/bin/sh
# File Name       :queue_sequential.sh
# Description     :Script to execute sequential execution of numerical integration on the cluster
# Author          :Karthik Rao
# Version         :0.1

qsub -q mamba -l nodes=1:ppn=16 -d $(pwd) ./run_sequential.sh


