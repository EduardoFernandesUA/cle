#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h> 

#define MAX_FILENAME_LENGTH 256
#define MAX_THREADS 100

pthread_mutex_t print_mutex = PTHREAD_MUTEX_INITIALIZER;

// worker threads
typedef struct {
    int id;
    int* array;
    int num_elements;
    char filename[MAX_FILENAME_LENGTH];
} WorkerParams;

// distributor thread
typedef struct {
    char filename[MAX_FILENAME_LENGTH];
    int num_threads;
    int* sorted_sequence; 
    int sequence_length;  
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

    printf("Thread %d: Sorting %d elements\n", params->id, params->num_elements);

    bitonic_sort(params->array, 0, params->num_elements, 1); // Decreasing order

    printf("Thread %d: Sorting completed\n", params->id);

    return NULL;
}


// Distributor thread function
void* distributor_thread(void* args) {
    DistributorParams* params = (DistributorParams*)args;

    FILE* file = fopen(params->filename, "rb");
    if (file == NULL) {
        printf("Error: Failed to open file %s\n", params->filename);
        return NULL;
    }

    // number of integers of the file is given by the first num of the file
    int n;
    fread(&n, sizeof(int), 1, file);

    int num_integers = n;
    int integers_per_thread = num_integers / params->num_threads; 
    int remaining_integers = num_integers % params->num_threads;
    int start_position = 0;

    // Allocate memory
    params->sorted_sequence = (int*)malloc(num_integers * sizeof(int));
    if (params->sorted_sequence == NULL) {
        printf("Error: Memory allocation failed for sorted sequence\n");
        fclose(file);
        return NULL;
    }

    pthread_t threads[params->num_threads]; // Create an array to hold thread IDs

    WorkerParams* worker_params = malloc(params->num_threads * sizeof(WorkerParams));
    if (worker_params == NULL) {
        printf("Error: Memory allocation failed for worker_params\n");
        fclose(file);
        free(params->sorted_sequence);
        return NULL;
    }

    for (int i = 0; i < params->num_threads; i++) {
        worker_params[i].id = i;
        worker_params[i].array = NULL;
        snprintf(worker_params[i].filename, MAX_FILENAME_LENGTH, "%s", params->filename);
        
        int thread_integers = integers_per_thread + (i < remaining_integers ? 1 : 0);
        
        fseek(file, sizeof(int) + start_position * sizeof(int), SEEK_SET);
        
        // Read integers 
        worker_params[i].array = (int*)malloc(thread_integers * sizeof(int));
        if (worker_params[i].array == NULL) {
            printf("Error: Memory allocation failed for worker %d\n", i);
            fclose(file);
            free(worker_params);
            free(params->sorted_sequence);
            return NULL;
        }
        
        size_t integers = fread(worker_params[i].array, sizeof(int), thread_integers, file);
        if (integers != thread_integers) {
            printf("Error: Failed to read integers for worker %d from file %s\n", i, params->filename);
            free(worker_params[i].array);
            fclose(file);
            free(worker_params);
            free(params->sorted_sequence);
            return NULL;
        }

        worker_params[i].num_elements = thread_integers; 

        start_position += thread_integers;

        // creation of multiple worker threads
        pthread_create(&threads[i], NULL, worker_function, (void*)&worker_params[i]);
    }

    // Wait for all worker threads to finish
    for (int i = 0; i < params->num_threads; i++) {
        pthread_join(threads[i], NULL); 
    }

    for (int i = 0; i < params->num_threads; i++) {
        for (int j = 0; j < worker_params[i].num_elements; j++) {
            params->sorted_sequence[params->sequence_length++] = worker_params[i].array[j];
        }
    }

    fclose(file);

    // Perform final bitonic merge on the sorted sequence from the given sequences of each worker
    bitonic_sort(params->sorted_sequence, 0, params->sequence_length, 1);

    free(worker_params);

    return NULL;
}

/**
 *  \brief Get the process time that has elapsed since last call of this time.
 *
 *  \return process elapsed time
 */
static double get_delta_time(void) {
    static struct timespec t0, t1;

    t0 = t1;
    if (clock_gettime(CLOCK_MONOTONIC, &t1) != 0) {
        perror("clock_gettime");
        exit(1);
    }
    return (double)(t1.tv_sec - t0.tv_sec) +
           1.0e-9 * (double)(t1.tv_nsec - t0.tv_nsec);
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

    // Store in shared region
    char filename[MAX_FILENAME_LENGTH];
    snprintf(filename, MAX_FILENAME_LENGTH, "%s", argv[2]);

    // Creation of distributor thread
    DistributorParams distributor_params;
    snprintf(distributor_params.filename, MAX_FILENAME_LENGTH, "%s", filename);
    distributor_params.num_threads = num_threads;
    distributor_params.sorted_sequence = NULL;
    distributor_params.sequence_length = 0;

    double start_time = get_delta_time(); // start time

    pthread_t distributor_thread_id;
    pthread_create(&distributor_thread_id, NULL, distributor_thread, (void*)&distributor_params);

    pthread_join(distributor_thread_id, NULL);

    double end_time = get_delta_time(); // end time

    //printf("Sorted sequence: \n");
    // for (int i = 0; i < distributor_params.sequence_length; i++) {
    //     printf("%d ", distributor_params.sorted_sequence[i]);
    // }
    
    
    printf("\n");

    printf("Elapsed time: %.9f seconds\n", end_time - start_time);

    free(distributor_params.sorted_sequence);

    return 0;
}
