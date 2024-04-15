#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
  int stat, rank, size;
  char errMessage[100];
  int errMessLen;

  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);
  if (rank == 1) {
    MPI_Comm_set_errhandler(MPI_COMM_WORLD, MPI_ERRORS_RETURN);
    if ((stat = MPI_Init(&argc, &argv) & 0xFF) != MPI_SUCCESS) {
      switch (stat) {
      case MPI_ERR_COMM:
        printf("Invalid communicator!\n");
        break;
      case MPI_ERR_OTHER:
        MPI_Error_string(stat, errMessage, &errMessLen);
        printf("%s: MPI_Init called more than once!\n", errMessage);
        break;
      }
      MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
    }
  }
  printf("Hello! I am %d of %d.\n", rank, size);
  MPI_Finalize();
  return EXIT_SUCCESS;
}
