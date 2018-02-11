/*
 * File Name : static_sched.cpp
 *
 * To compile :
 *
 * mpicxx -std=c++11 static_sched.cpp -o static_sched
 * 
 * Sample command line execution :
 * 
 * mpirun -n 3 ./static_sched  1 0 10 1000 1
 * qsub -d $(pwd) -q mamba -l procs=2 -v FID=1,A=0,B=10,N=1000,INTENSITY=1,PROC=2 ./run_static.sh
 *
 */

/* Debug prints will be enabled if set to 1 */
#define DEBUG 0
#define NODE_0 0

#include <mpi.h>
#include <stdio.h>
#include <iostream>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <chrono>
#include <string.h>
#include <cmath>

#include "CommonHeader.h"

#ifdef __cplusplus
extern "C" {
#endif

    float f1(float x, int intensity);
    float f2(float x, int intensity);
    float f3(float x, int intensity);
    float f4(float x, int intensity);

#ifdef __cplusplus
}
#endif

/* function pointer to one of the following functions : f1, f2, f3, f4 */
typedef float (*Func) (float, int);


/*==============================================================================
 *  main
 *=============================================================================*/

int main (int argc, char* argv[]) {


    if (argc < 6) {
        std::cerr<<"Usage: "<<argv[0]<<" <FunctionID> <LowerBound> <UpperBound> \
            <NoOfPoints> <Intensity> "<<std::endl;

        return -1;
    }

    MPI_Init(NULL, NULL);

    int i, FunctionID, Intensity, Node;
    float LowerBound, UpperBound;
    int NoOfPoints;
    float  x, y, FunOutput;
    //omp_sched_t SchedKind;
    int CommSize;
    int ProcRank;
    float StartIndex, StopIndex;
    float * IntegralOutput;
    float * NodeIntegralOutput;

    IntegralOutput = new float [1];
    NodeIntegralOutput = new float [1];
    memset (IntegralOutput, 0, sizeof(IntegralOutput[0]));
    memset (NodeIntegralOutput, 0, sizeof(NodeIntegralOutput[0]));

    /* function pointer to one of the following functions : f1, f2, f3, f4 */
    Func FuncToIntegrate;

    /* measure time taken for integration */
    std::chrono::time_point<std::chrono::system_clock> StartTime;
    std::chrono::time_point<std::chrono::system_clock>  EndTime;
    std::chrono::duration<double> ElapsedTime;

    FunctionID  = atoi (argv[1]);
    LowerBound  = atof (argv[2]);
    UpperBound  = atof (argv[3]);
    NoOfPoints  = atoi (argv[4]);
    Intensity   = atoi (argv[5]);

    DLOG (C_VERBOSE, "The FunctionID = %d\n", FunctionID);
    DLOG (C_VERBOSE, "The LowerBound = %f\n", LowerBound);
    DLOG (C_VERBOSE, "The UpperBound = %f\n", UpperBound);
    DLOG (C_VERBOSE, "The NoOfPoints = %d\n", NoOfPoints);
    DLOG (C_VERBOSE, "The Intensity = %d\n", Intensity);

    /* based on the input argument, select suitable function to integrate */
    switch (FunctionID)
    {
        case 1:FuncToIntegrate = f1;
               break;
        case 2:FuncToIntegrate = f2;
               break;
        case 3:FuncToIntegrate = f3;
               break;
        case 4:FuncToIntegrate = f4;
               break;
        default:
               DLOG(C_ERROR, "Invalid function input for integration\n");
               goto EXIT;
    }

    MPI_Comm_size(MPI_COMM_WORLD, &CommSize);
    MPI_Comm_rank(MPI_COMM_WORLD, &ProcRank);
    StartIndex = (int)floor ((ProcRank) * NoOfPoints/CommSize);
    StopIndex  = (int)floor ((ProcRank+1) * NoOfPoints/CommSize);
    /*  y = (a - b)/n */
    y = (UpperBound - LowerBound)/NoOfPoints;

    DLOG (C_VERBOSE, "rank %d out of %d processors. \n", ProcRank, CommSize);
    DLOG (C_VERBOSE, "node[%d] The StartIndex = %f\n", ProcRank, StartIndex);
    DLOG (C_VERBOSE, "node[%d] The StopIndex = %f\n", ProcRank, StopIndex);

    MPI_Barrier( MPI_COMM_WORLD ) ;
    if (ProcRank == NODE_0){
        StartTime = std::chrono::system_clock::now();
    }
    MPI_Barrier( MPI_COMM_WORLD ) ;


    for (i= StartIndex; i< StopIndex; i++) {

        x = (LowerBound + ((i + 0.5)*y));
        FunOutput = FuncToIntegrate (x, Intensity);
        FunOutput = FunOutput * y ;
        IntegralOutput[0] = IntegralOutput[0] + FunOutput;

    }
    if (ProcRank != NODE_0){
        DLOG (C_VERBOSE, "node[%d] The IntegralOutput = %f\n", ProcRank,IntegralOutput[0]);
        MPI_Send (IntegralOutput, 1, MPI_FLOAT, NODE_0, 0, MPI_COMM_WORLD);
    }else{

        for (Node=1; Node<CommSize; Node++)
        {
            MPI_Recv (NodeIntegralOutput, 1, MPI_FLOAT, Node, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE );
            IntegralOutput[0] = IntegralOutput[0] + NodeIntegralOutput[0];
        }
    }

    MPI_Barrier( MPI_COMM_WORLD ) ;
    /* compute the time taken to compute the sum and display the same */
    if (ProcRank == NODE_0){
        EndTime = std::chrono::system_clock::now();
        ElapsedTime = EndTime - StartTime;

        std::cout<<IntegralOutput[0]<<std::endl;
        std::cerr<<ElapsedTime.count()<<std::endl;
    }
    MPI_Barrier( MPI_COMM_WORLD ) ;

    delete[] NodeIntegralOutput;
    delete[] IntegralOutput;



EXIT:
    MPI_Finalize();

    return 0;
}
