/**
 *
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>

#include "common.h"
#include <cuda_runtime.h>

/* allusion to internal functions */

static void sort_cpu_kernel(unsigned int *matrix, unsigned int matrix_size);
__global__ static void sort_cuda_kernel(unsigned int *__restrict__ matrix, unsigned int matrix_size, unsigned int k, unsigned int j);
static double get_delta_time(void);

/**
 *   main program
 */

int main(int argc, char **argv) {

  printf("%s Starting...\n", argv[0]);
  if (sizeof(unsigned int) != (size_t)4)
    return 1; // it fails with prejudice if an integer does not have 4 bytes

  /* get argv of input file name */
  if (argc != 2) {
    printf("Invalid program usage!\nRun with:\n\t./bitonic <input file name>\n");
    return 2;
  }

  /* check if file exists */
  FILE *fd = fopen(argv[1], "r");
  if (fd == NULL) {
    printf("ERROR: Could not open the file (%s)\n", argv[1]);
    return 3;
  }

  /* read the first line (first line has the sample size) */
  u_int32_t n;
  if (fread(&n, sizeof(unsigned int), 1, fd) != 1) {
    printf("ERROR: reading the first row\n");
    return 4;
  }
  if ((n & (n - 1)) != 0) {
    printf("ERROR: number of numbers is not a power of two!\n");
    return 5;
  }

  /* allocate host matrix with and fill it with file numbers */
  get_delta_time();
  unsigned int *host_unsorted = (unsigned int *)malloc(n * sizeof(unsigned int));
  if (fread(host_unsorted, sizeof(unsigned int), n, fd) != n) {
    printf("ERROR: in reading the numbers from the file\n");
    return 6;
  }
  printf("Reading %d values from the file took %.3e seconds\n", n, get_delta_time());

  /* set up the device */

  int dev = 0;

  cudaDeviceProp deviceProp;
  CHECK(cudaGetDeviceProperties(&deviceProp, dev));
  printf("Using Device %d: %s\n", dev, deviceProp.name);
  CHECK(cudaSetDevice(dev));

  /* create the remaining memory areas in host and device memory where the disk sectors data and sector numbers will be stored */

  size_t matrix_size = n * sizeof(unsigned int);
  unsigned int *host_sorted, *host_device_sorted;
  unsigned int *device_matrix;

  if (matrix_size > (size_t)5e9) {
    fprintf(stderr, "The GeForce GTX 1660 Ti cannot handle more than 5GB of memory!\n");
    exit(1);
  }

  host_sorted = (unsigned int *)malloc(matrix_size);
  host_device_sorted = (unsigned int *)malloc(matrix_size);
  CHECK(cudaMalloc((void **)&device_matrix, matrix_size));

  unsigned int gridDimX, gridDimY, gridDimZ, blockDimX, blockDimY, blockDimZ;
  // for (int i = 0; i < 0; i++) {
  //  double avg = 0;
  // for (int j = 0; j < 10; j++) {

  /* copy the host data to the device memory */

  (void)get_delta_time();
  CHECK(cudaMemcpy(device_matrix, host_unsorted, matrix_size, cudaMemcpyHostToDevice));
  printf("The transfer of %ld bytes from the host to the device took %.3e seconds\n", matrix_size, get_delta_time());

  /* run the computational kernel (gpu) */

  blockDimX = 32; // optimize!
  blockDimY = 32; // optimize!
  blockDimZ = 1;  // do not change!
  gridDimX = 16;  // optimize!
  gridDimY = 32;  // optimize!
  gridDimZ = 1;   // do not change!

  /*
 blockDimX = 4; // optimize!
 blockDimY = 4;     // optimize!
 blockDimZ = 1;     // do not change!
 gridDimX = 1;      // optimize!
 gridDimY = 1;      // optimize!
 gridDimZ = 1;      // do not change!
 */

  dim3 grid(gridDimX, gridDimY, gridDimZ);
  dim3 block(blockDimX, blockDimY, blockDimZ);

  if ((gridDimX * gridDimY * gridDimZ * blockDimX * blockDimY * blockDimZ) != n / 2) {
    printf("Wrong configuration! %d != %d\n", (gridDimX * gridDimY * gridDimZ * blockDimX * blockDimY * blockDimZ), n / 2);
    return 1;
  }
  uint k, kk, j;
  (void)get_delta_time();
  for (k = 2, kk = 1; k <= n; k *= 2, kk++) { // for each iteration
    for (j = k / 2; j >= 1; j /= 2) {         // for each step
      sort_cuda_kernel<<<grid, block>>>(device_matrix, matrix_size, k, j);
      CHECK(cudaDeviceSynchronize()); // wait for kernel to finish
    }
  }
  CHECK(cudaGetLastError()); // check for kernel errors
  /*avg += get_delta_time();
}
*/
  printf("\nThe CUDA kernel <<<(%d,%d,%d), (%d,%d,%d)>>> took %.3e seconds to run\n",
         gridDimX, gridDimY, gridDimZ, blockDimX, blockDimY, blockDimZ, get_delta_time());

  /* copy kernel result back to host side */
  (void)get_delta_time();
  CHECK(cudaMemcpy(host_device_sorted, device_matrix, matrix_size, cudaMemcpyDeviceToHost));
  printf("The transfer of %ld bytes from the device to the host took %.3e seconds\n", (long)matrix_size, get_delta_time());

  /* free device global memory */

  CHECK(cudaFree(device_matrix));

  /* reset the device */

  CHECK(cudaDeviceReset());

  /* compute the modified sector data on the CPU */

  // memcpy(host_sorted, host_unsorted, n);
  (void)get_delta_time();
  sort_cpu_kernel(host_unsorted, n);
  printf("\nThe cpu kernel took %.3e seconds to run (single core)\n", get_delta_time());

  /* verify results */

  // printf("SORTED?\n");
  for (int i = 0; i < n - 1; i++) {
    // printf("%2d: %d\n", i, host_device_sorted[i]);
    if (host_device_sorted[i + 1] < host_device_sorted[i]) {
      // printf("!");
      printf("ERROR: CPU NOT SORTED\n");
      return 8;
    }
  }

  printf("All is well!\n");

  /* free host memory */

  free(host_unsorted);
  free(host_sorted);
  free(host_device_sorted);

  return 0;
}

static void sort_cpu_kernel(unsigned int *arr, unsigned int n) {
  uint i, j, k, l;
  uint kk;
  uint temp;

  for (k = 2, kk = 1; k <= n; k *= 2, kk++) { // for each iteration
    // printf("ITER %d, %d subsequences\n", kk, k);
    for (j = k / 2; j >= 1; j /= 2) { // for each step
      // printf("\tSTEP %d\n", j);
      for (i = 0; i < n; i += j << 1) { // for each block
        uint sort = ((i / k)) & 0x1;    // 0 for ascending block and 1 for descending
        // printf("\t\tblock start: %d %s\n", i, sort ? "<" : ">");
        for (l = i; l < i + j; l++) {
          // printf("\t\t\t(%d, %d)\n", l, l + j);
          if ((!sort && arr[l] > arr[l + j]) || (sort && arr[l] < arr[l + j])) {
            temp = arr[l];
            arr[l] = arr[l + j];
            arr[l + j] = temp;
          }
        }
      }
    }
  }
}

/**
 * arr -> sequence to sort
 * n -> size of the sequence
 * k -> current iteration
 * j -> current step
 **/
__global__ static void sort_cuda_kernel(unsigned int *__restrict__ arr, unsigned int n, unsigned int k, unsigned int j) {
  int row = blockIdx.y * blockDim.y + threadIdx.y;
  int col = blockIdx.x * blockDim.x + threadIdx.x;
  int idx = row * 512 + col;

  uint i, l, temp;

  // printf("\t%2d: ITER %d STEP %d\n", idx, k, j);

  // for (i = 0; i < n; i += j << 1) { // for each block
  i = idx / (j) * (j << 1);    // currespondent block
  uint sort = ((i / k)) & 0x1; // 0 for ascending block and 1 for descending
                               // printf("\t\tblock start: %d %s\n", i, sort ? "<" : ">");
  l = i + idx % j;
  //  for (l = i; l < i + j; l++) {
  // printf("\t\t%2d: %d(%d, %d)\n", idx, j, l, l + j);
  if ((!sort && arr[l] > arr[l + j]) || (sort && arr[l] < arr[l + j])) {
    temp = arr[l];
    arr[l] = arr[l + j];
    arr[l + j] = temp;
  }
  //}
  //}
  // printf("(%d %d %d)\n", row, col, idx);
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
static double get_delta_time(void) {
  static struct timespec t0, t1;

  t0 = t1;
  if (clock_gettime(CLOCK_MONOTONIC, &t1) != 0) {
    perror("clock_gettime");
    exit(1);
  }
  return (double)(t1.tv_sec - t0.tv_sec) + 1.0e-9 * (double)(t1.tv_nsec - t0.tv_nsec);
}
