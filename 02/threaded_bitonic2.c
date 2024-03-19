#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static double get_delta_time(void) {
  static struct timespec t0, t1;
  t0 = t1;
  if (clock_gettime(CLOCK_MONOTONIC, &t1) != 0) {
    perror("clock_gettime");
    exit(1);
  }
  return (double)(t1.tv_sec - t0.tv_sec) + 1.0e-9 * (double)(t1.tv_nsec - t0.tv_nsec);
}

void caps(uint32_t *a, uint32_t *b) {
  uint32_t temp;
  if (*a > *b) {
    temp = *a;
    *a = *b;
    *b = temp;
  }
}

int readCmdArgs(int argc, char **argv, FILE **fd, int *thr_count) {
  if (argc != 3) {
    printf("Usage: %s <thread count> <input_file.bin>\n", argv[0]);
    return 1; // Error 1 - invalid count of arguments
  }

  // Extract filename and integer from command line arguments
  *thr_count = atoi(argv[2]);
  printf("Invalid integer input\n");
  // Error handling for invalid integer input
  if (*thr_count == 0 && argv[2][0] != '0') {
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

void merge_sort_asc(uint32_t *buf, uint32_t start, uint32_t size) { // dir(0) -> ascending
  uint32_t i, k;
  buf += start;
  uint32_t *ntemp = (uint32_t *)malloc(size * 4);

  uint32_t l, r, step, halfstep;
  for (step = 2; step <= size; step <<= 1) {
    halfstep = step >> 1;

    for (i = 0; i < size; i += step) {
      k = i;
      l = i;
      r = i + halfstep;
      while (l < i + halfstep && r < i + step) {
        if (buf[l] < buf[r]) {
          ntemp[k++] = buf[l++];
        } else {
          ntemp[k++] = buf[r++];
        }
      }
      while (l < i + halfstep) {
        ntemp[k++] = buf[l++];
      }
      while (r < i + step) {
        ntemp[k++] = buf[r++];
      }
    }
    for (i = 0; i < size; i++) {
      buf[i] = ntemp[i];
    }
  }

  free(ntemp);
}
void merge_sort_desc(uint32_t *buf, uint32_t start, uint32_t size) {
  uint32_t i, k;
  buf += start;
  uint32_t *ntemp = (uint32_t *)malloc(size * sizeof(uint32_t));

  uint32_t l, r, step, halfstep;
  for (step = 2; step <= size; step <<= 1) {
    halfstep = step >> 1;

    for (i = 0; i < size; i += step) {
      k = i;
      l = i;
      r = i + halfstep;
      while (l < i + halfstep && r < i + step) {
        if (buf[l] > buf[r]) {
          ntemp[k++] = buf[l++];
        } else {
          ntemp[k++] = buf[r++];
        }
      }
      while (l < i + halfstep) {
        ntemp[k++] = buf[l++];
      }
      while (r < i + step) {
        ntemp[k++] = buf[r++];
      }
    }
    for (i = 0; i < size; i++) {
      buf[i] = ntemp[i];
    }
  }

  free(ntemp);
}

pthread_mutex_t mut;
pthread_cond_t cond;
int locked = 0;
int c = 0;
void sync(int thr_c) {
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
void bitonic_sort(uint32_t *buf, uint32_t size, uint32_t i, uint32_t thr_c) {
  uint32_t k = -1, n, u, m, t;
  n = size;
  for (int i = 0; n != 0; n >>= 1) {
    k++;
  }

  uint32_t v = size >> 1;
  uint32_t nL = 1;
  for (uint32_t m = 0; m < k; m++) {
    n = 0;
    u = 0;
    while (n < nL) {
      for (t = 0; t < v; t++)
        caps(&buf[t + u], &buf[t + u + v]);
      u += (v << 1);
      n += 1;
    }
    v >>= 1;
    nL <<= 1;
  }
}
void bitonic_sort2(uint32_t *buf, uint32_t N, uint32_t thr_i, uint32_t thr_c) {

  uint32_t i_size = (N / 2) / thr_c;
  uint32_t i_start = i_size * thr_i;
  uint32_t i_end = i_start + i_size;
  // printf("BAH %d %d\n", i_start, i_end);

  // calculate logN
  uint32_t k = -1;
  for (uint32_t i = N; i != 0; i >>= 1)
    k++;

  // this first pass is needed to merge ascending sequences
  for (uint32_t c = i_start; c < i_end; c++) {
    caps(&buf[c], &buf[N - c - 1]);
  }

  sync(thr_c);

  for (uint32_t m = 1; m < k; m++) {
    uint32_t nL = 1 << m;
    uint32_t v = N >> (m + 1);
    uint32_t vl1 = N >> m;
    // printf("nL: %d  v: %d\n", nL, v);

    // for (uint32_t c = 0; c < (N / 2); c++) {
    for (uint32_t c = i_start; c < i_end; c++) {
      uint32_t t = c % v;
      uint32_t u = (c * 2) / vl1 * vl1;

      caps(&buf[t + u], &buf[t + u + v]);

      // printf("%d(%d, %d) ", t, a, b);
    }

    sync(thr_c);
    // printf("\n");
  }
}

struct g_worker_st {
  uint32_t n;
  uint32_t thr_c;
  uint32_t *buf;
};
struct worker_st {
  int id;
  struct g_worker_st *worker_shm;
};

void *worker(void *args) {
  struct worker_st *st = (struct worker_st *)args;

  uint32_t seq_s = (st->worker_shm->n / st->worker_shm->thr_c);
  uint32_t seq_i = seq_s * st->id;

  // merge sort portion of sequence
  // if (st->id % 2 == 0) {
  //   merge_sort_asc(st->worker_shm->buf, seq_i, seq_s);
  // } else {
  //   merge_sort_desc(st->worker_shm->buf, seq_i, seq_s);
  // }
  merge_sort_asc(st->worker_shm->buf, seq_i, seq_s);

  sync(st->worker_shm->thr_c);

  // bitonic_sort2(st->worker_shm->buf, st->worker_shm->n / 2, st->id, st->worker_shm->thr_c);
  // bitonic_sort2(st->worker_shm->buf + st->worker_shm->n / 2, st->worker_shm->n / 2, st->id, st->worker_shm->thr_c);
  // bitonic_sort2(st->worker_shm->buf, st->worker_shm->n / 2, st->id, st->worker_shm->thr_c);
  // bitonic_sort2(st->worker_shm->buf + st->worker_shm->n / 2, st->worker_shm->n / 2, st->id, st->worker_shm->thr_c);
  for (uint32_t i = st->worker_shm->thr_c >> 1; i > 0; i >>= 1) {
    for (int j = 0; j < i; j++) {
      bitonic_sort2(st->worker_shm->buf + (st->worker_shm->n / i) * j, st->worker_shm->n / i, st->id, st->worker_shm->thr_c);
    }
  }

  // bitonic_sort2(st->worker_shm->buf, st->worker_shm->n / 2, st->id, st->worker_shm->thr_c);
  // bitonic_sort2(st->worker_shm->buf + st->worker_shm->n / 2, st->worker_shm->n / 2, st->id, st->worker_shm->thr_c);
  //
  bitonic_sort2(st->worker_shm->buf, st->worker_shm->n, st->id, st->worker_shm->thr_c);
  return NULL;
}

void bitonic(uint32_t thr_c, uint32_t n, uint32_t *buf) {
  // sync mutexs
  pthread_mutex_init(&mut, NULL);
  pthread_cond_init(&cond, NULL);

  pthread_t thr[thr_c];
  struct g_worker_st worker_shm = {n, thr_c, buf};
  struct worker_st worker_args[thr_c];

  for (int i = 0; i < thr_c; i++) {
    worker_args[i].id = i;
    worker_args[i].worker_shm = &worker_shm;
    pthread_create(&thr[i], NULL, worker, &worker_args[i]);
  }

  for (int i = 0; i < thr_c; i++) {
    pthread_join(thr[i], NULL);
  }
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
  // n = 32;
  // uint32_t buf[n];
  // for (int i = 0; i < n; i++) {
  //   buf[i] = n - i - 1;
  // }

  get_delta_time();
  bitonic(thr_count, n, buf);
  printf("Time to bitonic sort: %fs\n", get_delta_time());

  // verify correcteness
  for (int i = 0; i < n - 1; i++) {
    if (buf[i] > buf[i + 1]) {
      printf("Not Sorted\n");
      break;
    }
  }

  // printf("\n\n\n");
  // for (int i = 0; i < n; i++) {
  //   printf("%2d: %d\n", i, buf[i]);
  // }

  // frees
  fclose(fd);
  // free(buf);

  return 0;
}
