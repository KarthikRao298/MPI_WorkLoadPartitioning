# Objectives
The goal is to implement a static, master-worker and a advanced master-worker loop scheduler and use it to compute numerical integration.

#### Static
1. The first MPI process should take the first N/P iterations of the loop, the second should take the next N/P iterations of the loop, etc.. The partial integration should be accumulated on rank 0 so that it can print the correct answer to stdout and the time it took to stderr.
2. Run and time the program on cluster using 1, 2, 4, 8, 16, 32 cores and plot speedup charts.

#### Master-worker
1. A master-worker system enables dynamic scheduling of applications. The idea is that one of the MPI processes (usually rank 0) will be responsible for giving work to the other MPI processes and collect results. Usually the master process starts by sending one chunk of work to all the workers and when one of the worker provides the result of that chunk of work, the master process will send a new chunk of work to the worker that just completed the work. The workers will wait for a chunk of work to perform, perform that chunk of work, and return the result to the master node.
Adapt the numerical integration to make it schedule the calculation using a master-worker system.

2. Run and time the program on cluster using 1, 2, 4, 8, 16, 32 cores and plot speedup charts.

#### Advanced Master-worker
1. One of the issue with such a master-worker implementation is that the worker process will be idle until the master provides the next chunk. To avoid this, it is common that the master will give more than a single chunk of work (say, 3 chunks of work) to each worker. That way, when a worker sends a result to the master node, the worker still has some chunks of work (here, 2 chunks) to perform.
Adapt the numerical integration code to use advanced scheduling.

2. Run and time the program on cluster using 1, 2, 4, 8, 16, 32 cores and plot speedup charts.
