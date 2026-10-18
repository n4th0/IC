// Tarea 2 - "Hola mundo" paralelo con MPI.
// Basado en el ejemplo de la guia de la practica.
//
// Compilar:   mpicc hello_mpi.c -o hello_mpi
// Ejecutar:   mpirun -mca plm_rsh_no_tree_spawn 1 -hostfile <hostfile> -n <N> ./hello_mpi
#define _GNU_SOURCE
#include <stdio.h>
#include "mpi.h"
#include <unistd.h>
#include <stdlib.h>
#include <sched.h>

int sched_getcpu();

int main(int argc, char *argv[])
{
    int numprocs, rank, namelen;
    char processor_name[MPI_MAX_PROCESSOR_NAME];

    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &numprocs);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Get_processor_name(processor_name, &namelen);

    printf(" PID: %d Hello from process %d out of %d on %s processor %d \n",
           getppid(), rank, numprocs, processor_name, sched_getcpu());

    if (rank == 2)
    {
        printf("\xc2\xa1Hola! desde el procesador %d\n", rank);
    }

    MPI_Finalize();
    return 0;
}
