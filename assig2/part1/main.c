#include <math.h>
#include <mpi.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "./UTF8.h"
#include "mpi_proto.h"

#define BLOCK_SIZE 4096
#define False 0
#define True !False

struct Node {
  int file;
  uint8_t block[BLOCK_SIZE];
  struct Node* next;
};

int min(int a, int b) {
  return a < b ? a : b;
}

int main(int argc, char* argv[]) {
  MPI_Comm presentComm, nextComm;
  MPI_Group presentGroup, nextGroup;
  int gMemb[8];
  int rank, nProc, nProcNow;
  int *sendData = NULL, *recData;

  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &nProc);
  nProcNow = nProc;

  if (rank == 0) {
    struct Node *head = NULL, *tail = NULL;
    char* files[argc - 1];

    // store files
    for (int i = 0; i < argc - 1; i++) {
      files[i] = argv[i + 1];
    }

    MPI_Request request;
    int worker_ack;
    MPI_Irecv(&worker_ack, 1, MPI_INT, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &request);

    for (int i = 0; i < argc - 1; i++) {
      FILE* fd = fopen(files[i], "rb");
      if (fd == NULL) {
        printf("ERROR opening file: %s\n", files[i]);
        continue;
      }
      printf("OPENED FILE\n");
      fseek(fd, 0, SEEK_END);
      int fd_len = ftell(fd);
      fseek(fd, 0, SEEK_SET);

      printf("FILE %s: %d\n", files[i], fd_len);
      int nextBlockStart, blockStart, blockEnd;
      do {
        // read next 4k utf8s and iterate from the end till the first delimiter
        blockStart = ftell(fd);
        blockEnd = min(blockStart + BLOCK_SIZE - 1, fd_len);  // -1 because there is required space for the \0
        printf("\nBLOCK FROM BEFORE: %d to %d\n", blockStart, blockEnd);
        fseek(fd, blockEnd, SEEK_SET);
        endOfPreviusWord(fd);
        blockEnd = ftell(fd);
        printf("BLOCK FROM AFTER: %d to %d\n", blockStart, blockEnd);

        // read the block from the file system to the memory
        struct Node* node = (struct Node*)malloc(sizeof(struct Node));
        node->file = i;
        node->next = NULL;
        fseek(fd, blockStart, SEEK_SET);
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
    MPI_Request requestWorkers;
    MPI_Status status;
    int n;
    while (nProcNow > 1) {
      int flag;
      MPI_Test(&request, &flag, &status);

      if (flag) {
        int ack;
        if (status.MPI_SOURCE == -2) {  // broadcast ack by it self
          continue;
        }
        if (tail == NULL) {
          ack = 0;
          MPI_Isend(&ack, 1, MPI_INT, status.MPI_SOURCE, 0, MPI_COMM_WORLD, &requestWorkers);
          nProcNow--;
          continue;
        }

        ack = 1;
        MPI_Isend(&ack, 1, MPI_INT, status.MPI_SOURCE, 0, MPI_COMM_WORLD, &requestWorkers);
        onProc[status.MPI_SOURCE] = tail;
        MPI_Isend(&tail->block, BLOCK_SIZE, MPI_CHAR, status.MPI_SOURCE, 0, MPI_COMM_WORLD, &requestWorkers);
        tail = tail->next;

        MPI_Irecv(&n, 1, MPI_INT, status.MPI_SOURCE, 0, MPI_COMM_WORLD, &request);
      }
    }

  } else {
    int words = 0, consonants = 0;
    printf("Worker %d\n", rank);
    MPI_Status status;
    int n = 0;
    uint8_t buf[BLOCK_SIZE];

    while (1) {
      n = 0;
      MPI_Send(&n, 1, MPI_INT, 0, 0, MPI_COMM_WORLD);
      MPI_Recv(&n, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, &status);
      if (n == 0)
        break;

      MPI_Recv(buf, BLOCK_SIZE, MPI_CHAR, 0, 0, MPI_COMM_WORLD, &status);
      countBuffer(buf, &words, &consonants);
      printf("\n--------------------------------------------------------------------");
      printf("\nCOUNTERS: %d   %d\n", words, consonants);
      printf("--------------------------------------------------------------------\n");
    }
  }

  MPI_Finalize();

  return EXIT_SUCCESS;
}
