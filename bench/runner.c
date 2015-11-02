#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include "timer.h"
#include "generate.h"

static void print_help() {
    fprintf(stderr,"usage: bench [OPTIONS]\n");
    fprintf(stderr,"options:\n");
    fprintf(stderr, "-b 200\n");
    fprintf(stderr, "\tbench size in MBs\n");
    fprintf(stderr, "-e 2\n");
    fprintf(stderr, "\tenlarge bench size by repeating it x times\n");
    fprintf(stderr, "-c 6\n");
    fprintf(stderr, "\tcolumns to generate\n");
    fprintf(stderr, "-r 5\n");
    fprintf(stderr, "\tnumber of measure runs\n");
    fprintf(stderr, "-s 42\n");
    fprintf(stderr, "\tseed for random generator\n");
    fprintf(stderr, "-x\n");
    fprintf(stderr, "\tonly run the csvtools (used for comparing new commits)\n");
    fprintf(stderr, "-p\n");
    fprintf(stderr, "\toutput the generated data to stdout\n");
}

static void run(const char* restrict command, const char* restrict buffer, size_t buffer_size, unsigned int buffer_copy, unsigned int repeats, double* results) {
    for (unsigned int r = 0; r < repeats; r++) {
        FILE* target = popen(command, "w");
        if (!target) {
            fprintf(stderr, "Can't start \"%s\"\n", command);
            results[r] = -1;
        }
        double start = getRealTime();
        for (unsigned int b = 0; b < buffer_copy; b++) {
            fwrite(buffer, sizeof(char), buffer_size, target);
            fflush(target);
        }
        if (pclose(target) != 0) {
            fprintf(stderr, "\"%s\" had an error.\n", command);
            results[r] = -1;
        }
        double stop = getRealTime();
        results[r] = (stop - start);
    }
}

/* base on source: nneonneo in http://stackoverflow.com/questions/12890008/replacing-character-in-a-string */
char *replace(const char *s, char ch, const char *repl) {
    int count = 0;
    for(const char* t=s; *t; t++)
        count += (*t == ch);

    size_t rlen = strlen(repl);
    char *res = malloc(strlen(s) + (rlen-1)*count + 1);
    char *ptr = res;
    for(const char* t=s; *t; t++) {
        if(*t == ch) {
            memcpy(ptr, repl, rlen);
            ptr += rlen;
        } else {
            *ptr++ = *t;
        }
    }
    *ptr = 0;
    return res;
}

int compare_double(const void *d1, const void *d2) { 
    return ( *(double*)d1 < *(double*)d2) ? 1 : -1 ; 
} 

static double median(double* data, size_t elements) {
    if (elements % 2 == 1) {
        return data[((elements + 1) / 2) - 1];
    }
    return (data[((elements + 1) / 2) - 1] + data[((elements + 1) / 2)]) / 2;
}

static double to_MBps(double n, size_t buffer_size, unsigned int buffer_copy) {
    return( (buffer_size * buffer_copy) / n) / (1024*1024);
}

static void print_run(const char* program, const char* name, const char* restrict command, const char* restrict buffer, size_t buffer_size, unsigned int buffer_copy, unsigned int repeats) {
    double* results = calloc(repeats, sizeof(double));
    run(command, buffer, buffer_size, buffer_copy, repeats, results);
    qsort(results, repeats, sizeof(double), compare_double);

    char* command_escaped = replace(command, '"', "\"\"");
    char* name_escaped = replace(name, '"', "\"\"");
    fprintf(stdout, "%s,\"%s\",\"%s\"", program, name_escaped, command_escaped);
    fprintf(stdout, ",%f,%f,%f", to_MBps(results[0], buffer_size, buffer_copy), to_MBps(results[repeats - 1], buffer_size, buffer_copy), to_MBps(median(results, repeats), buffer_size, buffer_copy));
    fprintf(stdout, "\n");
    free(command_escaped);
    free(name_escaped);
    free(results);
}


static void csvgrep_csvkit(const char* restrict buffer, size_t buffer_size, unsigned int buffer_copy, unsigned int repeats, unsigned int columns) {
    (void)buffer_copy; // not used
    fprintf(stderr, "Running csvkit csvgrep\n");
    print_run("csvkit csvgrep", "first column", "python bench/csvkit-csvgrep.py -c column1 -r '.*[a-e]+.*' > /dev/null", buffer, buffer_size, 1, repeats);

    char command[255];
    sprintf(command, "python bench/csvkit-csvgrep.py -c column%u -r '.*[a-e]+.*' > /dev/null", columns / 2);
    print_run("csvkit csvgrep", "middle column", command, buffer, buffer_size, 1, repeats);

    sprintf(command, "python bench/csvkit-csvgrep.py -c column%u -r '.*[a-e]+.*' > /dev/null", columns);
    print_run("csvkit csvgrep", "last column", command , buffer, buffer_size, 1, repeats);

    sprintf(command, "python bench/csvkit-csvgrep.py -c column%u,column%u -r '.*[a-e]+.*' > /dev/null", columns / 2, columns - 1);
    print_run("csvkit csvgrep", "two columns", command , buffer, buffer_size, 1, repeats);
}

static void csvgrep_awk(const char* restrict buffer, size_t buffer_size, unsigned int buffer_copy, unsigned int repeats, unsigned int columns) {
    fprintf(stderr, "Running awk grep\n");
    print_run("awk grep", "first column", "LC_ALL='C' awk -F\",\" '$1 ~ /.*[a-e]+.*/ { print }' > /dev/null", buffer, buffer_size, 1, repeats);

    char command[255];
    sprintf(command, "LC_ALL='C' awk -F\",\" '$%u ~ /.*[a-e]+.*/ { print }' > /dev/null", columns / 2);
    print_run("awk grep", "middle column", command, buffer, buffer_size, 1, repeats);

    sprintf(command, "LC_ALL='C' awk -F\",\" '$%u ~ /.*[a-e]+.*/ { print }' > /dev/null", columns);
    print_run("awk grep", "last column", command , buffer, buffer_size, buffer_copy, repeats);

    sprintf(command, "LC_ALL='C' awk -F\",\" '$%u ~ /.*[a-e]+.*/ && $%u ~ /.*[F-L]+.*/ { print }' > /dev/null", columns / 2, columns - 1);
    print_run("awk grep", "two columns", command , buffer, buffer_size, buffer_copy, repeats);
}

static void csvgrep_csvtools(const char* restrict buffer, size_t buffer_size, unsigned int buffer_copy, unsigned int repeats, unsigned int columns) {
    fprintf(stderr, "Running csvtools csvgrep\n");
    print_run("csvtools csvgrep", "first column", "bin/csvgrep -p 'column1/[a-e]+/' > /dev/null", buffer, buffer_size, buffer_copy, repeats);

    char command[255];
    sprintf(command, "bin/csvgrep -p 'column%u/[a-e]+/' > /dev/null", columns / 2);
    print_run("csvtools csvgrep", "middle column", command, buffer, buffer_size, buffer_copy, repeats);

    sprintf(command, "bin/csvgrep -p 'column%u/[a-e]+/' > /dev/null", columns);
    print_run("csvtools csvgrep", "last column", command , buffer, buffer_size, buffer_copy, repeats);

    sprintf(command, "bin/csvgrep -p 'column%u/[a-e]+/' -p 'column%u/[F-L]+/' > /dev/null", columns / 2 , columns - 1);
    print_run("csvtools csvgrep", "two columns", command , buffer, buffer_size, buffer_copy, repeats);
}

static void repeat(char* restrict target, const char* restrict val, const char separator, size_t repeats) {
    size_t val_length = strlen(val);
    for (unsigned int r = 0; r < repeats; r++) {
        if (r > 0) {
            *target++ = separator;
        }
        memcpy(target, val, val_length);
        target += val_length;
    }
    *target = '\0';
}

static void csvgrep_gnugrep(const char* restrict buffer, size_t buffer_size, unsigned int buffer_copy, unsigned int repeats, unsigned int columns) {
    fprintf(stderr, "Running gnu grep\n");

    print_run("gnutools grep", "first column", "LC_ALL='C' grep \"^[^,a-e]*[a-e][a-e]*\" > /dev/null", buffer, buffer_size, buffer_copy, repeats);

    char skip_commands[1024];
    repeat(skip_commands, "[^,]*", ',', columns / 2);
    char command[255];
    sprintf(command, "LC_ALL='C' grep \"^%s,[^,a-e]*[a-e][a-e]*\" > /dev/null", skip_commands);
    print_run("gnutools grep", "middle column", command, buffer, buffer_size, buffer_copy, repeats);

    print_run("gnutools grep", "last column", "LC_ALL='C' grep \"[a-e][a-e]*[^,a-e]*$\" > /dev/null", buffer, buffer_size, buffer_copy, repeats);

    char skip_commands2[1024];
    repeat(skip_commands2, "[^,]*", ',', (columns - 1) - (columns / 2));
    sprintf(command, "LC_ALL='C' grep \"^%s,[^,a-e]*[a-e][a-e]*[^,]*,%s,[^,F-L][F-L][F-L]*\" > /dev/null", skip_commands, skip_commands2);
    print_run("gnutools grep", "two columns", command , buffer, buffer_size, buffer_copy, repeats);
}


static void csvcut_csvtools(const char* restrict buffer, size_t buffer_size, unsigned int buffer_copy, unsigned int repeats, unsigned int columns) {
    fprintf(stderr, "Running csvtools csvcut\n");
    print_run("csvtools csvcut", "first column", "bin/csvcut -d column1 > /dev/null", buffer, buffer_size, buffer_copy, repeats);

    char command[255];
    sprintf(command, "bin/csvcut -d column%u > /dev/null", columns / 2);
    print_run("csvtools csvcut", "middle column", command, buffer, buffer_size, buffer_copy, repeats);

    sprintf(command, "bin/csvcut -d column%u > /dev/null", columns);
    print_run("csvtools csvcut", "last column", command, buffer, buffer_size, buffer_copy, repeats);

    sprintf(command, "bin/csvcut -d column%u,column%u > /dev/null", columns - 3,columns - 2);
    print_run("csvtools csvcut", "two adjoining column", command, buffer, buffer_size, buffer_copy, repeats);

    sprintf(command, "bin/csvcut -d column%u,column%u > /dev/null", columns / 2,columns - 1);
    print_run("csvtools csvcut", "two distinct column", command, buffer, buffer_size, buffer_copy, repeats);
}

static void csvcut_csvkit(const char* restrict buffer, size_t buffer_size, unsigned int buffer_copy, unsigned int repeats, unsigned int columns) {
    (void)buffer_copy; // not used
    fprintf(stderr, "Running csvkit csvcut\n");
    print_run("csvkit csvcut", "first column", "python bench/csvkit-csvcut.py -c column1 > /dev/null", buffer, buffer_size, 1, repeats);

    char command[255];
    sprintf(command, "python bench/csvkit-csvcut.py -c column%u > /dev/null", columns / 2);
    print_run("csvkit csvcut", "middle column", command, buffer, buffer_size, 1, repeats);

    sprintf(command, "python bench/csvkit-csvcut.py -c column%u > /dev/null", columns);
    print_run("csvkit csvcut", "last column", command, buffer, buffer_size, 1, repeats);

    sprintf(command, "python bench/csvkit-csvcut.py -c column%u,column%u > /dev/null", columns - 3,columns - 2);
    print_run("csvkit csvcut", "two adjoining column", command, buffer, buffer_size, 1, repeats);

    sprintf(command, "python bench/csvkit-csvcut.py -c column%u,column%u > /dev/null", columns / 2,columns - 1);
    print_run("csvkit csvcut", "two distinct column", command, buffer, buffer_size, 1, repeats);
}

static void csvcut_gnucut(const char* restrict buffer, size_t buffer_size, unsigned int buffer_copy, unsigned int repeats, unsigned int columns) {
    fprintf(stderr, "Running gnu cut\n");

    char args[255];
    args[0] = '\0';
    size_t written = 0;
    for (unsigned int col = 2; col <= columns; col++) {
        sprintf(args + written, "%u", col);
        written = strlen(args);
        args[written++] = ',';
    }
    if (written > 0) {
        args[written - 1] = '\0';
    }

    char command[255];
    sprintf(command, "cut -d ',' -f %s > /dev/null", args);
    print_run("cut csvcut", "first column", command, buffer, buffer_size, 1, repeats);

    args[0] = '\0';
    written = 0;
    for (unsigned int col = 1; col <= columns; col++) {
        if (col != columns / 2) {
            sprintf(args + written, "%u", col);
            written = strlen(args);
            args[written++] = ',';
        }
    }
    if (written > 0) {
        args[written - 1] = '\0';
    }
    sprintf(command, "cut -d ',' -f %s > /dev/null", args);
    print_run("cut csvcut", "middle column", command, buffer, buffer_size, 1, repeats);

    args[0] = '\0';
    written = 0;
    for (unsigned int col = 1; col <= columns - 1; col++) {
        sprintf(args + written, "%u", col);
        written = strlen(args);
        args[written++] = ',';
    }
    if (written > 0) {
        args[written - 1] = '\0';
    }
    sprintf(command, "cut -d ',' -f %s > /dev/null", args);
    print_run("cut csvcut", "last column", command, buffer, buffer_size, 1, repeats);

    args[0] = '\0';
    written = 0;
    for (unsigned int col = 1; col <= columns; col++) {
        if (col == columns - 3 || col == columns - 2) {
            sprintf(args + written, "%u", col);
            written = strlen(args);
            args[written++] = ',';
        }
    }
    if (written > 0) {
        args[written - 1] = '\0';
    }
    sprintf(command, "cut -d ',' -f %s > /dev/null", args);
    print_run("cut csvcut", "two adjoining column", command, buffer, buffer_size, buffer_copy, repeats);

    args[0] = '\0';
    written = 0;
    for (unsigned int col = 1; col <= columns; col++) {
        if (col == columns / 2 || col == columns - 1) {
            sprintf(args + written, "%u", col);
            written = strlen(args);
            args[written++] = ',';
            args[written] = '\0';
        }
    }
    if (written > 0) {
        args[written - 1] = '\0';
    }
    sprintf(command, "cut -d ',' -f %s > /dev/null", args);
    print_run("cut csvcut", "two distinct column", command, buffer, buffer_size, buffer_copy, repeats);
}

static void csvcut_sed(const char* restrict buffer, size_t buffer_size, unsigned int buffer_copy, unsigned int repeats, unsigned int columns) {
    fprintf(stderr, "Running gnu sed\n");

    print_run("sed csvcut", "first column", "LC_ALL='C' sed 's/^[^,]*,//' > /dev/null", buffer, buffer_size, buffer_copy, repeats);

    char command[255];
    sprintf(command, "LC_ALL='C' sed 's/[^,]*,//%u' > /dev/null", columns / 2);
    print_run("sed csvcut", "middle column", command, buffer, buffer_size, buffer_copy, repeats);

    sprintf(command, "LC_ALL='C' sed 's/[^,]*,//%u' > /dev/null", columns - 1);
    print_run("sed csvcut", "last column", command, buffer, buffer_size, buffer_copy, repeats);

    sprintf(command, "LC_ALL='C' sed -e 's/[^,]*,//%u' -e 's/[^,]*,//%u' > /dev/null", columns - 2, columns - 3 );
    print_run("sed csvcut", "two adjoining column", command, buffer, buffer_size, buffer_copy, repeats);

    sprintf(command, "LC_ALL='C' sed -e 's/[^,]*,//%u' -e 's/[^,]*,//%u' > /dev/null", columns - 1, columns / 2);
    print_run("sed csvcut", "two distinct column", command, buffer, buffer_size, buffer_copy, repeats);

}
// based on xxhash avalanche
#define PRIME1   2654435761U
#define PRIME2   2246822519U
#define PRIME3   3266489917U

static unsigned int xxh_mix(unsigned int x, unsigned int seed) {
    unsigned int crc = x  + seed + PRIME1;
    crc ^= crc >> 15;
    crc *= PRIME2;
    crc ^= crc >> 13;
    crc *= PRIME3;
    crc ^= crc >> 16;
    return crc;
}

int main(int argc, char** argv) {
    size_t bench_size = 200 * 1024 * 1024;
    unsigned int columns = 6;
    unsigned int repeats = 5;
    unsigned int bench_copy = 2;
    unsigned int seed1 = xxh_mix(29, 42);
    unsigned int seed2 = xxh_mix(13, 11);
    bool only_csvtools = false;
    bool output_stdout = false;

    char c;
    while ((c = getopt (argc, argv, "b:c:r:e:s:xph")) != -1) {
        switch (c) {
            case 'b':
                sscanf(optarg, "%zu", &bench_size);
                bench_size *= 1024 * 1024;
                break;
            case 'c':
                sscanf(optarg, "%u", &columns);
                break;
            case 'r':
                sscanf(optarg, "%u", &repeats);
                break;
            case 'e':
                sscanf(optarg, "%u", &bench_copy);
                break;
            case 's':
                sscanf(optarg, "%u", &seed1);
                seed1 = xxh_mix(seed1, 42);
                break;
            case 'x':
                only_csvtools = true;
                break;
            case 'p':
                output_stdout = true;
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
    fprintf(stderr, "Preparing data (%zu bytes)\n",bench_size);
    size_t data_filled_small;
    size_t data_filled = generate_csv(buffer, bench_size, &data_filled_small, seed1, seed2, columns);
    fprintf(stderr, "Data ready (%zu bytes)\n",data_filled);
    if (output_stdout) {
        for (unsigned int b = 0; b < bench_copy; b++) {
            fwrite(buffer, sizeof(char), data_filled, stdout);
            fflush(stdout);
        }
        return 0;
    }

    fprintf(stdout, "program,name,command,min speed,max speed,median speed");
    fprintf(stdout, "\n");

    fprintf(stderr, "Running pipe bench fist\n");
    if (!only_csvtools) {
        print_run("bench pipe", "cat", "cat > /dev/null", buffer, data_filled, bench_copy, repeats);
        print_run("bench pipe", "wc -l",  "wc -l > /dev/null", buffer, data_filled, bench_copy, repeats);
        print_run("bench pipe", "md5sum", "md5sum > /dev/null", buffer, data_filled, bench_copy, repeats);
    }

    csvgrep_csvtools(buffer, data_filled, bench_copy, repeats, columns);
    if (!only_csvtools) {
        csvgrep_csvkit(buffer, data_filled_small, bench_copy, repeats, columns);
        csvgrep_awk(buffer, data_filled, bench_copy, repeats, columns);
        csvgrep_gnugrep(buffer, data_filled, bench_copy, repeats, columns);
    }

    csvcut_csvtools(buffer, data_filled, bench_copy, repeats, columns);
    if (!only_csvtools) {
        csvcut_csvkit(buffer, data_filled_small, bench_copy, repeats, columns);
        csvcut_gnucut(buffer, data_filled, bench_copy, repeats, columns);
        csvcut_sed(buffer, data_filled, bench_copy, repeats, columns);
    }


    return 0;
}
