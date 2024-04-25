#ifndef FILE_READER_H
#define FILE_READER_H

/**
 * @brief Reads a sequence of integers from a binary file.
 *
 * @param fileName The name of the file to read from.
 * @param sequence A pointer to an array of integers. This function will allocate memory for the array and fill it with the integers read from the file.
 * @return The number of integers read from the file.
 */
int readSequence(char *fileName, int **sequence);

#endif