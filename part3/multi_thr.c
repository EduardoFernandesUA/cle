#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#define SIZE 100      // Size of the random sequence
#define NUM_THREADS 5 // Number of threads to use

int numbers[SIZE];
int min, max;

struct ThreadArgs {
  int start_index;
  int end_index;
};

void *find_min_max(void *arg) {
  struct ThreadArgs *args = (struct ThreadArgs *)arg;
  int i;

  for (i = args->start_index; i < args->end_index; i++) {
    if (numbers[i] < min) {
      min = numbers[i];
    }
    if (numbers[i] > max) {
      max = numbers[i];
    }
  }

  pthread_exit(NULL);
}

int main(int argc, char **argv) {
  int size = SIZE, num_threads = NUM_THREADS;
  if (argc == 3) {
    size = atoi(argv[1]);
    num_threads = atoi(argv[2]);
  }
  pthread_t threads[num_threads];
  struct ThreadArgs thread_args[num_threads];
  int i;

  // Seed the random number generator and generate random sequence
  srand(123456);
  printf("Random sequence: ");
  for (i = 0; i < size; i++) {
    numbers[i] = rand(); // Generating numbers between 0 and 99
    printf("%d ", numbers[i]);
  }
  printf("\n");

  // Initialize min and max with the first element
  min = max = numbers[0];

  // Create threads to find min and max values in different parts of the sequence
  for (i = 0; i < num_threads; i++) {
    thread_args[i].start_index = i * (size / num_threads);
    thread_args[i].end_index = (i + 1) * (size / num_threads);
    pthread_create(&threads[i], NULL, find_min_max, &thread_args[i]);
  }

  // Join threads
  for (i = 0; i < num_threads; i++) {
    pthread_join(threads[i], NULL);
  }

  // Print min and max values
  printf("Minimum value: %d\n", min);
  printf("Maximum value: %d\n", max);

  return 0;
}
