// BS -> BITONIC SORTING
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// bitonic merge
void bitonic_merge(int* arr, int low, int cnt, int dir) {
    if (cnt > 1) {
        int k = cnt / 2; // divide in half the array
        for (int i = low; i < low + k; i++) {
            if ((arr[i] < arr[i + k]) == dir) { // decreasing order
                int temp = arr[i];
                arr[i] = arr[i + k];
                arr[i + k] = temp;
            }
        }
        bitonic_merge(arr, low, k, dir);
        bitonic_merge(arr, low + k, k, dir);
    }
}

// bitonic sort
void bitonic_sort(int* arr, int low, int cnt, int dir) {
    if (cnt > 1) {
        int k = cnt / 2;
        bitonic_sort(arr, low, k, 1);
        bitonic_sort(arr, low + k, k, 0);
        bitonic_merge(arr, low, cnt, dir);
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Please provide a file name as a command line argument.\n");
        return 1;
    }

    FILE *file = fopen(argv[1], "rb");
    if (file == NULL) {
        printf("Unable to open file.\n");
        return 1;
    }

    // n of integers in the file is given by the first num of the file
    int n;
    fread(&n, sizeof(int), 1, file); 
    
    int* arr = (int*) malloc(sizeof(int)*n);

    fread(arr, sizeof(int), n, file); 

    fclose(file);

    // printf("Original array: \n");
    // for (int i = 0; i < n; i++) {
    //     printf("%d ", arr[i]);
    // }

    clock_t start, end; 
    double cpu_time_used;

    start = clock(); 
    bitonic_sort(arr, 0, n, 1);
    end = clock(); 

    cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;

    printf("Time: %f", cpu_time_used);
    
    printf("\n\nSorted array: \n");
    for (int i = 0; i < n; i++) {
       printf("%d ", arr[i]);
    }

    printf("\n");
    return 0;
}
