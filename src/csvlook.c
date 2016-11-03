#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <stdbool.h>
#include <assert.h>
#include <string.h>
#include "csv_tokenizer.h"
#include "debug.h"
#include "hints.h"

//#define BUFFER_SIZE 30
//#define BUFFER_SIZE 72
#define CELL_BUFFER_SIZE (BUFFER_SIZE / 2) + 2


struct csv_tokenizer* _tokenizer;

static char _buffer[BUFFER_SIZE + BUFFER_TOKENIZER_POSTFIX];
static Cell _cells[CELL_BUFFER_SIZE];

static struct {
    FILE* source;

    char separator;
    char newline[2];
    size_t newline_length;

    int column_count;
    int *column_sizes;
    int *final_column_sizes;
} config;

static struct {
    char *specified_widths;
    int  max_width;
} preconfig;

static void parse_config(int argc, char** argv);
static void finish_config(size_t cells_found);

static void output_cells(size_t cells_found, bool last_full);
static void debug_cells(size_t total);

int main(int argc, char** argv) {
    size_t chars_read;
    bool first = true;

    parse_config(argc, argv);

    SEQUENTIAL_HINT(config.source);
    while ((chars_read = fread(_buffer, 1, BUFFER_SIZE, config.source)) > 0) {
        LOG_D("New data read: %zu\n", chars_read);
        size_t buffer_consumed = 0;
        size_t cells_found = 0;
        bool last_full = true;

        prepare_tokenization(_tokenizer, _buffer, chars_read);
        while (buffer_consumed < chars_read) {
            tokenize_cells(_tokenizer, buffer_consumed, chars_read, &buffer_consumed, &cells_found, &last_full);
            LOG_D("Processed: %zu, Cells: %zu\n", buffer_consumed, cells_found);
            debug_cells(cells_found);
            if (first == true) {
                first = false;
                finish_config(cells_found);
            }

            output_cells(cells_found, last_full);
        }
    }
    if (_tokenizer != NULL) {
        free_tokenizer(_tokenizer);
    }
    if (preconfig.specified_widths != NULL) {
        free(preconfig.specified_widths);
    }
    if (config.column_sizes != NULL) {
        free(config.column_sizes);
    }
    if (config.final_column_sizes != NULL) {
        free(config.final_column_sizes);
    }
    if (config.source != stdin) {
        fclose(config.source);
    }
    if (stdout != stdout) {
        fclose(stdout);
    }
    return 0;
}

static void debug_cells(size_t total) {
#ifdef MOREDEBUG
    Cell* current_cell = _cells;
    Cell* cell_end = _cells + total;

    while (current_cell < cell_end) {
        if (current_cell->start == NULL) {
            LOG_V("Cell %zu : Newline\n", (size_t)(current_cell - _cells));
        }
        else if (current_cell->length == 0) {
            LOG_V("Cell %zu : \n", (size_t)(current_cell - _cells));
        }
        else {
            LOG_V("%.*s ", (int)current_cell->length, current_cell->start);
        }
        current_cell++;
    }
#else
    (void)total;
#endif
}

static void print_help() {
    fprintf(stderr,"usage: csvlook [OPTIONS] [FILE]]\n");
    fprintf(stderr,"options:\n");
    fprintf(stderr, "-s ,\n");
    fprintf(stderr, "\tWhich character to use as separator (default is ,)\n");
    fprintf(stderr, "-w ,,30,,\n");
    fprintf(stderr, "\tSpecified column widths\n");
    fprintf(stderr, "-M 120\n");
    fprintf(stderr, "\tMaximum column width\n");
}

static void parse_config(int argc, char** argv) {
    config.separator = ',';
    config.column_sizes = NULL;
    config.final_column_sizes = NULL;
    config.source = stdin;

    preconfig.max_width = 120;
    preconfig.specified_widths = NULL;

    char c;
    while ((c = getopt (argc, argv, "s:w:n:b:M:h")) != -1) {
        switch (c) {
            case 's':
                if (strcmp(optarg, "\\t") == 0)
                    config.separator = '\t';
                else
                    config.separator = optarg[0];
                break;
            case 'w':
                preconfig.specified_widths = malloc(sizeof(char) * strlen(optarg) + 1);
                strcpy(preconfig.specified_widths, optarg);
            case 'M':
                preconfig.max_width = atoi(optarg);
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

    LOG_D("%s\n","Done parsing config params");

    _tokenizer = setup_tokenizer(config.separator, _buffer, _cells, CELL_BUFFER_SIZE);
}

static void finish_config(size_t cells_found) {

    Cell const* current_cell = _cells;

    while (current_cell < (_cells + cells_found) && current_cell->start != NULL) {
        current_cell++;
    }
    config.column_count = (int)(current_cell - _cells);
    config.column_sizes = calloc(sizeof(int), config.column_count);
    config.final_column_sizes = calloc(sizeof(int), config.column_count);

    const char* new_line = _cells[config.column_count-1].start + _cells[config.column_count - 1].length;
    config.newline[0] = new_line[0];
    config.newline_length = 1;
    if (new_line[1] == '\n' || new_line[0] == '\r') {
        config.newline[1] = '\n';
        config.newline_length = 2;
    }

    LOG_V("%s\n", "Starting column width detection");
    current_cell = _cells;
    Cell* cell_end = _cells + cells_found;
    int column_idx = 0;
    while (current_cell < cell_end) {
        if (current_cell->start == NULL) {
            column_idx = 0;
            LOG_V("%s\n","New Row");
        }
        else {
            LOG_V("Processing column %d: Prev max %d Cur len %d\n", column_idx, config.column_sizes[column_idx], (int)current_cell->length);
            /* max(prevmax, curlength) */
            config.column_sizes[column_idx] = (config.column_sizes[column_idx] < (int)current_cell->length) ? (int)current_cell->length : config.column_sizes[column_idx];
            /* min(curmax, configmax) */
            config.column_sizes[column_idx] = (config.column_sizes[column_idx] > preconfig.max_width) ? preconfig.max_width : config.column_sizes[column_idx];
            column_idx++;
        }
        current_cell++;
    }
    column_idx = 0;
    LOG_D("%s\n", "Trying to parse specified widths");
    char *token = strtok(preconfig.specified_widths, ",");
    while (token != NULL) {
        LOG_D("Got token %s\n", token);
        if (strlen(token) > 0) {
            int len = atoi(token);
            if (len > 0) {
                config.column_sizes[column_idx] = len;
            }
        }

        column_idx++;
        token = strtok(NULL, ",");
    }

    LOG_D("%s\n","Done finishing config calcs");
    for (int i = 0; i < config.column_count; i++) {
        LOG_D("Column %d: %d\n", i, config.column_sizes[i]);
    }
}

static void output_cells(size_t cells_found, bool last_full) {
    Cell* current_cell = _cells;
    Cell* cell_end = _cells + cells_found;
    int column_idx = 0;

    while (current_cell < cell_end) {
        if (current_cell->start == NULL) {
            fprintf(stdout, "%.*s", (int)config.newline_length, config.newline);
            column_idx = 0;
        }
        else {
            if (current_cell->length == 0) {
                fprintf(stdout, "%-*s ", column_idx, "");
            }
            else {
                fprintf(stdout, "%.*s", (int)current_cell->length, current_cell->start);
                if ((int)current_cell->length < config.column_sizes[column_idx]) {
                    //LOG_D("Attempting to pad col %d with %d\n", column_idx, config.column_sizes[column_idx] - (int)current_cell->length);
                    fprintf(stdout, "%*s", config.column_sizes[column_idx] - (int)current_cell->length, "");
                }
                fprintf(stdout, " ");
            }
            column_idx++;
        }
        current_cell++;
    }
    if (last_full) {
        /* TODO - print message for actual max vs detected */
    }
}
