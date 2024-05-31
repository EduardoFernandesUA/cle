#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main(int argc, char *argv[]) {
  if (argc != 2) {
    printf("Usage: %s <number_of_integers>\n", argv[0]);
    return 1;
  }

  int n = atoi(argv[1]);
  if (n <= 0) {
    printf("Number of integers must be a positive integer.\n");
    return 1;
  }

  // Seed the random number generator
  srand(time(NULL));

  // Calculate the size of the file in bytes
  unsigned int size = (n + 1) * sizeof(unsigned int);

  // Allocate memory for the integers
  unsigned int *data = (unsigned int *)malloc(size);
  if (data == NULL) {
    printf("Memory allocation failed.\n");
    return 1;
  }

  // First integer is the size of the file in bytes
  data[0] = n;

  // Generate n random unsigned integers
  for (int i = 1; i <= n; i++) {
    data[i] = rand();
  }

  // Open the binary file for writing
  FILE *file = fopen("dat.bin", "wb");
  if (file == NULL) {
    printf("Failed to open file for writing.\n");
    free(data);
    return 1;
  }

  // Write the data to the file
  fwrite(data, sizeof(unsigned int), n + 1, file);

  // Clean up
  fclose(file);
  free(data);

  printf("Binary file 'random_integers.bin' generated successfully.\n");
  return 0;
}
