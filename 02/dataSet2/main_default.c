#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>

int main(int argc, char *argv[]) {

  if (argc != 3) {
    printf("Usage: main_default.c <input_file.bin> <byte_size>");
    return 1;
  }

  uint64_t numberLen = atoi(argv[2]);

  uint i, j, k, err;
  FILE *fd = fopen(argv[1], "rb");

  err = fseek(fd, 0, SEEK_END);
  uint64_t fileLen = ftell(fd) - 4;
  printf("File Size: %ld Bytes\n", fileLen);

  uint64_t arrayLen = fileLen / numberLen;
  printf("Amount of numbers: %ld\n", arrayLen);

  // Error bellow, the array is not generic for the size of the numbers, only 4
  // bytes now
  uint32_t *numbers = malloc(arrayLen * numberLen);
  uint32_t *ntemp = malloc(arrayLen * numberLen);
  uint32_t *ntemp_ptr;

  i = 0;
  fseek(fd, 0, 0);
  do {
    err = fread(&numbers[i], numberLen, 1, fd);
    i++;
  } while (err);

  // printf("\nRaw\n");
  // for (i = 0; i < arrayLen; i++) {
  //   printf("%u ", numbers[i]);
  //   if ((i + 1) % 4 == 0)
  //     printf("\n");
  // }

  clock_t t = clock();

  // unroll of the first cycle
  uint32_t temp;
  for (i = 0; i < arrayLen; i += 2) {
    if (numbers[i] > numbers[i + 1]) {
      ntemp[i] = numbers[i + 1];
      ntemp[i + 1] = temp;
    } else {
      ntemp[i] = numbers[i];
      ntemp[i + 1] = numbers[i + 1];
    }
  }
  ntemp_ptr = numbers;
  numbers = ntemp;
  ntemp = ntemp_ptr;

  // second pass
  uint32_t l, r, step, halfStep;
  for (step = 4; step <= arrayLen; step <<= 1) {
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
    ntemp_ptr = numbers;
    numbers = ntemp;
    ntemp = ntemp_ptr;
  }

  t = clock() - t;
  double time_taken = ((double)t) / CLOCKS_PER_SEC;
  printf("Merge Sort: %.6fs\n", time_taken);

  // printf("\nSorted\n");
  for (i = 0; i < arrayLen; i++) {
    printf("%d ", numbers[i]);
  }

  free(numbers);
  free(ntemp);

  return 0;
}
