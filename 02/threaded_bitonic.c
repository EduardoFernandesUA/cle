#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

struct thr_st1 {
  int id;
  uint32_t thr_c;
  uint32_t *buf;
  uint32_t n;
};

void caps(uint32_t *a, uint32_t *b) {
  uint32_t temp;
  if (*a > *b) {
    temp = *a;
    *a = *b;
    *b = temp;
  }
}
void bitonic(uint32_t size, uint32_t *n) {
  uint i, j, k;
  uint t_count = size / 2;
  uint log2size = -1;
  uint32_t a, b;

  i = size;
  while (i != 0) {
    i >>= 1;
    log2size++;
  }

  uint32_t j0 = 1, j1 = 2, step = 0, jj0, jj1;
  for (j = 1; j <= log2size; j++) {
    for (i = 0; i < t_count; i++) {
      a = (i / j0) * j1 + i % j0;
      b = (i / j0) * j1 + j1 - i % j0 - 1;
      caps(&n[a], &n[b]);
    }

    // for (k = step; k > 0; k--) {
    for (k = 1; k <= step; k++) {
      for (i = 0; i < t_count; i++) {
        jj0 = j0 >> k;
        jj1 = j1 >> k;
        a = (i / jj0) * jj1 + i % jj0;
        b = a + jj0;
        caps(&n[a], &n[b]);
      }
    }

    j0 <<= 1;
    j1 <<= 1;
    step += 1;
  }
}

int readCmdArgs(int argc, char **argv, FILE **fd, int *thr_count) {
  if (argc != 3) {
    printf("Usage: %s <thread count> <input_file.bin>\n", argv[0]);
    return 1; // Error 1 - invalid count of arguments
  }

  // Extract filename and integer from command line arguments
  *thr_count = atoi(argv[2]);

  // Error handling for invalid integer input
  if (*thr_count == 0 && argv[2][0] != '0') {
    printf("Invalid integer input\n");
    return 2; // Return error code 2 indicating invalid integer
  }

  // Error handling for invalid file
  *fd = fopen(argv[1], "r");
  if (*fd == NULL) {
    printf("Error opening file %s\n", argv[1]);
    return 3; // Return error code 3 indicating file opening failure
  }

  return 0;
}

pthread_mutex_t mut;
pthread_cond_t cond;
int locked = 0;
int c = 0;

void bitonic1(uint32_t *buf, uint32_t i, uint32_t size, uint32_t step, uint32_t j) {
  uint32_t j1 = j << 1;
  uint32_t imj = i % j;
  uint32_t idj = (i / j) * j1;

  uint32_t n1 = idj + imj;
  uint32_t n2 = idj + j1 - imj - 1;

  caps(&buf[n1], &buf[n2]);
}

void bitonic2(uint32_t *buf, uint32_t i, uint32_t size, uint32_t step, uint32_t j) {
  if (i >= size / 2)
    return;
  uint32_t j1 = j << 1;

  uint32_t n1 = (i / j) * j1 + i % j;
  uint32_t n2 = n1 + j;

  caps(&buf[n1], &buf[n2]);
}

void barrier(int thr_c) {
  pthread_mutex_lock(&mut);
  c++;
  if (c == thr_c) {
    pthread_cond_broadcast(&cond);
    c = 0;
  } else {
    pthread_cond_wait(&cond, &mut);
  }
  pthread_mutex_unlock(&mut);
}

void *worker1(void *args) {
  struct thr_st1 *st = (struct thr_st1 *)args;
  uint32_t indxPerThr = (st->n / 2) / st->thr_c;

  uint32_t nsteps = -1;
  for (uint32_t i = st->n; i != 0; i >>= 1) {
    nsteps++;
  }
  printf("Nsteps: %d, indxPerThr %d\n", nsteps, indxPerThr);

  uint32_t j = 1;
  for (uint32_t step = 0; step < nsteps; step++) {
    for (uint32_t i = 0; i < indxPerThr; i++) {
      bitonic1(st->buf, indxPerThr * st->id + i, st->n, step, j);
    }
    barrier(st->thr_c);

    for (uint32_t k = 1; k <= step; k++) {
      for (uint32_t i = indxPerThr * st->id; i < indxPerThr * (st->id + 1); i++) {
        bitonic2(st->buf, i, st->n, step, j >> k);
      }
      barrier(st->thr_c);
    }

    j <<= 1;
  }

  return 0;
}

void threadedBitonic1(uint32_t thr_c, uint32_t n, uint32_t *buf) {
  struct thr_st1 thr_args[thr_c];
  pthread_t thr[thr_c];
  pthread_mutex_init(&mut, NULL);
  pthread_cond_init(&cond, NULL);

  // startup threads
  for (int i = 0; i < thr_c; i++) {
    thr_args[i].id = i;
    thr_args[i].thr_c = thr_c;
    thr_args[i].buf = buf;
    thr_args[i].n = n;
    pthread_create(&thr[i], NULL, worker1, (void *)&thr_args[i]);
  }

  for (int i = 0; i < thr_c; i++) {
    pthread_join(thr[i], NULL);
  }

  pthread_mutex_destroy(&mut);
  pthread_cond_destroy(&cond);
}

int main(int argc, char *argv[]) {
  int err;

  FILE *fd;
  int thr_count;
  err = readCmdArgs(argc, argv, &fd, &thr_count);
  if (err) {
    return err;
  }
  printf("Threads: %d\n", thr_count);
  printf("File: %s\n", argv[1]);

  uint32_t n;
  fread(&n, 4, 1, fd);
  // n = 8; // debug
  printf("Numbers Count: %d\n", n);

  uint32_t *buf = (uint32_t *)malloc(n * sizeof(uint32_t));
  // load all numbers to memory
  for (int i = 0; i < n; i++) {
    fread(&buf[i], 4, 1, fd);
  }
  // uint32_t buf[8] = {7, 6, 5, 4, 3, 2, 1, 0};

  // uncomment block below to test cpu bitonic (around 9s)
  // clock_t start_clk = clock();
  // bitonic(n, buf);
  // printf("Time to sort: %f\n", (double)(clock() - start_clk) / CLOCKS_PER_SEC);

  struct timespec start, end;
  clock_gettime(CLOCK_MONOTONIC, &start);
  threadedBitonic1(thr_count, n, buf);
  clock_gettime(CLOCK_MONOTONIC, &end);
  printf("Time to sort: %fs\n", (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9);

  // verify correcteness
  for (int i = 0; i < n - 1; i++) {
    if (buf[i] > buf[i + 1]) {
      printf("Not Sorted\n");
      break;
    }
  }

  // for (int i = 0; i < n; i++) {
  //   printf("%d\n", buf[i]);
  // }

  // frees
  fclose(fd);
  // free(buf);

  return 0;
}
