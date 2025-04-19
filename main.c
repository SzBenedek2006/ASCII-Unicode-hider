#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <wchar.h>
#include <utf8proc.h>


typedef char* unicode_t;
/* Don't forget to free() */
#define unicode_t_init() calloc(5, sizeof(char))

typedef enum { // Starts from 0
    UNDETECTED, // Likely ASCII or UTF-8
    ASCII,
    UTF_8,
    UTF_16_LE,
    UTF_16_BE,
    UTF_32_LE,
    UTF_32_BE,
} EncodingType;



// This is 99% wibe coded
char* normalize_string(char *input) {
    utf8proc_uint8_t *decomposed = utf8proc_NFKD((const utf8proc_uint8_t *)input);
    if (!decomposed) return NULL;

    size_t len = strlen((char *)decomposed);
    char *output = malloc(2 * len + 1);  // room for added symbols
    if (!output) return NULL;

    size_t out_i = 0;
    for (utf8proc_ssize_t i = 0; i < len;) {
        utf8proc_int32_t codepoint;
        utf8proc_ssize_t char_len = utf8proc_iterate(decomposed + i, -1, &codepoint);
        if (char_len < 1) break;

        if (codepoint <= 0x7F) {
            // ASCII base letter
            output[out_i++] = (char)codepoint;
        } else {
            // Known combining marks (https://www.fileformat.info/info/unicode/block/combining_diacritical_marks/list.htm)
            switch (codepoint) {
                case 0x0301: output[out_i++] = '\''; break; // acute (áéíóúű)
                case 0x0308: output[out_i++] = '~'; break; // diaeresis (öü)
                case 0x030B: output[out_i++] = '\"'; break; // double acute (őű)
                case 0x0300: output[out_i++] = '`';  break; // grave (if ever needed)
                // add more if needed
                default: break; // ignore unknown marks
            }
        }

        i += char_len;
    }

    output[out_i] = '\0';
    free(decomposed);
    return output;
}


// This function was half vibe-coded
void unicode_block(char ascii, char* unicode) {
    unsigned char c = ascii;
    if (c < 0x20 || c > 0x7E) {
        c = ' ';
    }


    unsigned int pua_code = c + 0xE0000;
    unicode[0] = 0xF3;
    unicode[1] = 0xA0;
    unicode[2] = 0x80 + ((pua_code >> 6) & 0x3F);
    unicode[3] = 0x80 + (pua_code & 0x3F);
    unicode[4] = '\0';
}


/*
* Returns 0 (false) if not utf (is ASCII), otherwise returns the position
* of the first non-ascii character
* If error, return -1 (if ever needed)
*/
int isUTF(char* buf) {
    int i = 1;
    while (buf[i] != '\0') {
        if ((unsigned char)buf[i] > 127) {
            return i;
        }
        i++;
    }
    return 0;
}


EncodingType check_encoding(FILE* file, char* buf) {

    unsigned char bom[4] = {0};
    unsigned long bytes_read = fread(bom, 1, 4, file);
    rewind(file);
    bool fTooSmall = false;
    // EncodingType enc = UNDETECTED; // This should be good default value for now

    if (bytes_read < 2) {
        printf("\tFile too small to determine encoding or encoding is missing\n%ld\n", bytes_read);
        fTooSmall = true;
    }

    // Detect encoding based on BOM
    if (!fTooSmall) {
        if (bom[0] == 0xEF && bom[1] == 0xBB && bom[2] == 0xBF) {
            printf("\tDetected UTF-8 encoding\n");
            return UTF_8;
        } else if (bom[0] == 0xFF && bom[1] == 0xFE) {
            printf("\tDetected UTF-16 LE encoding\n");
            return UTF_16_LE;
        } else if (bom[0] == 0xFE && bom[1] == 0xFF) {
            printf("\tDetected UTF-16 BE encoding\n");
            return UTF_16_BE;
        } else if (bom[0] == 0xFF && bom[1] == 0xFE && bom[2] == 0x00 && bom[3] == 0x00) {
            printf("\tDetected UTF-32 LE encoding\n");
            return UTF_32_LE;
        } else if (bom[0] == 0x00 && bom[1] == 0x00 && bom[2] == 0xFE && bom[3] == 0xFF) {
            printf("\tDetected UTF-32 BE encoding\n");
            return UTF_32_BE;

        } else {
            printf("\tNo BOM detected, assuming UTF-8 or ASCII\n");

            if (isUTF(buf)) {
                printf("\tFirst non ASCII char 1 indexed location: %d\n", isUTF(buf));
                return UTF_8;
            }
            printf("\tASCII\n");
            return ASCII; // (default value)

        }
    } else {
        return UNDETECTED;
    }

}


int main() {
    // Check BOM
    FILE* inputFile = fopen("input.txt", "rb");
    if (!inputFile) {
        printf("\tInput file not fount or can't be opened!\n");
        return 1;
    }

    FILE* outputFile = fopen("output.txt", "w");

    if (!inputFile) { printf("\tInput file error\n"); return 1; }
    if (!outputFile) { printf("\tOutput file error\n"); return 1; }

    struct stat st; // st.st_size: file size
    if (stat("input.txt", &st) != 0) {
        printf("\tstat() error\n");
        return 1;
    }
    char* buf = calloc(st.st_size + 1, sizeof(char));

    size_t bytesRead = fread(buf, 1, st.st_size, inputFile);
    rewind(inputFile);

    if (st.st_size != bytesRead) {
        printf("\tRead bytes and file size !=\n");
    }

    if (check_encoding(inputFile, buf) != ASCII) {
        buf = normalize_string(buf);                 // Before this point, buf isn't ASCII normalized.
    }





    //fputs(buf, outputFile);
    for (int i = 0; i < st.st_size; i++) {
        unicode_t b = unicode_t_init();
        unicode_block(buf[i], b);
        fputs(b, outputFile);
        free(b);
    }







    fclose(inputFile);
    fclose(outputFile);

    return 0;
}
