#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#define MAX_FILENAME_LENGTH 256
#define MAX_THREADS 100

pthread_mutex_t print_mutex = PTHREAD_MUTEX_INITIALIZER;

// Struct to hold parameters for worker threads
typedef struct {
    int id;
    int* array;
    int num_elements; // New member to hold the number of elements in the array
    char filename[MAX_FILENAME_LENGTH]; // Added filename for each worker
} WorkerParams;

// Struct to hold parameters for distributor thread
typedef struct {
    char filename[MAX_FILENAME_LENGTH];
    int num_threads;
    int* sorted_sequence; // Array to hold the sorted sequence
    int sequence_length;  // Length of the sorted sequence
} DistributorParams;

// Bitonic merge
void bitonic_merge(int* arr, int low, int cnt, int dir) {
    if (cnt > 1) {
        int k = cnt / 2; // Divide in half the array
        for (int i = low; i < low + k; i++) {
            if ((arr[i] < arr[i + k]) == dir) { // Decreasing order
                int temp = arr[i];
                arr[i] = arr[i + k];
                arr[i + k] = temp;
            }
        }
        bitonic_merge(arr, low, k, dir);
        bitonic_merge(arr, low + k, k, dir);
    }
}

// Bitonic sort
void bitonic_sort(int* arr, int low, int cnt, int dir) {
    if (cnt > 1) {
        int k = cnt / 2;
        bitonic_sort(arr, low, k, 1);
        bitonic_sort(arr, low + k, k, 0);
        bitonic_merge(arr, low, cnt, dir);
    }
}

// Worker thread function
void* worker_function(void* args) {
    WorkerParams* params = (WorkerParams*)args;

    // Perform bitonic sort
    bitonic_sort(params->array, 0, params->num_elements, 1); // Decreasing order

    // No need to print the sorted array here

    return NULL;
}

// Distributor thread function
void* distributor_thread(void* args) {
    DistributorParams* params = (DistributorParams*)args;

    // Open file
    FILE* file = fopen(params->filename, "rb");
    if (file == NULL) {
        printf("Error: Failed to open file %s\n", params->filename);
        return NULL;
    }

    // number of integers in the file is given by the first num of the file
    int n;
    fread(&n, sizeof(int), 1, file);

    int num_integers = n;
    int integers_per_thread = num_integers / params->num_threads;
    int remaining_integers = num_integers % params->num_threads;
    int start_position = 0;

    // Allocate memory for the sorted sequence
    params->sorted_sequence = (int*)malloc(num_integers * sizeof(int));
    if (params->sorted_sequence == NULL) {
        printf("Error: Memory allocation failed for sorted sequence\n");
        fclose(file);
        return NULL;
    }

    // Distribute sub-sequences of the sequence of integers to worker threads
    for (int i = 0; i < params->num_threads; i++) {
        WorkerParams* worker_params = (WorkerParams*)malloc(sizeof(WorkerParams)); // Allocate memory for each worker_params
        if (worker_params == NULL) {
            printf("Error: Memory allocation failed for worker %d\n", i);
            fclose(file);
            free(params->sorted_sequence);
            return NULL;
        }
        worker_params->id = i;
        worker_params->array = NULL;
        snprintf(worker_params->filename, MAX_FILENAME_LENGTH, "%s", params->filename);
        
        // Calculate the number of integers for the current thread
        int thread_integers = integers_per_thread + (i < remaining_integers ? 1 : 0);
        
        // Seek to the appropriate position in the file for the current thread
        fseek(file, sizeof(int) + start_position * sizeof(int), SEEK_SET);
        
        // Read integers for the current thread
        worker_params->array = (int*)malloc(thread_integers * sizeof(int));
        if (worker_params->array == NULL) {
            printf("Error: Memory allocation failed for worker %d\n", i);
            fclose(file);
            free(worker_params);
            free(params->sorted_sequence);
            return NULL;
        }
        
        size_t items_read = fread(worker_params->array, sizeof(int), thread_integers, file);
        if (items_read != thread_integers) {
            printf("Error: Failed to read integers for worker %d from file %s\n", i, params->filename);
            free(worker_params->array);
            fclose(file);
            free(worker_params);
            free(params->sorted_sequence);
            return NULL;
        }

        worker_params->num_elements = thread_integers; // Assign the number of elements to worker_params

        // Update start position for the next thread
        start_position += thread_integers;

        // Perform bitonic sort in each worker thread
        pthread_t worker_thread;
        pthread_create(&worker_thread, NULL, worker_function, (void*)worker_params);
        pthread_join(worker_thread, NULL); // Wait for the worker thread to finish

        // Copy the sorted segment into the sorted sequence
        pthread_mutex_lock(&print_mutex);
        for (int j = 0; j < thread_integers; j++) {
            params->sorted_sequence[params->sequence_length++] = worker_params->array[j];
        }
        pthread_mutex_unlock(&print_mutex);

        // Free dynamically allocated memory
        free(worker_params->array);
        free(worker_params);
    }

    // Close file
    fclose(file);

    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Usage: %s <thread_count> <filename>\n", argv[0]);
        return 1;
    }

    int num_threads = atoi(argv[1]);
    if (num_threads <= 0 || num_threads > MAX_THREADS) {
        printf("Invalid thread count.\n");
        return 1;
    }

    // Store filename in shared region
    char filename[MAX_FILENAME_LENGTH];
    snprintf(filename, MAX_FILENAME_LENGTH, "%s", argv[2]);

    pthread_t threads[num_threads];

    // Create distributor thread
    DistributorParams distributor_params;
    snprintf(distributor_params.filename, MAX_FILENAME_LENGTH, "%s", filename);
    distributor_params.num_threads = num_threads;
    distributor_params.sorted_sequence = NULL;
    distributor_params.sequence_length = 0;
    pthread_create(&threads[0], NULL, distributor_thread, (void*)&distributor_params);

    // Wait for distributor thread to finish
    pthread_join(threads[0], NULL);

    // Perform final bitonic merge on the sorted sequence
    bitonic_merge(distributor_params.sorted_sequence, 0, distributor_params.sequence_length, 1);

    // Print the sorted sequence
    printf("Sorted sequence: \n");
    for (int i = 0; i < distributor_params.sequence_length; i++) {
        printf("%d ", distributor_params.sorted_sequence[i]);
    }
    printf("\n");

    // Free memory allocated for the sorted sequence
    free(distributor_params.sorted_sequence);

    return 0;
}
