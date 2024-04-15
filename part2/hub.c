#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {
  int rank, size;

  char msg_send[100], msg_recv[100];

  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);

  if (rank == 0) {
    sprintf(msg_send, "Process 0 (root) ready!");
    printf("%s\n", msg_send);
    for (int i = 1; i < size; i++) {
      MPI_Send(&msg_send, 100, MPI_CHAR, i, 0, MPI_COMM_WORLD);
    }
    for (int i = 1; i < size; i++) {
      MPI_Recv(&msg_recv, 100, MPI_CHAR, i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
      printf("Received from %d: %s\n", i, msg_recv);
    }

  } else {
    MPI_Recv(&msg_recv, 100, MPI_CHAR, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    sprintf(msg_send, "Process %d ready!", rank);
    MPI_Send(&msg_send, 100, MPI_CHAR, 0, 0, MPI_COMM_WORLD);
  }

  MPI_Finalize();
  return EXIT_SUCCESS;
}
