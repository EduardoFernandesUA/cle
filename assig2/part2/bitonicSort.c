#include "bitonicSort.h"

#define swap(a, b) { int t = *a; *a = *b; *b = t; }

/**
 * @brief Sorts a sequence of integers using the Bitonic sort algorithm.
 *
 * @param sequence The array of integers to be sorted.
 * @param n The number of elements in the sequence.
 * @param direction The sorting direction, 1 for ascending and 0 for descending.
 */
void bitonicSort(int sequence[], int n, int direction) {
  int i, j, k;
  for (k = 2; k <= n; k = 2 * k) {
    for (j = k/2; j > 0; j = j/2) {
      for (i = 0; i < n; i++) {
        int x = i ^ j;

        if ((x) > i) {
          if ((i & k) == 0 && ((direction == 0 && sequence[i] > sequence[x]) || (direction == 1 && sequence[i] < sequence[x]))) {
            swap(&sequence[i], &sequence[x]);
          }
          if ((i & k) != 0 && ((direction == 0 && sequence[i] < sequence[x]) || (direction == 1 && sequence[i] > sequence[x]))) {
            swap(&sequence[i], &sequence[x]);
          }
        }
      }
    }
  }
}

/**
 * @brief Merges a sequence of integers using the Bitonic merge operation.
 *
 * @param sequence The array of integers to be merged.
 * @param n The number of elements in the sequence.
 * @param direction The merging direction, 1 for ascending and 0 for descending.
 */
void bitonicMerge(int sequence[], int n, int direction) {
  int i, j, k;
  for (k = n/2; k <= n; k = 2 * k) {
    for (j = k/2; j > 0; j = j/2) {
      for (i = 0; i < n; i++) {
        int x = i ^ j;

        if ((x) > i) {
          if ((i & k) == 0 && ((direction == 0 && sequence[i] > sequence[x]) || (direction == 1 && sequence[i] < sequence[x]))) {
            swap(&sequence[i], &sequence[x]);
          }
          if ((i & k) != 0 && ((direction == 0 && sequence[i] < sequence[x]) || (direction == 1 && sequence[i] > sequence[x]))) {
            swap(&sequence[i], &sequence[x]);
          }
        }
      }
    }
  }
}