#include "readFile.h"
#include <stdio.h>
#include <stdlib.h>

/**
 * @brief Reads a sequence of integers from a binary file.
 *
 * @param fileName The name of the file to read from.
 * @param sequence A pointer to an array of integers. This function will allocate memory for the array and fill it with the integers read from the file.
 * @return The number of integers read from the file.
 */
int readSequence(char *fileName, int **sequence) {
  FILE *file = fopen(fileName, "rb");
  if (file == NULL) {
    fprintf(stderr, "Error: file not found\n");
    exit(EXIT_FAILURE);
  }

  int size;
  if(fread(&size, sizeof(int), 1, file) != 1) {
    perror("Error reading file");
    exit(EXIT_FAILURE);
  }

  *sequence = (int *)malloc(size * sizeof(int));

  fread(*sequence, sizeof(int), size, file);
  fclose(file);

  return size;
}