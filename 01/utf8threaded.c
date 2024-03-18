#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define BUFFER_SIZE 1024
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

  do {
    int n = nextUTF8(fd, &utf);
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
  return 0;
}

void *worker(void *args) {
  struct worker_st *st = (struct worker_st *)args;

  FILE *fd = fopen(st->shm->file_str, "rb");
  fseek(fd, 0, SEEK_END);
  long file_size = ftell(fd);
  printf("FILE_SIZE: %ld\n", file_size);

  int err, local_words = 0, local_consonants = 0;
  int found_words = 0, found_consonants = 0;
  int fdt = 0, old_consonants;
  for (long i = st->id * BUFFER_SIZE; i < file_size; i += st->shm->thread_c * BUFFER_SIZE) {
    fseek(fd, i, SEEK_SET);
    fdt = ftell(fd);
    while (fdt < i + BUFFER_SIZE && fdt < file_size) {
      err = nextWord(fd, &found_words, &found_consonants);
      local_words += found_words;
      local_consonants += found_consonants;
      if (err == -1)
        break;
      fdt = ftell(fd);
    }
    if (fdt > i + BUFFER_SIZE + 1) {
      local_words--;
      if (found_consonants == 1) {
        fseek(fd, i + BUFFER_SIZE, SEEK_SET);
        local_consonants -= nextWord(fd, &found_words, &found_consonants);
        local_consonants -= found_consonants;
      }
    }
    printf("%ld\n", i);
  }

  printf("Worker %d: %d %d\n", st->id, local_words, local_consonants);

  // Update shared counts
  pthread_mutex_lock(&st->shm->mutex);
  *st->shm->words += local_words;
  *st->shm->consonants += local_consonants;
  pthread_mutex_unlock(&st->shm->mutex);

  fclose(fd);

  return NULL;
}

int main(int argc, char *argv[]) {
  int words = 0, consonants = 0;
  int err;

  // Threads variables
  int thread_c = atoi(argv[1]);
  pthread_t threads[thread_c];
  struct worker_st worker_args[thread_c];
  struct worker_shm workers_shm = {NULL, &words, &consonants, thread_c};
  pthread_mutex_init(&workers_shm.mutex, NULL);

  for (int i = 2; i < argc; i++) {
    printf("FILE: %s\n", argv[i]);

    workers_shm.file_str = argv[i];

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
  }

  printf("Total Number of words: %d\n", words);
  printf("Total number of words with at least two instances or the same "
         "consonant: %d\n",
         consonants);

  return 0;
}