/*
 * File Name       :advnc_sched.cpp
 * Description     :Implementation of a advanced version of master-worker scheduler
 *                  to perform numerical integration
 * Author          :Karthik Rao
 * Version         :1.1
 *
 * To compile :
 *
 * mpicxx -std=c++11 advnc_sched.cpp -o advnc_sched libfunctions.a libintegrate.a  
 * 
 * Sample command line execution :
 * 
 * mpirun -n 3 ./advnc_sched 1 0 10 1000 1
 * qsub -d $(pwd) -q mamba -l procs=2 -v FID=1,A=0,B=10,N=1000,INTENSITY=1,PROC=2 ./run_static.sh
 *
 */

/* Debug prints will be enabled if set to 1 */
#define DEBUG 0
/* Max no of processors available in the system */
#define MAX_PROCESSORS 32
/* max no of chunk of work available at the slave at any point of time */
#define MAX_CHUNK 3
/* rank of the master node */
#define MASTER_NODE 0
/* message from master to the slave indicating that the work is available */
#define MASTER_TO_SLAVE_WORK_AVAILABLE 1000
/* message from master to slave indicating that the slave should terminate */
#define MASTER_TO_SLAVE_QUIT 2000
/* message from slave to master indicating that the slave needs work */
#define SLAVE_TO_MASTER_REQ_WORK 3000
/* message from slave to master indicating that the slave is terminating */
#define SLAVE_TO_MASTER_EXITING 4000

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
    long StartIndex;
    /* stopping value of the range of indices a thread is supposed to execute */
    long StopIndex;
    int Intensity;
    /*   !!!!  should this be long ?   */
    long NoOfPoints;
    double LowerBound, UpperBound;
    int Granularity;
    /* stores the max value of the index upto which the integral has been computed */
    long CompletedIndex;
    /* function pointer to one of the following functions : f1, f2, f3, f4 */
    Func FuncToIntegrate;

} ThreadData;
/*Reference to thread private structure */
typedef ThreadData * RefThreadData;


typedef struct
{
    long StartIndex;
    long StopIndex;

} IndexSt;
/* Reference to Index structure */
typedef IndexSt * RefIndexSt;


/* function to check if all the iterations are complete */
bool IsLoopDone (void * inArg);
/* function to get the next loop iteration values */
int GetNextLoop (void * inArg);
/* function which will be executed by the slave nodes */
static void SlaveWork (void * inArg);
/* function which will be executed by the master node */
static void MasterWork (void * inArg);
/* function to used to index the 2D struct of indices */
int GetFreeChunkIndex (int Node, int * ChunkIndex);
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
    ThreadInfo.NoOfPoints  = atol (argv[4]);
    ThreadInfo.Intensity   = atoi (argv[5]);
    ThreadInfo.StartIndex = 0;
    ThreadInfo.StopIndex = 0;
    ThreadInfo.CompletedIndex = 0;

    if (ThreadInfo.NoOfPoints < 1000) {
        DLOG(C_ERROR, "Invalid 'no of points' input for integration."
                "This implementation needs 'no of points' to be more than or equal to 1000\n");
        goto EXIT;
    }
    if (ThreadInfo.NoOfPoints < 10000) {
        /* calculation : MAX_CHUNK * MAX_PROCESSORS * Granularity < NoOfPoints */
        ThreadInfo.Granularity = 10;
    }else {
        /* based on multiple runs of the program,
         * a granularuty of 100 was found to satisfy most of the cases */
        ThreadInfo.Granularity = 100;
    }


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

/* 
 * 1. assign (use MPI_Isend) 3 chunks of data to all the slaves in round robin order
 * 2. receive (use MPI_Recv) a message from slave requesting work & returing the integration result
 * 3. store the result & check if work is available
 * 4. if work is available send (use MPI_Isend) the work struct to slave
 * 5. if work is not available signal (use MPI_Isend) slave to abort
 * 6. go to step 2
 * 7. If the message from the slave is SLAVE_TO_MASTER_EXITING, then master records that the slave
 *    has quit.
 * 8. When all the slaves have quit the master (is no longer a master :P) terminates! 
 *
 */

/*==============================================================================
 *  MasterWork
 *=============================================================================*/

static void MasterWork (void * inArg){

    RefThreadData ThreadInfo = (RefThreadData)inArg;

    int CommSize;
    int ProcRank, Node;
    MPI_Comm_size(MPI_COMM_WORLD, &CommSize);
    MPI_Comm_rank(MPI_COMM_WORLD, &ProcRank);
    MPI_Status Status[2];
    MPI_Request SendReq[2];

    int QuitCounter = 0;

    /*ideally index2D should have been dynamically allocated */
    IndexSt index2D[MAX_PROCESSORS][MAX_CHUNK] = {0};

    MPI_Datatype StructOfIndex;

    /* create a single struct */
    {
        int NoOfBlocks = 2;               /* number of Blocks in the struct */
        int Blocks[2] = {1, 1};   /* set up 2 Blocks */
        MPI_Datatype Types[2] = {    /* index internal Types */
            MPI_LONG,
            MPI_LONG,
        };
        MPI_Aint Disp[2] = {          /* internal displacements */
            offsetof(IndexSt, StartIndex),
            offsetof(IndexSt, StopIndex),
        };

        MPI_Type_create_struct(NoOfBlocks, Blocks, Disp, Types, &StructOfIndex);
        MPI_Type_commit(&StructOfIndex);
    }

    /* End of struct creation */

    double * IntegralOutput;
    double * NodeIntegralOutput;
    IntegralOutput = new double [1];
    NodeIntegralOutput = new double [1];
    memset (IntegralOutput, 0, sizeof(IntegralOutput[0]));
    memset (NodeIntegralOutput, 0, sizeof(NodeIntegralOutput[0]));

    int ChunkIndex[MAX_PROCESSORS];
    memset (ChunkIndex,0,MAX_PROCESSORS * sizeof (int));
    int CurChunk;

    /* measure time taken for integration */
    std::chrono::time_point<std::chrono::system_clock> StartTime;
    std::chrono::time_point<std::chrono::system_clock>  EndTime;
    std::chrono::duration<double> ElapsedTime;
    StartTime = std::chrono::system_clock::now();

    /* Assign 3 chunks of data to all the slave nodes in round robin order */
    for (int i=0; i<MAX_CHUNK; i++) {
        for (int Node=1; Node<CommSize; Node++) {

            if (!IsLoopDone(ThreadInfo)) {

                GetNextLoop (ThreadInfo);
                index2D[Node][i].StartIndex = ThreadInfo->StartIndex;
                index2D[Node][i].StopIndex = ThreadInfo->StopIndex;

                DLOG (C_VERBOSE, "Node[master] StartIndex = %d StopIndex = %d\n",
                        index2D[Node][i].StartIndex, index2D[Node][i].StopIndex);

                DLOG (C_VERBOSE, "Node[master] Work Is Available. sending work to node :%d\n", Node);

                /*ideally req will be in a array. need not be , as we are not checking the status */
                MPI_Isend(&index2D[Node][i], 1, StructOfIndex, Node, MASTER_TO_SLAVE_WORK_AVAILABLE, MPI_COMM_WORLD, &SendReq[0]);


            }
        }
    }


    while (1) {

        MPI_Recv (NodeIntegralOutput, 1, MPI_DOUBLE, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &Status[0]);

        if (Status[0].MPI_TAG == SLAVE_TO_MASTER_EXITING ){
            QuitCounter++;

            if (QuitCounter == CommSize - 1){
                DLOG (C_VERBOSE, "Quit message received from all the slaves. master exiting\n");
                break;
            }
        }

        IntegralOutput[0] = IntegralOutput[0] + NodeIntegralOutput[0];
        DLOG (C_VERBOSE, "Node[master] IntegralOutput = %f, NodeIntegralOutput = %f\n", IntegralOutput[0], NodeIntegralOutput[0]);

        Node = Status[0].MPI_SOURCE;
        CurChunk = GetFreeChunkIndex (Node, ChunkIndex);
        index2D[Node][CurChunk].StartIndex = ThreadInfo->StartIndex;
        index2D[Node][CurChunk].StopIndex = ThreadInfo->StopIndex;

        if (!IsLoopDone(ThreadInfo)) {

            DLOG (C_VERBOSE, "Node[master] Work Is Available. sending work to node :%d\n", Node);

            GetNextLoop (ThreadInfo);


            DLOG (C_VERBOSE, "Node[master] StartIndex = %d StopIndex = %d\n",
                    index2D[Node][CurChunk].StartIndex, index2D[Node][CurChunk].StopIndex);

            MPI_Isend(&index2D[Node][CurChunk], 1, StructOfIndex , Node, MASTER_TO_SLAVE_WORK_AVAILABLE, MPI_COMM_WORLD, &SendReq[0]);

        }else {

            DLOG (C_VERBOSE, "Node[master] Work Is not Available. sending quit to node :%d\n", Node);
            MPI_Isend(&index2D[Node][CurChunk], 1, StructOfIndex , Node, MASTER_TO_SLAVE_QUIT, MPI_COMM_WORLD, &SendReq[0]);
        }
    }

    /* compute the time taken to compute the sum and display the same */
    EndTime = std::chrono::system_clock::now();
    ElapsedTime = EndTime - StartTime;

    std::cout<<IntegralOutput[0]<<std::endl;
    std::cerr<<ElapsedTime.count()<<std::endl;

    MPI_Type_free(&StructOfIndex);

    delete[] NodeIntegralOutput;
    delete[] IntegralOutput;

}

/*
 * 0.1 send (use MPI_Isend) integration to the master, initially 0.
 * 1. receive (use MPI_Recv) a data from master.
 * 2. compute the integration.
 * 3. check if the last sent data to the master was received successfully 
 * by waiting.(use MPI_WAIT)
 * 4. if the result was received by master then go to step 0.1
 * 5. If the message from the master is to quit, then the slave increments its
 *    quit counter 
 * 6. If the quit counter is 3 then the slave terminates by sending
 *    a SLAVE_TO_MASTER_EXITING message to master
 */

/*==============================================================================
 *  SlaveWork
 *=============================================================================*/

static void SlaveWork (void * inArg){

    RefThreadData ThreadInfo = (RefThreadData)inArg;

    int CommSize;
    int ProcRank;
    MPI_Comm_size(MPI_COMM_WORLD, &CommSize);
    MPI_Comm_rank(MPI_COMM_WORLD, &ProcRank);
    MPI_Status status;
    MPI_Request SendReq;

    IndexSt Index;
    long i, StartIndex, StopIndex;

    double * NodeIntegralOutput;
    double  NodeIntegralTemp = 0;
    NodeIntegralOutput = new double [1];
    memset (NodeIntegralOutput, 0, sizeof(NodeIntegralOutput[0]));

    double y, x, FuncOutput;
    int QuitCounter = 0;

    MPI_Datatype StructOfIndex;
    /* create a single struct */
    {
        int NoOfBlocks = 2;               /* number of Blocks in the struct */
        int Blocks[2] = {1, 1};   /* set up 2 Blocks */
        MPI_Datatype Types[2] = {    /* index internal Types */
            MPI_LONG,
            MPI_LONG,
        };
        MPI_Aint Disp[2] = {          /* internal displacements */
            offsetof(IndexSt, StartIndex),
            offsetof(IndexSt, StopIndex),
        };

        MPI_Type_create_struct(NoOfBlocks, Blocks, Disp, Types, &StructOfIndex);
        MPI_Type_commit(&StructOfIndex);
    }

    while (1){

        MPI_Recv (&Index, 1, StructOfIndex, MASTER_NODE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

        if (status.MPI_TAG == MASTER_TO_SLAVE_WORK_AVAILABLE) {

            DLOG (C_VERBOSE, "Node[%d] Doing Work. Computing integration\n", ProcRank);
            StartIndex = Index.StartIndex;
            StopIndex = Index.StopIndex;
            DLOG (C_VERBOSE, "Node[%d] StartIndex = %d StopIndex = %d\n", ProcRank, StartIndex, StopIndex);

            /*  y = (a - b)/n */
            y = (ThreadInfo->UpperBound - ThreadInfo->LowerBound)/ThreadInfo->NoOfPoints;
            NodeIntegralTemp = 0;
            for (i = StartIndex; i< StopIndex; i++) {
                x = (ThreadInfo->LowerBound + ((i + 0.5)* y ));
                FuncOutput = (double) ThreadInfo->FuncToIntegrate (x,ThreadInfo->Intensity );
                FuncOutput = FuncOutput * y ;
                NodeIntegralTemp += (double) FuncOutput;
            }
            /* Ideally NodeIntegralOutput has to be an array, as we are using MPI_Isend,
             * and there should be a MPI_Wait() but since the NoOfPoints is large we can assume that master receives 
             * the value sent by slave before the slave finishes computing the next iteration.
             */

            NodeIntegralOutput[0] = NodeIntegralTemp;
            DLOG (C_VERBOSE, "Node[%d] Sending integration %f y = %f\n", ProcRank, NodeIntegralOutput[0], y);
            MPI_Isend (NodeIntegralOutput, 1, MPI_DOUBLE, MASTER_NODE,
                    SLAVE_TO_MASTER_REQ_WORK, MPI_COMM_WORLD, &SendReq);





        }else if (status.MPI_TAG == MASTER_TO_SLAVE_QUIT) {
            QuitCounter++;
            DLOG (C_VERBOSE, "Node[%d] Quit message received from master. QuitCounter = %d\n", ProcRank,QuitCounter);

            if (QuitCounter >= 3 ){
                NodeIntegralOutput[0] = 0;
                DLOG (C_VERBOSE, "Node[%d] Node exiting\n", ProcRank);
                MPI_Isend (NodeIntegralOutput, 1, MPI_DOUBLE, MASTER_NODE,
                        SLAVE_TO_MASTER_EXITING, MPI_COMM_WORLD, &SendReq);

                break;
            }
        }
    }

    MPI_Type_free(&StructOfIndex);

    delete[] NodeIntegralOutput;
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

/*==============================================================================
 *  GetFreeChunkIndex
 *=============================================================================*/

int GetFreeChunkIndex (int Node, int * ChunkIndex)
{
    int IndexToBeReused;
    IndexToBeReused = ChunkIndex[Node];
    /* the 2D Index[][] has only 3 columns */
    if (ChunkIndex[Node] >= 2)
        ChunkIndex[Node] = 0;
    else
        ChunkIndex[Node]++;

    return IndexToBeReused;
}
