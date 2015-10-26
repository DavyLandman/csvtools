#include <stdio.h>
#include <stdlib.h>
#include "generate.h"

int main(int argc, char** argv) {
    size_t bench_size = 8*1024*1024;
    char* buffer = calloc(bench_size, sizeof(char));
    fprintf(stderr, "Filling buffer %lu\n",bench_size); 
    size_t data_filled = generate_csv(buffer, bench_size, 42, 11, 5);
    fprintf(stderr, "Printing buffer %lu\n",data_filled); 
    fwrite(buffer, sizeof(char), data_filled, stdout);
    return 0;
}
