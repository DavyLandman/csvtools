#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <math.h>
#include "pcg_basic.h"
#include "generate.h"


#define MAX(a,b) (((a) > (b)) ? (a) : (b))

static double random_float(pcg32_random_t* rng) {
    return ldexp(pcg32_random_r(rng), -32);
}

static char random_alpha(pcg32_random_t* rng) {
    if (pcg32_random_r(rng) > (UINT32_MAX / 2)) {
        return 'A' + pcg32_boundedrand_r(rng, 'Z' - 'A');
    }
    return 'a' + pcg32_boundedrand_r(rng, 'z' - 'a');
}
static char random_alpha_numeric(pcg32_random_t* rng) {
    if (pcg32_random_r(rng) > (UINT32_MAX / 2)) {
        return '0' + pcg32_boundedrand_r(rng, '9' - '0');
    }
    return random_alpha(rng); 
}


static size_t random_cell(pcg32_random_t* rng, char* restrict target, const unsigned int columns, const size_t cell_size_max) {
    size_t written = 0;
    for (unsigned int i = 0; i < columns; i++) {
        if (i > 0) {
            *target++ =',';
            written++;
        }
        size_t cell_size = pcg32_boundedrand_r(rng, random_float(rng) < 0.2 ? cell_size_max : MAX(1, cell_size_max / 40));
        if (cell_size < 2) {
            cell_size = 2;
        }
        if (random_float(rng) > 0.1) {
            for (size_t c = 0; c < cell_size; c++) {
                *target++ = random_alpha_numeric(rng);
            }
        }
        else {
            *target++ = '"';
            written++;
            for (size_t c = 0; c < cell_size - 2; c++) {
                *target++ = random_alpha(rng);
                if (random_float(rng) > 0.6) {
                    *target++ = ' ';
                    cell_size--;
                    written++;
                }
                else if (random_float(rng) > 0.7) {
                    *target++ = ',';
                    cell_size--;
                    written++;
                }
                else if (random_float(rng) < 0.01) {
                    *target++ ='"';
                    *target++ ='"';
                    written += 2;
                    cell_size-=2;
                }
                else if (random_float(rng) < 0.001) {
                    *target++ ='\n';
                    cell_size--;
                    written++;
                }
            }
            *target++ = '"';
            written++;
        }
        written += cell_size;
    }
    *target++ = '\n';
    written++;
    return written;
}

size_t generate_csv(char* restrict buffer, size_t size, unsigned int columns) {
    char* restrict current_char = buffer;
    for (unsigned int i = 1; i <= columns; i++) {
        if (i > 1) {
            *current_char++ = ',';
            size--;
        }
        memcpy(current_char, "column", 6);
        current_char += 6;
        size -= 6;
        int len = snprintf(current_char, (CHAR_BIT * sizeof(int) - 1) / 3 + 2, "%d", i);
        current_char += len;
        size -= len;
    }
    *current_char++ = '\n';
    size--;

    pcg32_random_t rng;

    pcg32_srandom_r(&rng, 42u, 11u);

    unsigned int cell_large_max = 255;

    while (size > ((cell_large_max + 1) * columns + 1)) {
        size_t written = random_cell(&rng, current_char, columns, cell_large_max);
        current_char += written;
        size -= written;
    }
    size_t written = random_cell(&rng, current_char, columns, 2);
    current_char += written;
    return current_char - buffer;
}
