#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define SIZE 10 // Size of the random sequence

int main(int argc, char **argv) {
  int size = SIZE;
  if (argc == 2) {
    size = atoi(argv[1]);
  }
  int numbers[size];
  int i, min, max;

  // Seed the random number generator
  srand(123456);

  // Generate random sequence
  printf("Random sequence: ");
  for (i = 0; i < size; i++) {
    numbers[i] = rand(); // Generating numbers between 0 and 99
    printf("%d ", numbers[i]);
  }
  printf("\n");

  // Initialize min and max with the first element
  min = max = numbers[0];

  // Find min and max values
  for (i = 1; i < size; i++) {
    if (numbers[i] < min) {
      min = numbers[i];
    }
    if (numbers[i] > max) {
      max = numbers[i];
    }
  }

  // Print min and max values
  printf("Minimum value: %d\n", min);
  printf("Maximum value: %d\n", max);

  return 0;
}
