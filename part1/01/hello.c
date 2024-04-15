#include <locale.h>
#include <stdio.h>
#include <wchar.h>

#define BUFFER_SIZE 64

int main(int argc, char *argv[]) {
  int i, j, k, found;
  FILE *fp;
  int wordLetters[26] = {0};
  wchar_t buffer[BUFFER_SIZE];
  int bsize;

  int letterCount = 0;
  int wordCount = 0;

  setlocale(LC_ALL, "en_US.UTF-8");

  for (i = 1; i < argc; i++) {
    printf("\n\nFILE: %s\n", argv[i]);
    fp = fopen(argv[i], "r");
    while (fgetws(buffer, BUFFER_SIZE, fp)) {
      j = 0;
      while (buffer[j] != '\0') {
        switch (buffer[j]) {
        // word separators
        case 0x20: // <space>
        case 0x9:  // <tab>
        case 0xA:  // <\n>
        case 0xD:  // <carriage treturn character>
        case '-':
        case 0x22:
        case 0xE28009C:
        case 0xe2809D:
        case '[':
        case ']':
        case '(':
        case ')':
        case '.':
        case ',':
        case ':':
        case ';':
        case '?':
        case '!':
        case 0xE28093:
        case 0x2026:
        case 0x27:
          buffer[j] = ' ';
          letterCount = 0;
          found = 0;
          for (k = 0; k < 26; k++) {
            wordLetters[k] = 0;
          }
          j++;
          continue;

        // word merger
        case 0xE28098: // '
        case 0xE28099: // '
        case '_':      // '
          buffer[j] = 'a';
          j++;
          continue;

        case 0xe7: // ç
        case 0xc7: // Ç
          buffer[j] = 'c';
          break;
        }

        if (found) {
          j++;
          continue;
        }

        if (buffer[j] >= 'A' && buffer[j] <= 'Z') {
          buffer[j] -= 'A' - 'a';
        }

        // printf("%lc %#x\n", buffer[j], buffer[j]);
        printf("%lc", buffer[j]);
        if (buffer[j] >= 'a' && buffer[j] <= 'z' && buffer[j] != 'a' &&
            buffer[j] != 'e' && buffer[j] != 'i' && buffer[j] != 'o' &&
            buffer[j] != 'u') {
          wordLetters[buffer[j] - 'a']++;

          if (wordLetters[buffer[j] - 'a'] > 1) {
            found = 1;
            wordCount++;
          }
        }

        j++;
      }
    }
  }

  printf("\nCOUNT: %d\n", wordCount);

  fclose(fp);

  return 0;
}
