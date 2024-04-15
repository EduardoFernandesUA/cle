/*
 * idea: is to use one fd per thread
 * problem: they cannot overlap and you cannot start in the middle of a word
 * solution trial 1: read the first 128 bytes + the necessary till next
 * delimiters and always align the start to the "buffer size (e.g. 128)"
 * when starting the read go to the next delimiter
 *
 * 1 12
 * 2 12
 * 3 13
 * 4 11
 * _ 48
 * */

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#define BUFFER_SIZE 128 * 128 * 128
#define False 0
#define True !False
#define DELIMITER_COUNT 20
#define DEBUG True

union UTF8 {
  uint8_t bytes[4];
  uint32_t code;
};

struct worker_shm_st {
  int worker_c;
  long file_size;
  char *file_str;
  pthread_mutex_t global_mutex;
  int global_words;
  int global_consonants;
};

struct worker_st {
  int id;
  struct worker_shm_st *shm;
};

void printfUTF8(union UTF8 *utf) {
  for (int i = 3; utf->bytes[i] != 0 && i >= 0; i--) {
    printf("%c", utf->bytes[i]);
  }
}

int readUTF8(union UTF8 *utf, uint8_t *buf, uint32_t *bufpos) {
  uint8_t c = buf[*bufpos];
  if (c == 0)
    return 0;

  uint8_t n = 1;

  if ((c & 0b11000000) == 0b11000000 && (c & 0b00100000) == 0) {
    n = 2;
  } else if ((c & 0b11100000) == 0b11100000 && (c & 0b00010000) == 0) {
    n = 3;
  } else if ((c & 0b11110000) == 0b11110000 && (c & 0b00001000) == 0) {
    n = 4;
  }

  utf->code = 0;
  for (int i = 0; i < n; i++) {
    utf->bytes[3 - i] = buf[*bufpos + i];
  }

  *bufpos += n;
  return n;
}

void removeAssentuation(union UTF8 *utf) {
  switch (utf->code >> 16) {
  // á - à - â - ã
  case 'A' << 8:
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
  case 'E' << 8:
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
  case 'I' << 8:
  case 0xC3AD:
  case 0xC3AC:
  case 0xC38D:
  case 0xC38C:
    utf->code = 0;
    utf->bytes[3] = 'i';
    break;
  // ó - ò - ô - õ
  case 'O' << 8:
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
  case 'U' << 8:
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
  uint8_t c = utf->bytes[3];
  if (c >= 'A' && c <= 'Z') {
    c -= 'A' + 'a';
  }
}

int nextUTF(union UTF8 *utf, FILE *fd) {
  utf->code = 0;
  fread(&utf->bytes[3], 1, 1, fd);
  uint8_t n = 1;
  uint8_t *c = &utf->bytes[3];

  if ((*c & 0b11000000) == 0b11000000 && (*c & 0b00100000) == 0) {
    n = 2;
  } else if ((*c & 0b11100000) == 0b11100000 && (*c & 0b00010000) == 0) {
    n = 3;
  } else if ((*c & 0b11110000) == 0b11110000 && (*c & 0b00001000) == 0) {
    n = 4;
  }

  for (int i = 1; i < n; i++) {
    fread(&utf->bytes[3 - i], 1, 1, fd);
  }

  return n;
}

// returns a bool (zero or !zero)
int isWordUTF(union UTF8 *utf) {
  uint8_t c = utf->bytes[3];
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9' && c == '_');
}
int isMergerUTF(union UTF8 *utf) {
  return utf->bytes[3] == '-' || utf->bytes[3] == 0x27 || utf->code == 0xE2809800 || utf->code == 0xE2809900;
}

void gotoNextWord(FILE *fd, long file_size) {
  union UTF8 utf;

  // skip all the non word utf
  int n;
  do {
    n = nextUTF(&utf, fd);
    removeAssentuation(&utf);
  } while (!isWordUTF(&utf) && ftell(fd) < file_size);

  fseek(fd, ftell(fd) - n, SEEK_SET);
}

int evaluateWord(FILE *fd) {
  int consonant = 0;
  int letters[26] = {0};

  union UTF8 utf;

  while (1) {
    int n = nextUTF(&utf, fd);
    removeAssentuation(&utf);
    printfUTF8(&utf);
    if (isWordUTF(&utf) || isMergerUTF(&utf)) {
      if (consonant) {
        continue;
      }
      uint8_t c = utf.bytes[3];
      if (c >= 'a' && c <= 'z') {
        letters[c - 'a'] += 1;
        if (letters[c - 'a'] > 1 && c != 'a' && c != 'e' && c != 'i' &&
            c != 'o' && c != 'u') {
          consonant = 1;
        }
      }
    } else {
      break;
    }
  }

  return consonant;
}

void *worker(void *ptr) {
  int err;
  int words = 0, consonants = 0;
  int dict[26] = {0};
  struct worker_st *args = (struct worker_st *)ptr;
  printf("Worker %d started\n", args->id);
  FILE *fd = fopen(args->shm->file_str, "rb");

  fseek(fd, 0, SEEK_END);
  long file_size = ftell(fd);

  for (int i = BUFFER_SIZE * args->id; i <= file_size; i += BUFFER_SIZE * args->shm->worker_c) {
    printf("\n\nWorker %d starting at %d\n", args->id, i);
    printf("\033[1;31m");
    err = fseek(fd, i, SEEK_SET);
    if (err != 0) {
      printf("End of file\n");
      break;
    }

    int j = 0;

    //  1. go to first non word char
    gotoNextWord(fd, file_size);
    printf("\033[0m\033[1;32m");

    //  2. count all the characters till the first not character after
    //    i+buffer_size
    while (ftell(fd) < BUFFER_SIZE + 1 && ftell(fd) < file_size - 1) {
      if (j == BUFFER_SIZE) {
        printf("\033[1;31m");
      }
      consonants += evaluateWord(fd);
      gotoNextWord(fd, file_size);
      words++;
      printf("\nBAH: %d %d\n\n", words, consonants);
    }
    printf("\033[0m");
  }

  pthread_mutex_lock(&args->shm->global_mutex);
  args->shm->global_words += words;
  args->shm->global_consonants += consonants;
  pthread_mutex_unlock(&args->shm->global_mutex);

  return 0;
}

int main(int argc, char *argv[]) {
  int err;

  if (argc < 3) {
    printf("Usage: %s <thread_count> <files.txt>", argv[0]);
    return 1;
  }

  uint thread_c = atoi(argv[1]);

  printf("Opening %s in %d threads\n", argv[2], thread_c);

  pthread_t *threads = (pthread_t *)malloc(thread_c * sizeof(pthread_t));
  struct worker_st *workers_args = (struct worker_st *)malloc(thread_c * sizeof(struct worker_st));

  struct worker_shm_st shm;
  shm.worker_c = thread_c;
  shm.file_str = argv[2];
  shm.global_words = 0;
  shm.global_consonants = 0;
  pthread_mutex_init(&shm.global_mutex, NULL);

  for (int i = 0; i < thread_c; i++) {
    printf("Creating %d worker...\n", i);
    workers_args[i].id = i;
    workers_args[i].shm = &shm;
    pthread_create(&threads[i], NULL, worker, (void *)&workers_args[i]);
  }

  for (int i = 0; i < thread_c; i++) {
    pthread_join(threads[i], NULL);
  }

  printf("All workers exited!\n");

  printf("Words: %d\n", shm.global_words);
  printf("Consonants: %d\n", shm.global_consonants);

  free(threads);

  return 0;
}
