#include "fileReader.h"
#include <stdio.h>
#include <stdlib.h>

int readSequence(char *fileName, int **sequence) {

    FILE *file = fopen(fileName, "rb");
    if (file == NULL) {
        fprintf(stderr, "Error: file not found\n");
        exit(EXIT_FAILURE);
    }

    // Read from the file the first int
    int size;
    fread(&size, sizeof(int), 1, file);

    *sequence = (int *)malloc(size * sizeof(int));
    if (*sequence == NULL) {
        perror("Error allocating memory");
        exit(EXIT_FAILURE);
    }

    // Read from the file the sequenceay of n ints
    fread(*sequence, sizeof(int), size, file);
    fclose(file);

    return size;
}