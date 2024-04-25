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

struct Seq {
  int direction; // 0 for ascending order, 1 for descending order
  int size; // size of the sequence
};

static void help (char *cmdName) {
  fprintf (stderr, "OPTIONS:\n"
            "  -d      --- direction (0 for ascending || 1 for descending) \n"
            "  -h      --- print this help\n");
}

static double get_delta_time(void) {
  static struct timespec t0, t1;

  t0 = t1;
  if (clock_gettime(CLOCK_MONOTONIC, &t1) != 0) {
    perror("clock_gettime");
    exit(EXIT_FAILURE);
  }

  return (double) (t1.tv_sec - t0.tv_sec) + 1.0e-9 * (double) (t1.tv_nsec - t0.tv_nsec) ;
}

int verifySequenceCorrectness(const int *sequence, int size, int direction) {
  for (int i = 0; i < size - 1; i++) {
    if ((direction == 0 && sequence[i] > sequence[i + 1]) || 
      (direction == 1 && sequence[i] < sequence[i + 1])) {
      return 0;
    }
  }
  return 1;
}


int main(int argc, char *argv[]) {
  MPI_Init (&argc, &argv);
  int rank, numProc, currentProc;

  MPI_Comm_rank (MPI_COMM_WORLD, &rank); 
  MPI_Comm_size (MPI_COMM_WORLD, &numProc);

  struct Seq seq;
  int *sequence = NULL;
  MPI_Group nowGroup, nextGroup;
  MPI_Comm nowComm, nextComm;

  int Group[8]; // 8 max number of processes

  if ((numProc & (numProc - 1)) != 0) {
    if (rank == 0) {
      fprintf(stderr, "Number of processes must be 1 or a power of 2 (2,4,8)\n");
    }
    MPI_Finalize();
    return EXIT_FAILURE;
  }

  if (rank == 0){
    int opt;
    char* fileName;
    int direction = 0;

    do { 
      switch ((opt = getopt (argc, argv, "d:h"))) {
        case 'd':
          if (atoi(optarg) != 0 && atoi(optarg) != 1) {
            fprintf(stderr, "%s: invalid sort direction\n", basename(argv[0]));
            help(basename(argv[0]));
            exit(EXIT_FAILURE);
          }
          direction = (int) atoi (optarg);
          break;

        case 'h':
          help (basename (argv[0]));
          return EXIT_SUCCESS;

        case '?': 
          fprintf (stderr, "%s: invalid option\n", basename (argv[0]));
          help (basename (argv[0]));
          return EXIT_FAILURE;

        case -1:  break;
      }
    } while (opt != -1);

    if (argc < 4){ 
      fprintf (stderr, "Invalid format!\n");
      help (basename (argv[0]));
      return EXIT_FAILURE;
    }

    fileName = argv[optind];

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

  // Create Communication Groups for the processes
  nowComm = MPI_COMM_WORLD;
  MPI_Comm_group (nowComm, &nowGroup);

  // Number of current processes
  currentProc = numProc;

  // Create the group of processes
  for (int i = 0; i < numProc; i++) {
    Group[i] = i;
  }

  if(rank == 0) {
    get_delta_time();
  }

  for (int i = 0; i <= numIterations; i++) {
    if (i != 0) {
      MPI_Group_incl(nowGroup, currentProc, Group, &nextGroup); // 
      MPI_Comm_create(nowComm, nextGroup, &nextComm);

      nowComm = nextComm;
      nowGroup = nextGroup;

      // If the rank is greater than the current number of processes, then the process is not needed
      if (rank >= currentProc) {
        free(buf);
        MPI_Finalize();
        return EXIT_SUCCESS;
      }
    }

    MPI_Comm_size (nowComm, &numProc);
    subsequenceSize = seq.size / currentProc;

    MPI_Scatter(sequence, subsequenceSize, MPI_INT, buf, subsequenceSize, MPI_INT, 0, nowComm);

    // Sort subsequence
    if (i == 0) {
      printf("Rank %d: Sorting %d elements\n", rank, subsequenceSize);
      bitonicSort(buf, subsequenceSize, seq.direction);
    } else {
      printf("Rank %d: Merging %d elements\n", rank, subsequenceSize);
      bitonicMerge(buf, subsequenceSize, seq.direction);
    }

    MPI_Gather(buf, subsequenceSize, MPI_INT, sequence, subsequenceSize, MPI_INT, 0, nowComm);
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