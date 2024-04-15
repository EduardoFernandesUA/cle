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

  sprintf(msg_send, "I am here (%d)!", rank);
  if (rank == 0) {
    // MPI_Send(&msg_send, 100, MPI_CHAR, (rank + 1) % size, 0, MPI_COMM_WORLD);
    // MPI_Recv(&msg_recv, 100, MPI_CHAR, (rank - 1 + size) % size, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    MPI_Sendrecv(&msg_send, 100, MPI_CHAR, (rank + 1) % size, 0, &msg_recv, 100, MPI_CHAR, (rank - 1 + size) % size, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

  } else {
    MPI_Recv(&msg_recv, 100, MPI_CHAR, (rank - 1 + size) % size, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    MPI_Send(&msg_send, 100, MPI_CHAR, (rank + 1) % size, 0, MPI_COMM_WORLD);
  }

  printf("msg_recv in %d from %d: %s\n", rank, (rank - 1 + size) % size, msg_recv);

  MPI_Finalize();
  return EXIT_SUCCESS;
}
