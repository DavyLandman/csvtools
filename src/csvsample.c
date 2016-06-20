#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <assert.h>
#include <string.h>
#include <pcre.h> 
#include "csv_tokenizer.h"
#include "debug.h"
#include "hints.h"

#include "pcg_variants.h"
#include "entropy.h"

#if __SIZEOF_INT128__
    #define BIG_RANDOM
#endif


#define CELL_BUFFER_SIZE (BUFFER_SIZE / 2) + 2
struct csv_tokenizer* _tokenizer;
static char _buffer[BUFFER_SIZE + BUFFER_TOKENIZER_POSTFIX];
static Cell _cells[CELL_BUFFER_SIZE];

static struct {
    FILE* source;
    char separator;
    char newline[2];
    size_t newline_length;
    size_t column_count;
    uint64_t sample_size;

    char** samples;
    size_t** sample_sizes;
#if BIG_RANDOM
    pcg64_random_t rng;
#else
    pcg32_random_t rng1;
    pcg32_random_t rng2;
#endif

} config;

static void parse_config(int argc, char** argv);
static size_t output_header(size_t cells_found);
static void sample_cells(size_t cells_found, size_t offset, bool last_full);
static void output_sampled_cells();

int main(int argc, char** argv) {
    parse_config(argc, argv);

    size_t chars_read;
    bool first = true;
    SEQUENTIAL_HINT(config.source);
    while ((chars_read = fread(_buffer, sizeof(char), BUFFER_SIZE, config.source)) > 0) {
        LOG_D("New data read: %zu\n", chars_read);
        prepare_tokenization(_tokenizer, _buffer, chars_read);
        size_t buffer_consumed = 0;
        size_t cells_found = 0;
        bool last_full = true;

        while (buffer_consumed < chars_read) {
            tokenize_cells(_tokenizer, buffer_consumed, chars_read, &buffer_consumed, &cells_found, &last_full);
            LOG_D("Processed: %zu, Cells: %zu\n", buffer_consumed, cells_found);
            size_t cell_offset = 0;

            size_t cell_offset = 0;
            if (first) {
                first = false;
                cell_offset = output_header(cells_found);
            }
            sample_cells(cells_found, cell_offset, last_full);
        }
    }
    output_sampled_cells();
    if (_tokenizer != NULL) {
        free_tokenizer(_tokenizer);
    }
    if (config.samples != NULL) {
        for (int s = 0; s < config.sample_size; s++) {
            free(config.samples[s]);
        }
        free(config.samples);
    }
    if (config.source != stdin) {
        fclose(config.source);
    }
}

static void print_help() {
    fprintf(stderr,"usage: csvsample [OPTIONS] [FILE]\n");
    fprintf(stderr,"options:\n");
    fprintf(stderr, "-s ,\n");
    fprintf(stderr, "\tWhich character to use as separator (default is ,)\n");
    fprintf(stderr, "-n 10\n");
    fprintf(stderr, "\tHow many samples to collect (default is 10)\n");
    fprintf(stderr, "-u 42\n");
    fprintf(stderr, "\tUse a deterministic seed (by default /dev/random is used). For two invocations with the same seed value, same sample size (-n), and the same input, the same rows will be sampled\n");
}

static void parse_config(int argc, char** argv) {
    config.source = stdin;
    config.separator = ',';
    config.sample_size = 10;
    
    
    bool nondeterministic_seed = true;
    uint64_t seed = -1;
    char c;
    while ((c = getopt (argc, argv, "s:n:u:")) != -1) {
        switch (c) {
            case 's': 
                config.separator = optarg[0];
                break;
            case 'n':
                if (sscanf(optarg, "%llu", &config.sample_size) != 1) {
                    fprintf(stderr, "Error converting %s to a positive number\n", optarg);
                    print_help();
                    exit(1);
                }
                break;
            case 'u':
                nondeterministic_seed = false;
                if (sscanf(optarg, "%zu", &seed) != 1) {
                    fprintf(stderr, "Error converting %s to a positive number\n", optarg);
                    print_help();
                    exit(1);
                }
                break;
            case '?':
            case 'h':
                print_help();
                exit(1);
                break;
        }
    }
    if (optind < argc) {
        config.source = fopen(argv[optind], "r");
        if (!config.source) {
            fprintf(stderr, "Could not open file %s for reading\n", argv[optind]);
            exit(1);
        }
    }
                
    if (nondeterministic_seed) {
        uint64_t seeds[2];
        entropy_getbytes((void*)seeds, sizeof(seeds));
        pcg32_srandom_r(&(config.rng), seeds[0], seeds[1]);
    } else {
        pcg32_srandom_r(&(config.rng), UINT64_C(0x42) * UINT64_C(0x7FFFFFFFFFFFFFE7), seed);
    }
    
    config.samples = calloc(config.sample_size, sizeof(char*));
    config.sample_sizes = calloc(config.sample_size, sizeof(size_t*));
}

static size_t output_header(size_t cells_found) {
    Cell* current_cell = _cells;
    while (current_cell < (_cells + cells_found) && current_cell->start != NULL) {
        // also immediatly print the header
        if (current_cell != _cells) {
            fwrite(&(config.separator),sizeof(char),1, stdout);
        }
        fwrite(current_cell->start, sizeof(char), current_cell->length, stdout);
        current_cell++;
    }
    config.column_count = (current_cell - _cells);
    const char* new_line = _cells[config.column_count-1].start + _cells[config.column_count - 1].length;
    config.newline[0] = new_line[0];
    config.newline_length = 1;
    if (new_line[1] == '\n' && new_line[0] == '\r') {
        config.newline[1] = '\n';
        config.newline_length = 2;
    }
    fwrite(config.newline, sizeof(char), config.newline_length, stdout);
    return config.column_count + 1 ;
}


static bool _prev_sample = false;
static sample_t _rows_seen = 0;
static bool _half_line = false;
static bool _half_cell = false;
static size_t _target_sample = -1;


static void sample_cells(size_t cells_found, size_t offset, bool last_full) {
    Cell const* restrict current_cell = _cells + offset;
    Cell const* restrict cells_end = _cells + cells_found;
    if (_half_line) {
        if (_prev_sample) {
            // TODO: implement the case of the half sample
        }
        else {
            // skip until the end of the line
            while (++current_cell < cell_end, current_cell->start != NULL);
            _rows_seen++;
        }
    }
    // we start at a new line
    while (current_cell < cells_end) {
        bool will_sample = false;
        _target_sample = -1;
        if (_rows_seen < config.sample_size) {
            will_sample = true;
            _target_sample = _rows_seen;
        }
        else {
            

        }
        
        char const* restrict current_line_start = current_cell->start;
        size_t current_line_length = 0;

}
