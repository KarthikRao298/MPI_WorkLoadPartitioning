/*
 * File Name       :dynamic_sched.cpp
 * Description     :Implimentation of master-worker scheduler
 * Author          :Karthik Rao
 * Version         :1.1
 * To compile :
 *
 * mpicxx -std=c++11 dynamic_sched.cpp -o dynamic_sched libfunctions.a libintegrate.a  
 * 
 * Sample command line execution :
 * 
 * mpirun -n 3 ./dynamic_sched 1 0 10 1000 1
 * qsub -d $(pwd) -q mamba -l procs=2 -v FID=1,A=0,B=10,N=1000,INTENSITY=1,PROC=2 ./run_static.sh
 *
 */

/* Debug prints will be enabled if set to 1 */
#define DEBUG 0
#define MAX_PROCESS_NO 32
#define MASTER_NODE 0
#define MASTER_TO_SLAVE_WORK_AVAILABLE 1
#define MASTER_TO_SLAVE_QUIT 2
#define SLAVE_TO_MASTER_REQ_WORK 3

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


typedef struct
{
    /* starting value of the range of indices a thread is supposed to execute */
    int StartIndex;
    /* stopping value of the range of indices a thread is supposed to execute */
    int StopIndex;
    int Intensity;
    /*   !!!!  should this be long ?   */
    int NoOfPoints;
    float LowerBound, UpperBound;
    int Granularity;
    //float IntegralOutput;
    /* stores the max value of the index upto which the integral has been computed */
    int CompletedIndex;
    /* function pointer to one of the following functions : f1, f2, f3, f4 */
    Func FuncToIntegrate;

} ThreadData;
/*Reference to thread private structure */
typedef ThreadData * RefThreadData;


/* function to check if all the iterations are complete */
bool IsLoopDone (void * inArg);
/* function to get the next loop iteration values */
int GetNextLoop (void * inArg);
/* function which will be executed by the slave nodes */
static void SlaveWork (void * inArg);
/* function which will be executed by the master node */
static void MasterWork (void * inArg);

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

    int FunctionID;
    int CommSize;
    int ProcRank;
    MPI_Comm_size(MPI_COMM_WORLD, &CommSize);
    MPI_Comm_rank(MPI_COMM_WORLD, &ProcRank);

    ThreadData ThreadInfo;

    FunctionID  = atoi (argv[1]);
    ThreadInfo.LowerBound  = atof (argv[2]);
    ThreadInfo.UpperBound  = atof (argv[3]);
    ThreadInfo.NoOfPoints  = atoi (argv[4]);
    ThreadInfo.Intensity   = atoi (argv[5]);
    ThreadInfo.StartIndex = 0;
    ThreadInfo.StopIndex = 0;
    ThreadInfo.CompletedIndex = 0;
    /* based on multiple runs of the program,
     * a granularuty of 100 was found to be OK
     */
    ThreadInfo.Granularity = 100;
    /* based on the input argument, select suitable function to integrate */
    switch (FunctionID)
    {
        case 1:ThreadInfo.FuncToIntegrate = f1;
               break;
        case 2:ThreadInfo.FuncToIntegrate = f2;
               break;
        case 3:ThreadInfo.FuncToIntegrate = f3;
               break;
        case 4:ThreadInfo.FuncToIntegrate = f4;
               break;

        default: 
               DLOG(C_ERROR, "Invalid function input for integration\n");
               goto EXIT;
    }


    if (ProcRank == MASTER_NODE){
        MasterWork(&ThreadInfo);
    }else{
        SlaveWork(&ThreadInfo);
    }

EXIT:

    MPI_Finalize();

    return 0;

}

/*==============================================================================
 *  MasterWork
 *=============================================================================*/

static void MasterWork (void * inArg){

    RefThreadData ThreadInfo = (RefThreadData)inArg;
    /* receive a message from slave requesting work */
    /* check if work is available */
    /* if work is available send the work struct to slave */
    /* if work is not available signal slave to abort */

    int CommSize;
    int ProcRank;
    MPI_Comm_size(MPI_COMM_WORLD, &CommSize);
    MPI_Comm_rank(MPI_COMM_WORLD, &ProcRank);
    MPI_Status status;

    int * Index;
    Index = new int [2];
    int QuitCounter = 0;

    float * IntegralOutput;
    float * NodeIntegralOutput;
    IntegralOutput = new float [1];
    NodeIntegralOutput = new float [1];
    memset (IntegralOutput, 0, sizeof(IntegralOutput[0]));
    memset (NodeIntegralOutput, 0, sizeof(NodeIntegralOutput[0]));

    /* measure time taken for integration */
    std::chrono::time_point<std::chrono::system_clock> StartTime;
    std::chrono::time_point<std::chrono::system_clock>  EndTime;
    std::chrono::duration<double> ElapsedTime;
    StartTime = std::chrono::system_clock::now();

    while (1) {

        MPI_Recv (NodeIntegralOutput, 1, MPI_FLOAT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD,&status);

        IntegralOutput[0] = IntegralOutput[0] + NodeIntegralOutput[0];
        DLOG (C_VERBOSE, "Node[master] IntegralOutput = %f, NodeIntegralOutput = %f\n", IntegralOutput[0], NodeIntegralOutput[0]);

        if (!IsLoopDone(ThreadInfo)) {

            DLOG (C_VERBOSE, "Node[master] Work Is Available. sending work to node :%d\n", status.MPI_SOURCE);

            GetNextLoop (ThreadInfo);

            Index[0] = ThreadInfo->StartIndex;
            Index[1] = ThreadInfo->StopIndex;

            DLOG (C_VERBOSE, "Node[master] StartIndex = %d StopIndex = %d\n", Index[0], Index[1]);

            MPI_Send(Index, 2, MPI_INT, status.MPI_SOURCE, MASTER_TO_SLAVE_WORK_AVAILABLE, MPI_COMM_WORLD);

        }else {

            DLOG (C_VERBOSE, "Node[master] Work Is not Available. sending quit to node :%d\n", status.MPI_SOURCE);

            MPI_Send(Index, 2, MPI_INT, status.MPI_SOURCE, MASTER_TO_SLAVE_QUIT, MPI_COMM_WORLD);
            QuitCounter++;
        }

        if (QuitCounter == CommSize - 1){
            DLOG (C_VERBOSE, "Quit message sent to all the slaves. master exiting\n");
            break;
        }
    }

    /* compute the time taken to compute the sum and display the same */
    EndTime = std::chrono::system_clock::now();
    ElapsedTime = EndTime - StartTime;

    std::cout<<IntegralOutput[0]<<std::endl;
    std::cerr<<ElapsedTime.count()<<std::endl;

    delete[] NodeIntegralOutput;
    delete[] IntegralOutput;
    delete[] Index;

}

/*==============================================================================
 *  SlaveWork
 *=============================================================================*/

static void SlaveWork (void * inArg){

    RefThreadData ThreadInfo = (RefThreadData)inArg;

    /* send a mmessage to master, requesting work */
    /* terminate if the message from master says so */
    /* do the work if the message is not to terminate */
    int CommSize;
    int ProcRank;
    MPI_Comm_size(MPI_COMM_WORLD, &CommSize);
    MPI_Comm_rank(MPI_COMM_WORLD, &ProcRank);
    MPI_Status status;

    int * Index;
    Index = new int [2];
    int i, StartIndex, StopIndex;

    float * NodeIntegralOutput;
    NodeIntegralOutput = new float [1];
    memset (NodeIntegralOutput, 0, sizeof(NodeIntegralOutput[0]));

    float y, x, FuncOutput;

    /*  y = (a - b)/n */
    y = (ThreadInfo->UpperBound - ThreadInfo->LowerBound)/ThreadInfo->NoOfPoints;

    while (1){

        DLOG (C_VERBOSE, "Node[%d] Sending integration %f y = %f\n", ProcRank, NodeIntegralOutput[0], y);
        MPI_Send (NodeIntegralOutput, 1, MPI_FLOAT, MASTER_NODE, SLAVE_TO_MASTER_REQ_WORK, MPI_COMM_WORLD);

        memset (NodeIntegralOutput, 0, sizeof(NodeIntegralOutput[0]));

        MPI_Recv (Index, 2, MPI_INT, MASTER_NODE, MPI_ANY_TAG, MPI_COMM_WORLD,&status);

        if (status.MPI_TAG == MASTER_TO_SLAVE_WORK_AVAILABLE) {

            DLOG (C_VERBOSE, "Node[%d] Doing Work. Computing integration\n", ProcRank);
            StartIndex = Index[0];
            StopIndex = Index[1];
            DLOG (C_VERBOSE, "Node[%d] StartIndex = %d StopIndex = %d\n", ProcRank, StartIndex, StopIndex);


            for (i=StartIndex; i< StopIndex; i++) {
                x = (ThreadInfo->LowerBound + ((i + 0.5)* y ));
                FuncOutput = ThreadInfo->FuncToIntegrate (x,ThreadInfo->Intensity );
                NodeIntegralOutput[0] += FuncOutput;
            }
            NodeIntegralOutput[0] = NodeIntegralOutput[0] * y;

        }else if (status.MPI_TAG == MASTER_TO_SLAVE_QUIT) {
            DLOG (C_VERBOSE, "Quit message received from master. Node %d exiting\n", ProcRank);
            break;
        }

    }

    delete[] NodeIntegralOutput;
    delete[] Index;
}

/*==============================================================================
 *  IsLoopDone
 *=============================================================================*/

bool IsLoopDone (void * inArg)
{
    bool C_Status;
    RefThreadData ThreadInfo = (RefThreadData)inArg;

    DLOG (C_VERBOSE, "ThreadInfo->CompletedIndex = %d\n", ThreadInfo->CompletedIndex);
    if (ThreadInfo->CompletedIndex == ThreadInfo->NoOfPoints){
        C_Status = true;
    }
    else{
        C_Status = false;
    }

    DLOG (C_VERBOSE, "C_Status = %d Exit\n", (int)C_Status);

    return C_Status;
}

/*==============================================================================
 *  GetNextLoop
 *=============================================================================*/

int GetNextLoop (void * inArg)
{
    int C_Status = C_SUCCESS; 
    RefThreadData ThreadInfo = (RefThreadData)inArg;

    DLOG (C_VERBOSE, "Granularity = %d\n",ThreadInfo->Granularity);

    ThreadInfo->StartIndex = ThreadInfo->CompletedIndex;
    ThreadInfo->StopIndex = ThreadInfo->CompletedIndex + ThreadInfo->Granularity;

    if (ThreadInfo->StopIndex >= ThreadInfo->NoOfPoints) {
        ThreadInfo->StopIndex = ThreadInfo->NoOfPoints;
    }

    ThreadInfo->CompletedIndex = ThreadInfo->StopIndex;
    DLOG (C_VERBOSE, "ThreadInfo->CompletedIndex = %d\n", ThreadInfo->CompletedIndex);


    return C_Status;
}
