#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "generate.h"

static void print_help() {
    fprintf(stderr,"usage: bench [OPTIONS]");
    fprintf(stderr,"options:");
    fprintf(stderr, "-b 200\n");
    fprintf(stderr, "\tbench size in MBs\n");
    fprintf(stderr, "-c 6\n");
    fprintf(stderr, "\tcolumns to generate\n");
    fprintf(stderr, "-r 5\n");
    fprintf(stderr, "\tnumber of measure runs\n");
}

int main(int argc, char** argv) {
    size_t bench_size = 200 * 1024 * 1024; 
    unsigned int columns = 6;
    unsigned int repeats = 5;

    char c;
    while ((c = getopt (argc, argv, "b:c:r:h")) != -1) {
        switch (c) {
            case 'b':
                sscanf(optarg, "%zd", &bench_size);
                bench_size *= 1024 * 1024;
                break;
            case 'c':
                sscanf(optarg, "%u", &columns);
                break;
            case 'r':
                sscanf(optarg, "%u", &repeats);
                break;
            case '?':
            case 'h':
            default:
                print_help();
                exit(1);
                break;
        }
    }
    char* buffer = calloc(bench_size, sizeof(char));
    fprintf(stderr, "Filling buffer %lu\n",bench_size); 
    size_t data_filled = generate_csv(buffer, bench_size, 42, 11, columns);
    fprintf(stderr, "Printing buffer %lu\n",data_filled); 
    fwrite(buffer, sizeof(char), data_filled, stdout);
    return 0;
}
