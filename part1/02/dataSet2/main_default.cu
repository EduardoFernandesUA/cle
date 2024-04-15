#include <cstdint>
#include <cstdio>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <time.h>

__global__ void dev_mergesort(uint32_t *a, uint32_t *b, uint32_t arrayLen,
                              uint32_t step) {
  uint32_t index = blockIdx.x * blockDim.x + threadIdx.x;
  index *= step;

  // printf("BAH %d %d %d %d\n", index, blockIdx.x, blockDim.x, + threadIdx.x);

  if (index >= arrayLen) {
    return;
  }

  uint32_t halfStep = step >> 1;
  uint32_t i, k, l, r;

  k = index;
  l = index;
  r = index + halfStep;

  while (l < index + halfStep && r < index + step) {
    if (a[l] < a[r]) {
      b[k] = a[l];
      l++;
    } else {
      b[k] = a[r];
      r++;
    }
    k++;
  }
  while (l < index + halfStep) {
    b[k] = a[l];
    l++;
    k++;
  }
  while (r < index + step) {
    b[k] = a[r];
    r++;
    k++;
  }
  for (i = index; i < index + step; i++) {
    a[i] = b[i];
  }
}

__global__ void dev_bitonicsort1(uint32_t *n, uint32_t size, uint32_t step,
                                 uint32_t j) {
  uint32_t i = blockIdx.x * blockDim.x + threadIdx.x;

  if (i >= size / 2) {
    return;
  }

  uint32_t j1 = j << 1;
  uint32_t imj = i % j;
  uint32_t idj = (i / j) * j1;

  uint32_t n1 = idj * j1 + imj;
  uint32_t n2 = idj * j1 + j1 - imj - 1;

  if (n[n1] > n[n2]) {
    uint32_t t = n[n1];
    n[n1] = n[n2];
    n[n2] = t;
  }
}

__global__ void dev_bitonicsort2(uint32_t *n, uint32_t size, uint32_t step,
                                 uint32_t j) {
  uint32_t i = blockIdx.x * blockDim.x + threadIdx.x;

  if (i >= size / 2)
    return;

  uint32_t j1 = j << 1;

  uint32_t n1 = (i / j) * j1 + i % j;
  uint32_t n2 = n1 + j;

  if (n[n1] > n[n2]) {
    uint32_t t = n[n1];
    n[n1] = n[n2];
    n[n2] = t;
  }
}

void copynumbers(uint32_t *a, uint32_t *b, uint32_t len) {
  for (uint32_t i = 0; i < len; i++) {
    a[i] = b[i];
  }
}

void caps(uint32_t *a, uint32_t *b) {
  uint32_t temp;
  if (*a > *b) {
    temp = *a;
    *a = *b;
    *b = temp;
  }
}

// dir is 1 for ascending and -1 for descending
void caps(uint32_t *a, uint32_t *b, int dir) {
  uint32_t temp;
  if ((dir < 0 && *a > *b) || (dir > 0 && *a < *b)) {
    temp = *a;
    *a = *b;
    *b = temp;
  }
}

void host_bitonicsort2(uint32_t *numbers, uint32_t arrayLen) {
  uint32_t N = arrayLen;

  printf("\nHOST CODE\n");
  // first pass, groups of 2
  for (int i = 0; i < arrayLen; i += 2) {
    caps(&numbers[i], &numbers[i + 1], ((i % 4) - 1));
  }
  printf("\n\n");
  // second pass, groups of 2
  for (int i = 0; i < arrayLen; i += arrayLen / 2) {
    for (int j = i; j < i + arrayLen / 4; j++) {
      caps(&numbers[j], &numbers[j + arrayLen / 4], ((i % 8) - 2));
    }
  }
  printf("\n\n");

  // third pass
  for (int i = 0; i < arrayLen; i += 2) {
    printf("%d ", (i / 4) * 2 - 1);
    caps(&numbers[i], &numbers[i + 1], (i / 4) * 2 - 1);
  }
  printf("\n\n");
}

void host_bitonicsort(uint32_t *n, uint32_t size) {
  uint i, j, k;
  uint t_count = size / 2;
  uint log2size = -1;
  uint32_t a, b;

  // calculate thread count
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
      // printf("( %d, %d)\n", a, b);
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
        // printf("_( %d, %d)\n", bah, bah + (j0 >> k));
      }
    }

    j0 <<= 1;
    j1 <<= 1;
    step += 1;
  }
}

void host_mergeSort(uint32_t *numbers, uint32_t arrayLen) {
  uint32_t i, k;
  uint32_t *ntemp = (uint32_t *)malloc(arrayLen * 4);

  // second pass
  uint32_t l, r, step, halfStep;
  for (step = 2; step <= arrayLen; step <<= 1) {
    halfStep = step >> 1;

    for (i = 0; i < arrayLen; i += step) {
      k = i;
      l = i;
      r = i + halfStep;
      while (l < i + halfStep && r < i + step) {
        if (numbers[l] < numbers[r]) {
          ntemp[k] = numbers[l];
          l++;
        } else {
          ntemp[k] = numbers[r];
          r++;
        }
        k++;
      }
      while (l < i + halfStep) {
        ntemp[k] = numbers[l];
        l++;
        k++;
      }
      while (r < i + step) {
        ntemp[k] = numbers[r];
        r++;
        k++;
      }
    }
    for (i = 0; i < arrayLen; i++) {
      numbers[i] = ntemp[i];
    }
  }

  free(ntemp);
}

void host_wikibitonicsort(uint32_t *n, uint32_t size) {
  int k, j, i, l, t;

  for (k = 2; k <= size; k *= 2) {
    for (j = k / 2; j > 0; j /= 2) {
      for (i = 0; i < size; i++) {
        l = i ^ j;
        if (l > i) {
          if (((i & j) == 0 && n[i] > n[l]) || ((i & j) != 0 && n[i] < n[l])) {
            t = n[i];
            n[i] = n[l];
            n[l] = t;
          }
        }
      }
    }
  }
}

int main(int argc, char *argv[]) {

  if (argc != 2) {
    printf("Usage: main_default.c <input_file.bin>");
    return 1;
  }

  uint i, err;
  clock_t clk;
  FILE *fd = fopen(argv[1], "rb");

  err = fseek(fd, 0, SEEK_END);
  uint64_t fileLen = ftell(fd) - 4;
  printf("File Size: %ld Bytes\n", fileLen);

  uint64_t arrayLen = fileLen / 4;
  // uint32_t arrayLen = 8;
  //  printf("Amount of numbers: %d\n", arrayLen);

  // Error bellow, the array is not generic for the size of the numbers, only 4
  // bytes now
  uint32_t *numbers = (uint32_t *)malloc(arrayLen * sizeof(uint32_t));
  uint32_t *numbers_dcopy = (uint32_t *)malloc(arrayLen * sizeof(uint32_t));

  // read numbers from the file
  i = 0;
  fseek(fd, 0, 0);
  do {
    err = fread(&numbers[i], sizeof(uint32_t), 1, fd);
    i++;
  } while (err);

  uint32_t *d_n, bytes = arrayLen * sizeof(uint32_t);
  cudaMalloc(&d_n, bytes);
  cudaMemcpy(d_n, numbers, bytes, cudaMemcpyHostToDevice);

  int blockSize = 512;
  int numBlocks = (arrayLen / 2 + blockSize - 1) / blockSize;

  clk = clock();
  uint32_t j = 1, k;
  uint32_t nsteps = -1;
  i = arrayLen;
  while (i != 0) {
    i >>= 1;
    nsteps++;
  }
  printf("Log Amount = %d\n", nsteps);
  for (uint32_t step = 0; step < nsteps; step++) {
    dev_bitonicsort1<<<numBlocks, blockSize>>>(d_n, arrayLen, step, j);
    cudaDeviceSynchronize();

    // printf("step2\n");
    for (k = 1; k <= step; k++) {
      dev_bitonicsort2<<<numBlocks, blockSize>>>(d_n, arrayLen, step, j >> k);
      cudaDeviceSynchronize();
      // printf("break\n");
    }

    j <<= 1;
    // printf("WWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWW\n");
  }
  clk = clock() - clk;
  printf("Device bitonic Sort: %fs\n", ((double)clk) / CLOCKS_PER_SEC);
  cudaMemcpy(numbers_dcopy, d_n, bytes, cudaMemcpyDeviceToHost);

  uint32_t *hostnumbers = (uint32_t *)malloc(arrayLen * sizeof(uint32_t));
  copynumbers(hostnumbers, numbers, arrayLen);
  clk = clock();
  host_mergeSort(hostnumbers, arrayLen);
  clk = clock() - clk;
  printf("Host Merge Sort: %fs\n", ((double)clk) / CLOCKS_PER_SEC);

  copynumbers(hostnumbers, numbers, arrayLen);
  clk = clock();
  host_bitonicsort(hostnumbers, arrayLen);
  clk = clock() - clk;
  printf("Host Wiki Bitonic Sort: %fs\n", ((double)clk) / CLOCKS_PER_SEC);
  for (i = 0; i < arrayLen; i++) {
    // printf("%d\n", hostnumbers[i]);
  }

  // Verify device solution agains host
  // for (i = 0; i < arrayLen; i++) {
  //   printf("%8d %8d", hostnumbers[i], numbers_dcopy[i]);
  //   if (hostnumbers[i] != numbers_dcopy[i]) {
  //     printf("  WRONG SOLUTION\n");
  //   } else {
  //     printf("\n");
  //   }
  // }

  // printf("\nSorted\n");
  // for (i = 0; i < arrayLen; i++) {
  //   printf("%u\n", numbers[i]);
  // }

  cudaFree(d_n);
  free(hostnumbers);
  free(numbers);

  return 0;
}
