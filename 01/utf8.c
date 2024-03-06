#include <stdint.h>
#include <stdio.h>

#define BUFFER_SIZE 64
#define False 0
#define True !False
#define DELIMITER_COUNT 20

union UTF8 {
  uint8_t bytes[4];
  uint32_t code;
};

unsigned int swap_endian(uint32_t value, int n) {
  if (n == 1) {
    return value;
  } else if (n == 2) {
    return ((value >> 8) & 0x00FF) | ((value << 8) & 0xFF00);
  } else if (n == 3) {
    return ((value >> 16) & 0x0000FF) | ((value << 16) & 0xFF0000) |
           (value & 0x00FF00);
  }
  // else if n==4
  return ((value >> 24) & 0x000000FF) | ((value >> 8) & 0x0000FF00) |
         ((value << 8) & 0x00FF0000) | ((value << 24) & 0xFF000000);
}

void printUTF8(union UTF8 *utf8, int n) {
  for (int i = 0; i < n; i++)
    printf("%c", utf8->bytes[i]);
}
void printlnUTF8(union UTF8 *utf8, int n) {
  printUTF8(utf8, n);
  printf("\n");
}

int nextUTF8(FILE *fd, union UTF8 *utf) {
  int n = 0;

  // Debug
  utf->code = 0;

  // read first byte
  if (!fread(&utf->bytes[0], 1, 1, fd)) {
    return 0; // Reached EOF
  }

  if ((utf->bytes[0] & 0b10000000) == 0) {
    return 1;
  } else if ((utf->bytes[0] & 0b11000000) == 0b11000000 &&
             (utf->bytes[0] & 0b00100000) == 0) {
    n = 2;
  } else if ((utf->bytes[0] & 0b00010000) == 0) {
    n = 3;
  } else if ((utf->bytes[0] & 0b00001000) == 0) {
    n = 4;
  }
  // read remaining n-1 bytes
  fread(&utf->bytes[1], n - 1, 1, fd);

  return n;
}

int main(int argc, char *argv[]) {
  int dict[26] = {0};
  uint32_t delimiters[] = {0x20,     0x9, 0xA, 0xD, '-',      0x22,    0xE2809C,
                           0xE2809D, '[', ']', '(', ')',      '.',     ',',
                           ':',      ';', '?', '!', 0xE28093, 0xE280A6};

  int i, j, k;
  int n = -1; // can not be 0 because of EOF handling

  // Counters
  int words = 0, twoConsonants = 0;

  FILE *fd;
  union UTF8 utf8;

  int hasFoundConsonants = False;
  int inWord = False;
  for (i = 1; i < argc; i++) {
    printf("FILE: %s\n", argv[i]);

    fd = fopen(argv[i], "rb");

    while (True) {
      n = nextUTF8(fd, &utf8);
      if (n == 0) { // EOF
        break;
      }

      uint8_t c = utf8.bytes[0];
      uint32_t lsbCode = swap_endian(utf8.code, n);

      switch (lsbCode) {
      // á - à - â - ã
      case 0xC3A1:
      case 0xC3A0:
      case 0xC3A2:
      case 0xC3A3:
      case 0xC381:
      case 0xC380:
      case 0xC382:
      case 0xC383:
        c = 'a';
        break;
      // é - è - ê
      case 0xC3A9:
      case 0xC3A8:
      case 0xC3AA:
      case 0xC389:
      case 0xC388:
      case 0xC38A:
        c = 'e';
        break;
      // í - ì
      case 0xC3AD:
      case 0xC3AC:
      case 0xC38D:
      case 0xC38C:
        c = 'i';
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
        c = 'o';
        break;
      // ú - ù
      case 0xC3BA:
      case 0xC3B9:
      case 0xC39A:
      case 0xC399:
        c = 'u';
        break;
      // ç - Ç
      case 0xC3A7:
      case 0xC387:
        c = 'c';
        break;
      default:
        break;
      }

      /*
      if (lsbCode == 0xC3A1 || lsbCode == 0xC3A0 || lsbCode == 0xC3A2 ||
          lsbCode == 0xC3A3 || lsbCode == 0xC381 || lsbCode == 0xC380 ||
          lsbCode == 0xC382 || lsbCode == 0xC383) {
        c = 'a';
      }
      // é - è - ê
      if (lsbCode == 0xC3A9 || lsbCode == 0xC3A8 || lsbCode == 0xC3AA ||
          lsbCode == 0xC389 || lsbCode == 0xC388 || lsbCode == 0xC38A) {
        c = 'e';
      }
      // í - ì
      if (lsbCode == 0xC3AD || lsbCode == 0xC3AC || lsbCode == 0xC38D ||
          lsbCode == 0xC38C) {
        c = 'i';
      }
      // ó - ò - ô - õ
      if (lsbCode == 0xC3B3 || lsbCode == 0xC3B2 || lsbCode == 0xC3B4 ||
          lsbCode == 0xC3B5 || lsbCode == 0xC393 || lsbCode == 0xC392 ||
          lsbCode == 0xC394 || lsbCode == 0xC395) {
        c = 'o';
      }
      // ú - ù
      if (lsbCode == 0xC3BA || lsbCode == 0xC3B9 || lsbCode == 0xC39A ||
          lsbCode == 0xC399) {
        c = 'u';
      }
      // transform ç and Ç in c
      else if (lsbCode == 0xC3A7 || lsbCode == 0xC387) {
        c = 'c';
      }
      */

      // word counter
      int type = 0; // 0 is delimiter , 1 is word character
      //
      if (c >= 'A' && c <= 'Z') {
        c = c - 'A' + 'a';
      }
      if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_') {
        inWord = True;

        if (!hasFoundConsonants && c > 'a' && c <= 'z' && c != 'e' &&
            c != 'i' && c != 'o' && c != 'u') {
          dict[c - 'a'] += 1;
          if (dict[c - 'a'] == 2) {
            twoConsonants++;
            hasFoundConsonants = True;
          }
        }

      } else if (lsbCode != 0x27 && lsbCode != 0xE28098 &&
                 lsbCode != 0xE28099) {
        if (inWord) {
          words++;
        }
        inWord = False;
        for (k = 0; k < 26; k++) {
          dict[k] = 0;
        }
        hasFoundConsonants = False;
      }
    }
  }

  printf("Total Number of words: %d\n", words);
  printf("Total number of words with at least two instances or the same "
         "consonant: %d\n",
         twoConsonants);

  return 0;
}
