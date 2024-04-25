#ifndef BITONICSORT_H
#define BITONICSORT_H

/**
 * @brief Sorts a sequence of integers using the Bitonic sort algorithm.
 *
 * @param sequence The array of integers to be sorted.
 * @param n The number of elements in the sequence.
 * @param direction The sorting direction, 1 for ascending and 0 for descending.
 */
extern void bitonicSort(int sequence[], int n, int direction);

/**
 * @brief Merges a sequence of integers using the Bitonic merge operation.
 *
 * @param sequence The array of integers to be merged.
 * @param n The number of elements in the sequence.
 * @param direction The merging direction, 1 for ascending and 0 for descending.
 */
extern void bitonicMerge(int sequence[], int n, int direction);

#endif 