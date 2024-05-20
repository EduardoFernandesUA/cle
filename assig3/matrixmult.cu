/**
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "common.h"
#include <cuda_runtime.h>

/**
 *   program configuration
 */

#ifndef SQUARE_MATRIX_SIZE
#define SQUARE_MATRIX_SIZE 1024
#endif

/* allusion to internal functions */

static void matrix_mult_cpu_kernel(unsigned int *matrix_A, unsigned int *matrix_B,
                                   unsigned int *matrix_C, unsigned int matrix_size);
__global__ static void matrix_mult_cuda_kernel(unsigned int *__restrict__ matrix_A, unsigned int *__restrict__ matrix_B,
                                               unsigned int *__restrict__ matrix_C, unsigned int matrix_size);
static double get_delta_time(void);

/**
 *   main program
 */

int main(int argc, char **argv) {

  printf("%s Starting...\n", argv[0]);
  if (sizeof(unsigned int) != (size_t)4)
    return 1; // it fails with prejudice if an integer does not have 4 bytes

  /* set up the device */

  int dev = 0;

  cudaDeviceProp deviceProp;
  CHECK(cudaGetDeviceProperties(&deviceProp, dev));
  printf("Using Device %d: %s\n", dev, deviceProp.name);
  CHECK(cudaSetDevice(dev));

  /* create memory areas in host and device memory where the disk sectors data and sector numbers will be stored */

  size_t matrix_size = SQUARE_MATRIX_SIZE * SQUARE_MATRIX_SIZE * sizeof(unsigned int);
  unsigned int *host_matrix_A, *host_matrix_B, *host_matrix_C, *host_device_matrix_C;
  unsigned int *device_matrix_A, *device_matrix_B, *device_matrix_C;

  if (matrix_size * 2 > (size_t)5e9) {
    fprintf(stderr, "The GeForce GTX 1660 Ti cannot handle more than 5GB of memory!\n");
    exit(1);
  }

  host_matrix_A = (unsigned int *)malloc(matrix_size);
  host_matrix_B = (unsigned int *)malloc(matrix_size);
  host_matrix_C = (unsigned int *)malloc(matrix_size);
  host_device_matrix_C = (unsigned int *)malloc(matrix_size);
  CHECK(cudaMalloc((void **)&device_matrix_A, matrix_size));
  CHECK(cudaMalloc((void **)&device_matrix_B, matrix_size));
  CHECK(cudaMalloc((void **)&device_matrix_C, matrix_size));

  /* initialize the host data */

  int i;

  printf("Start attribution of random data\n");
  (void)get_delta_time();
  srand(0xCCE2021);
  for (i = 0; i < SQUARE_MATRIX_SIZE * SQUARE_MATRIX_SIZE; i++) {
    host_matrix_A[i] = rand() & 0xFF;
    host_matrix_B[i] = rand() & 0xFF;
  }
  printf("The initialization of host data took %.3e seconds\n", get_delta_time());

  unsigned int gridDimX, gridDimY, gridDimZ, blockDimX, blockDimY, blockDimZ;
  for (int i = 0; i < 6; i++) {
    double avg = 0;
    for (int j = 0; j < 10; j++) {

      /* copy the host data to the device memory */

      (void)get_delta_time();
      CHECK(cudaMemcpy(device_matrix_A, host_matrix_A, matrix_size, cudaMemcpyHostToDevice));
      CHECK(cudaMemcpy(device_matrix_B, host_matrix_B, matrix_size, cudaMemcpyHostToDevice));
      /*printf("The transfer of %ld bytes from the host to the device took %.3e seconds\n",
             (long)sector_data_size + (long)sector_number_size, get_delta_time());*/

      /* run the computational kernel
         as an example, N_SECTORS threads are launched where each thread deals with one sector */

      blockDimX = 1 << i;       // optimize!
      blockDimY = 1 << i;       // optimize!
      blockDimZ = 1 << 0;       // do not change!
      gridDimX = 1 << (10 - i); // optimize!
      gridDimY = 1 << (10 - i); // optimize!
      gridDimZ = 1 << 0;        // do not change!

      dim3 grid(gridDimX, gridDimY, gridDimZ);
      dim3 block(blockDimX, blockDimY, blockDimZ);

      if ((gridDimX * gridDimY * gridDimZ * blockDimX * blockDimY * blockDimZ) != SQUARE_MATRIX_SIZE * SQUARE_MATRIX_SIZE) {
        printf("Wrong configuration! %d != %d\n",
               (gridDimX * gridDimY * gridDimZ * blockDimX * blockDimY * blockDimZ), SQUARE_MATRIX_SIZE * SQUARE_MATRIX_SIZE);
        return 1;
      }
      (void)get_delta_time();
      matrix_mult_cuda_kernel<<<grid, block>>>(device_matrix_A, device_matrix_B, device_matrix_C, matrix_size);
      CHECK(cudaDeviceSynchronize()); // wait for kernel to finish
      CHECK(cudaGetLastError());      // check for kernel errors
      avg += get_delta_time();
    }
    printf("The CUDA kernel <<<(%d,%d,%d), (%d,%d,%d)>>> took %.3e seconds to run\n",
           gridDimX, gridDimY, gridDimZ, blockDimX, blockDimY, blockDimZ, avg / 10);
  }

  /* copy kernel result back to host side */
  (void)get_delta_time();
  CHECK(cudaMemcpy(host_device_matrix_C, device_matrix_C, matrix_size, cudaMemcpyDeviceToHost));
  printf("The transfer of %ld bytes from the device to the host took %.3e seconds\n", (long)matrix_size, get_delta_time());

  /* free device global memory */

  CHECK(cudaFree(device_matrix_A));
  CHECK(cudaFree(device_matrix_B));
  CHECK(cudaFree(device_matrix_C));

  /* reset the device */

  CHECK(cudaDeviceReset());

  /* compute the modified sector data on the CPU */

  (void)get_delta_time();
  matrix_mult_cpu_kernel(host_matrix_A, host_matrix_B, host_matrix_C, matrix_size);
  printf("The cpu kernel took %.3e seconds to run (single core)\n", get_delta_time());

  /* compare results */

  for (i = 0; i < SQUARE_MATRIX_SIZE * SQUARE_MATRIX_SIZE; i++)
    if (host_device_matrix_C[i] != host_matrix_C[i]) {
      printf("Mismatch in elem %d\n", i);
      exit(1);
    }
  printf("All is well!\n");

  /* free host memory */

  free(host_matrix_A);
  free(host_matrix_B);
  free(host_matrix_C);

  return 0;
}

static void matrix_mult_cpu_kernel(unsigned int *matrix_A, unsigned int *matrix_B,
                                   unsigned int *matrix_C, unsigned int sector_size) {
  long sum = 0, a, b;
  for (int cy = 0; cy < SQUARE_MATRIX_SIZE; cy++) {
    for (int cx = 0; cx < SQUARE_MATRIX_SIZE; cx++) {
      sum = 0;
      for (int i = 0; i < SQUARE_MATRIX_SIZE; i++) {
        a = matrix_A[cy * SQUARE_MATRIX_SIZE + i];
        b = matrix_B[i * SQUARE_MATRIX_SIZE + cx];
        sum += a * b;
      }
      matrix_C[cy * SQUARE_MATRIX_SIZE + cx] = sum;
    }
  }
}

__global__ static void matrix_mult_cuda_kernel(unsigned int *__restrict__ matrix_A, unsigned int *__restrict__ matrix_B,
                                               unsigned int *__restrict__ matrix_C, unsigned int matrix_size) {
  int row = blockIdx.y * blockDim.y + threadIdx.y;
  int col = blockIdx.x * blockDim.x + threadIdx.x;

  int sum = 0;
  for (int k = 0; k < SQUARE_MATRIX_SIZE; k++) {
    sum += matrix_A[row * SQUARE_MATRIX_SIZE + k] * matrix_B[k * SQUARE_MATRIX_SIZE + col];
  }
  matrix_C[row * SQUARE_MATRIX_SIZE + col] = sum;
}

static double get_delta_time(void) {
  static struct timespec t0, t1;

  t0 = t1;
  if (clock_gettime(CLOCK_MONOTONIC, &t1) != 0) {
    perror("clock_gettime");
    exit(1);
  }
  return (double)(t1.tv_sec - t0.tv_sec) + 1.0e-9 * (double)(t1.tv_nsec - t0.tv_nsec);
}

/*
 * CODE USED TO TEST THE CPU MATRIX MULTIPLICATION PRIMITIVE
 *
  unsigned int *a, *b, *c;
  a = (unsigned int *)malloc(9 * sizeof(unsigned int));
  b = (unsigned int *)malloc(9 * sizeof(unsigned int));
  c = (unsigned int *)malloc(9 * sizeof(unsigned int));
  for (int i = 0; i < 9; i++)
    a[i] = i + 1;
  for (int i = 0; i < 9; i++)
    b[i] = 9 - i;

  printf("A:\n");
  for (int i = 0; i < 9; i++) {
    printf("%d ", a[i]);
    if ((i + 1) % 3 == 0)
      printf("\n");
  }
  printf("\nB:\n");
  for (int i = 0; i < 9; i++) {
    printf("%d ", b[i]);
    if ((i + 1) % 3 == 0)
      printf("\n");
  }
  matrix_mult_cpu_kernel(a, b, c, 9 * sizeof(unsigned int));
  printf("\nC:\n");
  for (int i = 0; i < 9; i++) {
    printf("%d ", c[i]);
    if ((i + 1) % 3 == 0)
      printf("\n");
  }

  exit(0);
  */
