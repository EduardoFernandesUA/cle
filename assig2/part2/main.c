/* USAGE: 
mpicc -Wall -O3 -o out main.c readFile.c bitonicSort.c
mpiexec -n 8 ./out -d 1 dataset2/datSeq16M.bin
*/
#include <stdio.h>
#include <mpi.h>
#include <stdlib.h>
#include <libgen.h>
#include <unistd.h>
#include <time.h>
#include <math.h>
#include "readFile.h"
#include "bitonicSort.h"

/**
 * @brief A structure to hold a sequence's direction and size.
 */
struct Seq {
  int direction; // 0 for ascending order, 1 for descending order
  int size; // size of the sequence
};

/**
 * @brief Prints the help message for the command.
 *
 * @param cmdName The name of the command.
 */
static void help (char *cmdName) {
  fprintf (stderr, "OPTIONS:\n"
            "  -d      --- direction (0 for ascending || 1 for descending) \n"
            "  -h      --- print this help\n");
}

/**
 * @brief Calculates the time difference between two consecutive calls to this function.
 *
 * @return The time difference in seconds.
 */
static double get_delta_time(void) {
  static struct timespec t0, t1;

  t0 = t1;
  if (clock_gettime(CLOCK_MONOTONIC, &t1) != 0) {
    perror("clock_gettime");
    exit(EXIT_FAILURE);
  }

  return (double) (t1.tv_sec - t0.tv_sec) + 1.0e-9 * (double) (t1.tv_nsec - t0.tv_nsec) ;
}

/**
 * @brief Verifies if a sequence of integers is sorted in a given direction.
 *
 * @param sequence The sequence of integers to verify.
 * @param size The number of elements in the sequence.
 * @param direction The expected direction of the sequence, 0 for ascending and 1 for descending.
 * @return 1 if the sequence is sorted in the given direction, 0 otherwise.
 */
int verifySequenceCorrectness(const int *sequence, int size, int direction) {
  for (int i = 0; i < size - 1; i++) {
    if ((direction == 0 && sequence[i] > sequence[i + 1]) || 
      (direction == 1 && sequence[i] < sequence[i + 1])) {
      return 0;
    }
  }
  return 1;
}

/**
 * @brief Processes the command line arguments.
 *
 * @param argc The number of arguments.
 * @param argv The arguments.
 * @param direction The sorting direction.
 * @param fileName The name of the file to read the sequence from.
 */
void process_command_line(int argc, char *argv[], int *direction, char **fileName) {
  int opt;
  *direction = 0;

  do { 
    switch ((opt = getopt (argc, argv, "d:h"))) {
      case 'd':
        if (atoi(optarg) != 0 && atoi(optarg) != 1) {
          fprintf(stderr, "%s: invalid sort direction\n", basename(argv[0]));
          help(basename(argv[0]));
          exit(EXIT_FAILURE);
        }
        *direction = (int) atoi (optarg);
        break;

      case 'h':
        help (basename (argv[0]));
        exit(EXIT_SUCCESS);

      case '?': 
        fprintf (stderr, "%s: invalid option\n", basename (argv[0]));
        help (basename (argv[0]));
        exit(EXIT_FAILURE);

      case -1:  break;
    }
  } while (opt != -1);

  if (argc < 4){ 
    fprintf (stderr, "Invalid format!\n");
    help (basename (argv[0]));
    exit(EXIT_FAILURE);
  }

  *fileName = argv[optind];
}

int main(int argc, char *argv[]) {
  MPI_Init (NULL, NULL);
  int rank, numProc;
  MPI_Comm_rank (MPI_COMM_WORLD, &rank); 
  MPI_Comm_size (MPI_COMM_WORLD, &numProc);

  struct Seq seq;
  MPI_Group currentGroup, nextGroup;
  MPI_Comm currentComm, nextComm;
  int *sequence = NULL;

  if ((numProc & (numProc - 1)) != 0) {
    if (rank == 0) {
      fprintf(stderr, "Number of processes must be 1 or a power of 2 (2,4,8)\n");
    }
    MPI_Finalize();
    return EXIT_FAILURE;
  }

  if (rank == 0){
    char* fileName;
    int direction;
    process_command_line(argc, argv, &direction, &fileName);

    printf ("File: %s\n", fileName);
    printf ("%s Sorting...\n", direction == 0 ? "Ascending" : "Descending" );

    int size = readSequence(fileName, &sequence);

    seq.direction = direction;
    seq.size = size;

    // Send to the other processes
    MPI_Bcast(&seq, 1, MPI_2INT, 0, MPI_COMM_WORLD);
  } else {
    // Receive from the other processes
    MPI_Bcast(&seq, 1, MPI_2INT, 0, MPI_COMM_WORLD);
  }

  int *buf = (int *) malloc(seq.size * sizeof(int));

  int numIterations = (int) log2(numProc);
  
  int subsequenceSize = 0;

  int Group[8]; // 8 max number of processes
  int currentProc = numProc;

  // Create Communication Groups for the processes
  currentComm = MPI_COMM_WORLD;
  MPI_Comm_group (currentComm, &currentGroup);

  // Create the group of processes
  for (int i = 0; i < numProc; i++) {
    Group[i] = i;
  }

  if(rank == 0) {
    get_delta_time();
  }

  for (int i = 0; i <= numIterations; i++) {
    if (i != 0) {
      MPI_Group_incl(currentGroup, currentProc, Group, &nextGroup); 
      MPI_Comm_create(currentComm, nextGroup, &nextComm);

      currentComm = nextComm;
      currentGroup = nextGroup;

      if (rank >= currentProc) {
        free(buf);
        MPI_Finalize();
        return EXIT_SUCCESS;
      }
    }

    MPI_Comm_size (currentComm, &numProc);
    subsequenceSize = seq.size / currentProc;

    // Distribute the sequence through the processes
    MPI_Scatter(sequence, subsequenceSize, MPI_INT, buf, subsequenceSize, MPI_INT, 0, currentComm);

    // Sort subsequence
    if (i == 0) {
      printf("Rank %d: Sorting %d elements\n", rank, subsequenceSize);
      bitonicSort(buf, subsequenceSize, seq.direction);
    } else {
      printf("Rank %d: Merging %d elements\n", rank, subsequenceSize);
      bitonicMerge(buf, subsequenceSize, seq.direction);
    }

    // Wait for all processes to finish sorting
    MPI_Barrier(currentComm); 

    MPI_Gather(buf, subsequenceSize, MPI_INT, sequence, subsequenceSize, MPI_INT, 0, currentComm);
    currentProc = numProc / 2;
  }

  // Validation
  if (rank == 0) {
    if (verifySequenceCorrectness(sequence, seq.size, seq.direction)) {
      printf("Sorted!\n");
    } else {
      printf("Not Sorted!\n");
    }
    printf("Time to bitonic sort: %fs\n", get_delta_time());
  }

  free(buf);
  free(sequence);
  
  MPI_Finalize();

  return EXIT_SUCCESS;
}