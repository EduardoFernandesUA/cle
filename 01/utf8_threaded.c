#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define BUFFER_SIZE 1024 * 4
#define False 0
#define True !False

union UTF8 {
  uint8_t bytes[4];
  uint32_t code;
};

struct worker_shm {
  char *file_str;
  int *words;
  int *consonants;
  int thread_c;
  pthread_mutex_t mutex;
};

struct worker_st {
  int id;
  struct worker_shm *shm;
};

// GLOBAL VARIABLES
char **files;
int files_c;

static double get_delta_time(void) {
  static struct timespec t0, t1;
  t0 = t1;
  if (clock_gettime(CLOCK_MONOTONIC, &t1) != 0) {
    perror("clock_gettime");
    exit(1);
  }
  return (double)(t1.tv_sec - t0.tv_sec) + 1.0e-9 * (double)(t1.tv_nsec - t0.tv_nsec);
}

void printUTF8(union UTF8 *utf8) {
  for (int i = 3; i >= 0 && utf8->bytes[i] != 0; i--)
    printf("%c", utf8->bytes[i]);
}
void printlnUTF8(union UTF8 *utf8) {
  printUTF8(utf8);
  printf("\n");
}

int nextUTF8(FILE *fd, union UTF8 *utf) {
  utf->code = 0;

  // read first byte
  fread(&utf->bytes[3], 1, 1, fd);
  uint8_t c = utf->bytes[3];
  if (c == 0)
    return -1;

  uint8_t n = 1;

  if ((c & 0b11000000) == 0b11000000 && (c & 0b00100000) == 0) {
    n = 2;
  } else if ((c & 0b11100000) == 0b11100000 && (c & 0b00010000) == 0) {
    n = 3;
  } else if ((c & 0b11110000) == 0b11110000 && (c & 0b00001000) == 0) {
    n = 4;
  }

  for (int i = 1; i < n; i++) {
    fread(&utf->bytes[3 - i], 1, 1, fd);
  }

  return n;
}

void removeAccentuation(union UTF8 *utf) {
  uint8_t c = utf->bytes[3];
  switch (utf->code >> 16) {
  // á - à - â - ã
  case 0xC3A1:
  case 0xC3A0:
  case 0xC3A2:
  case 0xC3A3:
  case 0xC381:
  case 0xC380:
  case 0xC382:
  case 0xC383:
    utf->code = 0;
    utf->bytes[3] = 'a';
    break;
  // é - è - ê
  case 0xC3A9:
  case 0xC3A8:
  case 0xC3AA:
  case 0xC389:
  case 0xC388:
  case 0xC38A:
    utf->code = 0;
    utf->bytes[3] = 'e';
    break;
  // í - ì
  case 0xC3AD:
  case 0xC3AC:
  case 0xC38D:
  case 0xC38C:
    utf->code = 0;
    utf->bytes[3] = 'i';
    break;
  // ó - ò - ô - õ
  case 0xC3B3:
  case 0xC3B2:
  case 0xC3B4:
  case 0xC3B5:
  case 0xC393:
  case 0xC392:
  case 0xC394:
  case 0xC395:
    utf->code = 0;
    utf->bytes[3] = 'o';
    break;
  // ú - ù
  case 0xC3BA:
  case 0xC3B9:
  case 0xC39A:
  case 0xC399:
    utf->code = 0;
    utf->bytes[3] = 'u';
    break;
  // ç - Ç
  case 0xC3A7:
  case 0xC387:
    utf->code = 0;
    utf->bytes[3] = 'c';
    break;
  default:
    break;
  }
  if (utf->bytes[3] >= 'A' && utf->bytes[3] <= 'Z') {
    utf->bytes[3] += 'a' - 'A';
  }
}

// returns a bool (zero or !zero)
int isWordLetter(union UTF8 *utf) {
  uint8_t c = utf->bytes[3];
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
}
// returns a bool (zero or !zero)
int isMergerLetter(union UTF8 *utf) {
  return utf->bytes[3] == 0x27 || utf->code == 0xE2809800 || utf->code == 0xE2809900;
}

// might read a word or not, can not be used to always get a word, only use case is this problem use case
int nextWord(FILE *fd, int *words, int *consonants) {
  int letter[26] = {0};
  union UTF8 utf;
  int foundWordLetter = 0;
  int foundDoubleConsonant = 0;
  int i = 0;
  int n;

  do {
    n = nextUTF8(fd, &utf);
    if (n == 0) { // EOF
      return -1;
    }
    removeAccentuation(&utf);
    // printUTF8(&utf);

    uint8_t c = utf.bytes[3];
    if (c >= 'a' && c <= 'z' && c != 'a' && c != 'e' && c != 'i' && c != 'o' && c != 'u') {
      if ((++letter[c - 'a']) >= 2) {
        foundDoubleConsonant = 1;
      }
    }
    i++;
    foundWordLetter = isWordLetter(&utf) || foundWordLetter;

  } while (isWordLetter(&utf) || isMergerLetter(&utf));
  (*words) = i > 1 && foundWordLetter;
  (*consonants) = foundDoubleConsonant;
  return n; // number of bytes read
}

/*
 * gives the file descriptor for the next available block
 * is protected by a mutex that provides mutual exclusion
 * uses static variables to keep track of current file and position
 * returns !0 if no next block available (the thread should when, no more work to do)
 * */
static int distributor(FILE **fd) {
  static pthread_mutex_t mut = PTHREAD_MUTEX_INITIALIZER;
  static int file_i = 0;   // between 0 and file count
  static int file_pos = 0; // multiple of BLOCK_SIZE
  static long file_s = 0;  // keeps the file size for easier access

  pthread_mutex_lock(&mut);

  if (file_i >= files_c) {
    pthread_mutex_unlock(&mut);
    return 1; // all files read
  }

  // open file descriptor for current thread
  *fd = fopen(files[file_i], "rb");
  fseek(*fd, 0, SEEK_END);
  file_s = ftell(*fd);
  fseek(*fd, file_pos, SEEK_SET);

  // printf("distributor %d\n", file_pos);

  // prepare the variables for the next thread
  if (file_pos + BUFFER_SIZE > file_s) { // reached end of file
    if (file_i + 1 > files_c) {
      pthread_mutex_unlock(&mut);
      return 1; // all files read
    }
    file_i++;
    file_pos = 0;
  } else {
    file_pos += BUFFER_SIZE;
  }

  pthread_mutex_unlock(&mut);
  return 0;
}

void *worker(void *args) {
  struct worker_st *st = (struct worker_st *)args;

  FILE *fd;
  int err, n, local_words = 0, local_consonants = 0;
  int found_words = 0, found_consonants = 0;
  int fdt = 0, fdt_start, old_consonants;

  while (True) { // each loop handles a sub sequence
    err = distributor(&fd);
    fdt_start = ftell(fd);
    fdt = ftell(fd);
    if (err) { // end of work, all done
      break;
    }

    // count all the sub sequence words and consonants
    while (fdt < fdt_start + BUFFER_SIZE) {
      n = nextWord(fd, &found_words, &found_consonants);
      local_words += found_words;
      local_consonants += found_consonants;
      fdt = ftell(fd);
      if (n == -1) // EOF
        break;
    }

    // handle duplication of intersecting word (word that is in 2 different sub-sequences)
    if (fdt > fdt_start + BUFFER_SIZE + 1) {
      local_words--;
      if (found_consonants == 1) {
        fseek(fd, fdt_start + BUFFER_SIZE, SEEK_SET);
        nextWord(fd, &found_words, &found_consonants);
        local_consonants -= found_consonants;
      }
    }
    // printf("Count from (%d, %d, %d) = %d %d \n", st->id, fdt_start, fdt, local_words, local_consonants);
  }

  // printf("Worker %d: %d %d\n", st->id, local_words, local_consonants);

  // update shared words and consonants (mutual exclusion)
  pthread_mutex_lock(&st->shm->mutex);
  *st->shm->words += local_words;
  *st->shm->consonants += local_consonants;
  pthread_mutex_unlock(&st->shm->mutex);

  return 0;
}

int main(int argc, char *argv[]) {
  if (argc < 3) {
    printf("Insuficient number of arguments!\n");
    return 1;
  }
  files_c = argc - 2;
  files = argv + 2;

  printf("Reading %d files.\n", files_c);
  for (int i = 0; i < files_c; i++) {
    printf("File %d: %s\n", i, files[i]);
  }

  int words = 0, consonants = 0;
  int err;

  // Threads variables
  int thread_c = atoi(argv[1]);
  if (thread_c < 1) {
    printf("Invalid number of threads, thread count should be >1\n");
    return 2;
  }
  pthread_t threads[thread_c];
  struct worker_st worker_args[thread_c];
  struct worker_shm workers_shm = {NULL, &words, &consonants, thread_c};
  pthread_mutex_init(&workers_shm.mutex, NULL);

  get_delta_time();
  // start threads
  for (int j = 0; j < thread_c; j++) {
    worker_args[j].id = j;
    worker_args[j].shm = &workers_shm;
    pthread_create(&threads[j], NULL, worker, &worker_args[j]);
  }

  // wait for ending of threads
  for (int j = 0; j < thread_c; j++) {
    pthread_join(threads[j], NULL);
  }

  printf("\nTotal Number of words: %d\n", words);
  printf("Total number of words with at least two instances or the same "
         "consonant: %d\n",
         consonants);
  printf("Took %f seconds to run\n", get_delta_time());

  return 0;
}
