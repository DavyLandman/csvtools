#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <string.h>
#include "debug.h"
#include "hints.h"



#define NULL_ENCODED '\x1a'

#if UINT32_MAX == UINT_FAST32_MAX
    #define UINT_FAST32_C(v) UINT32_C(v)
#elif UINT64_MAX == UINT_FAST32_MAX
    #define UINT_FAST32_C(v) UINT64_C(v)
#else
    #error "Only added code for 32 and 64 bit scanning"
#endif

#define REPEAT(n) (~UINT_FAST32_C(0)/255 * (n))
#define HAS_ZERO(v) (((v) - REPEAT(0x10)) & ~(v) & REPEAT(0x80))

#define HAS_VALUE(x,m) (HAS_ZERO((x) ^ (m)))
#define IS_ALIGNED(p,s) (((uintptr_t)(const void*)(p)) % (s) == 0)
#define FLOOR_ALIGNED(p,s) ((const void*)((((uintptr_t)(const void*)(p)) / (s)) * (s)))

#define NULL_ENCODED_MASK REPEAT(NULL_ENCODED)

#ifndef BUFFER_SIZE
    #define BUFFER_SIZE 8*1024*1024
#endif

static char _buffer[BUFFER_SIZE];

static FILE* _source;

static void parse_config(int argc, char** argv);
static void do_unpipe(size_t chars_read);

int main(int argc, char** argv) {
    parse_config(argc, argv);

    size_t chars_read;
    SEQUENTIAL_HINT(_source);
    while ((chars_read = fread(_buffer, sizeof(char), BUFFER_SIZE, _source)) > 0) {
        do_unpipe(chars_read);
    }
    if (_source != stdin) {
        fclose(_source);
    }
    return 0;
}

static void print_help() {
    fprintf(stderr, "usage: csvunpipe [OPTIONS] [FILE]");
    fprintf(stderr, "options:");
    fprintf(stderr, "-p header,row,to,print\n");
    fprintf(stderr, "  Header row to print first\n");
}

static void parse_config(int argc, char** argv) {
    _source = stdin;
    char c;
    while ((c = getopt (argc, argv, "p:")) != -1) {
        switch (c) {
            case 'p':
                fwrite(optarg, sizeof(char), strlen(optarg), stdout);
                fwrite("\n", sizeof(char), 1, stdout);
                break;
            case '?':
            case 'h':
            default:
                print_help();
                exit(1);
                break;
        }
    }
    if (optind < argc) {
        _source = fopen(argv[optind], "r");
        if (!_source) {
            fprintf(stderr, "Could not open file %s for reading\n", argv[optind]);
            exit(1);
        }
    }
}

#define CHECK_REPLACE(p,off, ch, rp) if (((char* restrict)(p))[off] == (ch)) { ((char* restrict)(p))[off] = (rp); }

#if UINT32_MAX == UINT_FAST32_MAX
    #define CHECK_REPLACE_LARGE(p, ch, rp) \
        CHECK_REPLACE((p), 0, ch, rp) \
        CHECK_REPLACE((p), 1, ch, rp) \
        CHECK_REPLACE((p), 2, ch, rp) \
        CHECK_REPLACE((p), 3, ch, rp) 
#elif UINT64_MAX == UINT_FAST32_MAX
    #define CHECK_REPLACE_LARGE(p, ch, rp) \
        CHECK_REPLACE((p), 0, ch, rp) \
        CHECK_REPLACE((p), 1, ch, rp) \
        CHECK_REPLACE((p), 2, ch, rp) \
        CHECK_REPLACE((p), 3, ch, rp) \
        CHECK_REPLACE((p), 4, ch, rp) \
        CHECK_REPLACE((p), 5, ch, rp) \
        CHECK_REPLACE((p), 6, ch, rp) \
        CHECK_REPLACE((p), 7, ch, rp) 
#else
    #error "Only added code for 32 and 64 bit scanning"
#endif

#define SCAN_REPLACE(start, end, find, repl) \
    do { \
        char* restrict __current_char = (start); \
        while (__current_char < (end) && !IS_ALIGNED(__current_char, sizeof(uint_fast32_t))) { \
            CHECK_REPLACE(__current_char, 0, find, repl); \
            __current_char++; \
        } \
        const uint_fast32_t* restrict __large_steps = (const uint_fast32_t* restrict)__current_char; \
        const uint_fast32_t* restrict __large_end = FLOOR_ALIGNED((end), sizeof(uint_fast32_t)); \
        while (__large_steps < __large_end) { \
            if (HAS_VALUE(*__large_steps, REPEAT((find)))) { \
                CHECK_REPLACE_LARGE(__large_steps, (find), (repl)); \
            } \
            __large_steps++; \
        } \
        __current_char = (char* restrict)__large_steps; \
        while (__current_char < (end)) { \
            CHECK_REPLACE(__current_char, 0, find, repl); \
            __current_char++; \
        } \
    } \
    while(0); 

static void do_unpipe(size_t chars_read) {
    SCAN_REPLACE(_buffer, _buffer + chars_read, '\0', '\n');
    SCAN_REPLACE(_buffer, _buffer + chars_read, NULL_ENCODED, '\0');
    fwrite(_buffer, sizeof(char), chars_read, stdout);
}

