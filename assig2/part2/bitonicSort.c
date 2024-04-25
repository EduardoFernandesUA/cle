#include "bitonicSort.h"

#define swap(a, b) { int t = *a; *a = *b; *b = t; }

void bitonicMerge(int arr[], int n, int direction) {
  int i, j, k;
  for (k = n/2; k <= n; k = 2 * k) {
    for (j = k/2; j > 0; j = j/2) {
      for (i = 0; i < n; i++) {
        int ixj = i ^ j;

        if ((ixj) > i) {
          if ((i & k) == 0 && ((direction == 0 && arr[i] > arr[ixj]) || (direction == 1 && arr[i] < arr[ixj]))) {
            swap(&arr[i], &arr[ixj]);
          }
          if ((i & k) != 0 && ((direction == 0 && arr[i] < arr[ixj]) || (direction == 1 && arr[i] > arr[ixj]))) {
            swap(&arr[i], &arr[ixj]);
          }
        }
      }
    }
  }
}

void bitonicSort(int arr[], int n, int direction) {
  int i, j, k;
  for (k = 2; k <= n; k = 2 * k) {
    for (j = k/2; j > 0; j = j/2) {
      for (i = 0; i < n; i++) {
        int ixj = i ^ j;

        if ((ixj) > i) {
          if ((i & k) == 0 && ((direction == 0 && arr[i] > arr[ixj]) || (direction == 1 && arr[i] < arr[ixj]))) {
            swap(&arr[i], &arr[ixj]);
          }
          if ((i & k) != 0 && ((direction == 0 && arr[i] < arr[ixj]) || (direction == 1 && arr[i] > arr[ixj]))) {
            swap(&arr[i], &arr[ixj]);
          }
        }
      }
    }
  }
}