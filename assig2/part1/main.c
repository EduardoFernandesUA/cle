#include <math.h>
#include <mpi.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "./UTF8.h"
#include "mpi_proto.h"

#define BLOCK_SIZE 4096
// #define BLOCK_SIZE 256
#define False 0
#define True !False

struct Node {
  int file;
  uint8_t block[BLOCK_SIZE];
  int startPos;
  int endPos;
  struct Node* next;
};

struct FileCounter {
  int words;
  int consonants;
};

static double get_delta_time(void) {
  static struct timespec t0, t1;
  t0 = t1;
  if (clock_gettime(CLOCK_MONOTONIC, &t1) != 0) {
    perror("clock_gettime");
    exit(1);
  }
  return (double)(t1.tv_sec - t0.tv_sec) +
         1.0e-9 * (double)(t1.tv_nsec - t0.tv_nsec);
}

int min(int a, int b) {
  return a < b ? a : b;
}

int main(int argc, char* argv[]) {
  int rank, nProc, nProcNow;

  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &nProc);
  nProcNow = nProc;

  if (rank == 0) {
    get_delta_time();

    struct Node *head = NULL, *tail = NULL;
    char* files[argc - 1];

    // store files
    for (int i = 0; i < argc - 1; i++) {
      files[i] = argv[i + 1];
    }

    for (int i = 0; i < argc - 1; i++) {
      FILE* fd = fopen(files[i], "rb");
      if (fd == NULL) {
        printf("ERROR opening file: %s\n", files[i]);
        continue;
      }
      fseek(fd, 0, SEEK_END);
      int fd_len = ftell(fd);
      fseek(fd, 0, SEEK_SET);

      int blockStart, blockEnd;
      do {
        // read next 4k utf8s and iterate from the end till the first delimiter
        blockStart = ftell(fd);
        blockEnd = min(blockStart + BLOCK_SIZE - 1, fd_len);  // -1 because there is required space for the \0
        fseek(fd, blockEnd, SEEK_SET);
        endOfPreviusWord(fd);
        blockEnd = ftell(fd);

        // read the block from the file system to the memory
        struct Node* node = (struct Node*)malloc(sizeof(struct Node));
        node->startPos = blockStart;
        node->endPos = blockEnd;
        node->file = i;
        node->next = NULL;
        fseek(fd, blockStart, SEEK_SET);
        for (int i = 0; i < BLOCK_SIZE + 1; i++) {
          node->block[i] = '\0';
        }
        fread(node->block, 1, blockEnd - blockStart, fd);
        node->block[blockEnd - blockStart + 1] = '\0';

        // save it in the next node to be further sent to a worker
        if (tail == NULL) {
          head = tail = node;
        } else {
          head->next = node;
          head = node;
        }

        // wait for next worker request and send him the block calculated
      } while (fd_len > blockStart + BLOCK_SIZE);

      fclose(fd);
    }

    struct Node* onProc[nProc];

    struct FileCounter fileCounter[argc - 1];
    for (int i = 0; i < argc - 1; i++) {
      fileCounter[i].words = 0;
      fileCounter[i].consonants = 0;
    }

    MPI_Request requestWorkers;
    MPI_Request request;
    MPI_Status status;
    int worker_ack;
    MPI_Irecv(&worker_ack, 1, MPI_INT, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &request);
    while (nProcNow > 1) {
      int flag;
      MPI_Test(&request, &flag, &status);

      if (flag) {

        int ack;
        if (status.MPI_SOURCE == -2) {  // broadcast ack by it self
          sleep(1);
          continue;

        } else if (worker_ack == 0 && tail == NULL) {
          ack = 0;
          MPI_Isend(&ack, 1, MPI_INT, status.MPI_SOURCE, 0, MPI_COMM_WORLD, &requestWorkers);
          nProcNow--;

        } else if (worker_ack == 0 && tail != NULL) {
          ack = 1;
          MPI_Isend(&ack, 1, MPI_INT, status.MPI_SOURCE, 0, MPI_COMM_WORLD, &requestWorkers);
          onProc[status.MPI_SOURCE] = tail;
          MPI_Isend(&tail->block, BLOCK_SIZE, MPI_CHAR, status.MPI_SOURCE, 0, MPI_COMM_WORLD, &requestWorkers);
          tail = tail->next;

        } else if (worker_ack == 1) {
          struct FileCounter workerData;
          MPI_Recv((char*)&workerData, sizeof(struct FileCounter), MPI_BYTE, status.MPI_SOURCE, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

          int file_idx = onProc[status.MPI_SOURCE]->file;
          fileCounter[file_idx].words += workerData.words;
          fileCounter[file_idx].consonants += workerData.consonants;
        } else {
          printf("SHOULD NOT BE POSSIBLE TO REACH\n");
          exit(0);
        }

        // send the recv ready for the next process
        if (nProcNow > 1)
          MPI_Irecv(&worker_ack, 1, MPI_INT, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &request);
      }
    }

    MPI_Wait(&request, &status);

    for (int i = 0; i < argc - 1; i++) {
      printf("\nFile Name: %s\n", argv[i + 1]);
      printf("Total Number of Words = %d\n", fileCounter[i].words);
      printf("Total number of words with at least two instances of the same consonant = %d\n", fileCounter[i].consonants);
    }
    printf("\nTime: %fs", get_delta_time());

  } else {
    MPI_Status status;
    int n = 0;
    uint8_t buf[BLOCK_SIZE];

    while (1) {
      n = 0;
      MPI_Send(&n, 1, MPI_INT, 0, 0, MPI_COMM_WORLD);
      MPI_Recv(&n, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, &status);
      if (n == 0) {
        break;  // end process

      } else if (n == 1) {  // calculate new block
        MPI_Recv(buf, BLOCK_SIZE, MPI_CHAR, 0, 0, MPI_COMM_WORLD, &status);
        struct FileCounter workerData = {0, 0};
        countBuffer(buf, &workerData.words, &workerData.consonants);
        n = 1;
        MPI_Send(&n, 1, MPI_INT, 0, 0, MPI_COMM_WORLD);
        MPI_Send((char*)&workerData, sizeof(struct FileCounter), MPI_BYTE, 0, 0, MPI_COMM_WORLD);
      }
    }
  }

  MPI_Finalize();

  return EXIT_SUCCESS;
}
