#include "readFile.h"
#include <stdio.h>
#include <stdlib.h>

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