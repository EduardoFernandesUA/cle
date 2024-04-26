#include "bitonicSort.h"

/**
 * @brief Swaps two integers.
 */
void swap(int *a, int *b) {
  int t = *a;
  *a = *b;
  *b = t;
}

/**
 * @brief Compares and swaps two elements in a sequence based on the sorting direction.
 */
void compareAndSwap(int sequence[], int i, int j, int direction) {
  if ((direction == 0 && sequence[i] > sequence[j]) || (direction == 1 && sequence[i] < sequence[j])) {
    swap(&sequence[i], &sequence[j]);
  }
}

/**
 * @brief Sorts a sequence of integers using the Bitonic sort algorithm.
 *
 * @param sequence The array of integers to be sorted.
 * @param n The number of elements in the sequence.
 * @param direction The sorting direction, 1 for ascending and 0 for descending.
 */
void bitonicSort(int sequence[], int n, int direction) {
  for (int k = 2; k <= n; k *= 2) {
    for (int j = k / 2; j > 0; j /= 2) {
      for (int i = 0; i < n; i++) {
        int x = i ^ j;
        if (x > i) {
          compareAndSwap(sequence, i, x, (i & k) == 0 ? direction : 1 - direction);
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
  for (int k = n / 2; k <= n; k *= 2) {
    for (int j = k / 2; j > 0; j /= 2) {
      for (int i = 0; i < n; i++) {
        int x = i ^ j;
        if (x > i) {
          compareAndSwap(sequence, i, x, (i & k) == 0 ? direction : 1 - direction);
        }
      }
    }
  }
}