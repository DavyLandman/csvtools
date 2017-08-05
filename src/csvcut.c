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
    FILE* target;

    char separator;
    char newline[2];
    size_t newline_length;

    bool* keep;
    int column_count;
    int first_cell;
} config;


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
            if (first == true) {
                first = false;
                finish_config(cells_found);
            }

            LOG_D("Processed: %zu, Cells: %zu\n", buffer_consumed, cells_found);
            debug_cells(cells_found);
            output_cells(cells_found, last_full);
        }
    }
    if (_tokenizer != NULL) {
        free_tokenizer(_tokenizer);
    }
    if (config.keep != NULL) {
        free(config.keep);
    }
    if (config.source != stdin) {
        fclose(config.source);
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
            char* s = calloc(sizeof(char), current_cell->length + 1);
            s[current_cell->length] = '\0';
            memcpy(s, current_cell->start, current_cell->length);
            LOG_V("Cell %zu : %s\n", (size_t)(current_cell - _cells), s);
            free(s);
        }
        current_cell++;
    }
#else
    (void)total;
#endif
}

static void print_help() {
    fprintf(stderr,"usage: csvcut [OPTIONS] [FILE]");
    fprintf(stderr,"options:");
    fprintf(stderr, "-s ,\n");
    fprintf(stderr, "\tWhich character to use as separator (default is ,)\n");
    fprintf(stderr, "-k column,names,to,keep\n");
    fprintf(stderr, "-d column,names,to,drop\n");
    fprintf(stderr, "-K 0,1,3\n");
    fprintf(stderr, "\tWhich columns to keep\n");
    fprintf(stderr, "-D 0,1,3\n");
    fprintf(stderr, "\tWhich columns to drop\n");
    fprintf(stderr, "-e\n");
    fprintf(stderr, "\tProvide column names one at a time, useful in case of embedded commas.\n");
}

enum column_kind {
    NONE,
    KEEP_NAMES,
    KEEP_INDEXES,
    DROP_NAMES,
    DROP_INDEXES
};

static struct {
    enum column_kind kind;
    size_t cuts_defined;
    const char** cuts;
} preconfig;

static void parse_config(int argc, char** argv) {
    config.separator = ',';
    config.source = stdin;

    
    preconfig.kind = NONE;
    preconfig.cuts_defined = 0;
    preconfig.cuts = NULL;

    bool one_at_a_time =  false;
    char c;
    while ((c = getopt (argc, argv, "s:k:d:K:D:eh")) != -1) {
        switch (c) {
            case 'e':
                one_at_a_time = true;
                break;
            case 's': 
                config.separator = optarg[0];
                break;
            case 'k':
            case 'd':
            case 'K':
            case 'D':
                if (!one_at_a_time) {
                    if (preconfig.kind != NONE) {
                        fprintf(stderr, "Error, you can only pass one kind of cut option.\n");
                        exit(1);
                    }
                    preconfig.cuts = malloc(sizeof(char*));
                    preconfig.cuts_defined = 1;
                    preconfig.cuts[0] = strtok(optarg, ",");
                    char* next_column;
                    while ((next_column = strtok(NULL, ",")) != NULL) {
                        preconfig.cuts_defined++;
                        preconfig.cuts = realloc(preconfig.cuts, sizeof(char*) * preconfig.cuts_defined);
                        preconfig.cuts[preconfig.cuts_defined - 1] = next_column;
                    }
                }
                else {
                    if (!preconfig.cuts) {
                        preconfig.cuts = malloc(sizeof(char*));
                        preconfig.cuts_defined = 1;
                    }
                    else {
                        preconfig.cuts_defined++;
                        preconfig.cuts = realloc(preconfig.cuts, sizeof(char*) * preconfig.cuts_defined);
                    }
                    preconfig.cuts[preconfig.cuts_defined - 1] = optarg;
                }
                if (c == 'k') {
                    if (preconfig.kind != NONE && preconfig.kind != KEEP_NAMES) {
                        fprintf(stderr, "You can only choose one mode of dropping/keeping columns\n");
                        print_help();
                        exit(1);
                    }
                    preconfig.kind = KEEP_NAMES;
                }
                else if (c == 'd') {
                    if (preconfig.kind != NONE && preconfig.kind != DROP_NAMES) {
                        fprintf(stderr, "You can only choose one mode of dropping/keeping columns\n");
                        print_help();
                        exit(1);
                    }
                    preconfig.kind = DROP_NAMES;
                }
                else if (c == 'K') {
                    if (preconfig.kind != NONE && preconfig.kind != KEEP_INDEXES) {
                        fprintf(stderr, "You can only choose one mode of dropping/keeping columns\n");
                        print_help();
                        exit(1);
                    }
                    preconfig.kind = KEEP_INDEXES;
                }
                else if (c == 'D') {
                    if (preconfig.kind != NONE && preconfig.kind != DROP_INDEXES) {
                        fprintf(stderr, "You can only choose one mode of dropping/keeping columns\n");
                        print_help();
                        exit(1);
                    }
                    preconfig.kind = DROP_INDEXES;
                }
                break;
            case '?':
            case 'h':
                print_help();
                exit(1);
                break;
        }
    }

    if (preconfig.kind == NONE) {
        fprintf(stderr, "You should describe how you want to cut the csv\n");
        print_help();
        exit(1);
    }

    if (optind < argc) {
        config.source = fopen(argv[optind], "r");
        if (!config.source) {
            fprintf(stderr, "Could not open file %s for reading\n", argv[optind]);
            exit(1);
        }
    }

    LOG_D("%s\n","Done parsing config params");    

    _tokenizer = setup_tokenizer(config.separator, _buffer, _cells,CELL_BUFFER_SIZE);
}

static char _unquote_buffer[BUFFER_SIZE];
static char const * unquote(char const* restrict quoted, size_t* restrict length) {
    char * restrict result = _unquote_buffer;
    char const * restrict current_char = quoted; 
    char const * restrict char_end = quoted + *length; 
    while (current_char < char_end) {
        if (*current_char == '"') {
            // must be an escaped "
            current_char++;
            (*length)--;
        }
        *result++ = *current_char++;
    }
    return _unquote_buffer;
}

bool str_contains_n(size_t amount, const char** strings, const char* needle, size_t needle_size) {
    if (*needle == '"') {
        needle++;
        needle_size -= 2;
        needle = unquote(needle, &needle_size);
    }
    for (size_t i = 0; i < amount; i++) {
        if (strlen(strings[i]) == needle_size && strncasecmp(strings[i], needle, needle_size) == 0) {
            return true;
        }
    }
    return false;
}

static void finish_config(size_t cells_found) {
    debug_cells(cells_found);

    Cell const* current_cell = _cells;
    while (current_cell < (_cells + cells_found) && current_cell->start != NULL) {
        current_cell++;
    }
    config.column_count = (int)(current_cell - _cells);

    const char* new_line = _cells[config.column_count-1].start + _cells[config.column_count - 1].length;
    config.newline[0] = new_line[0];
    config.newline_length = 1;
    if (new_line[1] == '\n' || new_line[0] == '\r') {
        config.newline[1] = '\n';
        config.newline_length = 2;
    }

    config.keep = calloc(sizeof(bool), config.column_count);
    for (int c = 0; c < config.column_count; c++) {
        config.keep[c] =  false;
    }

    if (preconfig.kind == KEEP_NAMES || preconfig.kind == DROP_NAMES) {
        for (int c = 0; c < config.column_count; c++) {
            bool cond = str_contains_n(preconfig.cuts_defined, preconfig.cuts, _cells[c].start, _cells[c].length);
            if ((cond && (preconfig.kind == KEEP_NAMES)) || (!cond && (preconfig.kind == DROP_NAMES))) {
                config.keep[c] = true;
            }
        }
    }
    else if (preconfig.kind == KEEP_INDEXES || preconfig.kind == DROP_INDEXES) {
        for (int c = 0; c < config.column_count; c++) {
            char str_index[15];
            int str_length = sprintf(str_index, "%d", c);
            bool cond = str_contains_n(preconfig.cuts_defined, preconfig.cuts, str_index, str_length);
            if ((cond && (preconfig.kind == KEEP_INDEXES)) || (!cond && (preconfig.kind == DROP_INDEXES))) {
                config.keep[c] = true;
            }
        }
    }
    else {
        assert(false);
    }
    free(preconfig.cuts);
    for (int c = 0; c < config.column_count; c++) {
        if (config.keep[c]) {
            config.first_cell = c;
            break;
        }
    }
}

static bool _half_printed = false;
static int _current_cell_id = 0;

static void output_cells(size_t cells_found, bool last_full) {
    LOG_D("Starting output: %zu (%d)\n", cells_found, last_full);
    LOG_V("Entry: current_cell: %d\n", _current_cell_id);
    Cell const * restrict current_cell = _cells;
    Cell const * restrict cell_end = _cells + cells_found;

    char const * restrict current_chunk_start = current_cell->start;
    size_t current_chunk_length = 0;
    int current_chunk_start_id = _current_cell_id;

    while (current_cell < cell_end) {
        //LOG_D("Current cell: %d %p\n", _current_cell_id,current_cell->start);
        if (current_cell->start == NULL) {
            if (_current_cell_id < config.column_count) {
                fprintf(stderr, "Not enough cells in this row, expect: %d, got: %d (cell %zu)\n", config.column_count, _current_cell_id,  (size_t)(current_cell - _cells));
                exit(1);
                return;
            }
            if (current_chunk_start != NULL) {
                current_chunk_length--; // take away newline 
                if (current_chunk_start != _buffer || !_half_printed) {
                    if (current_chunk_start_id != config.first_cell) {
                        fwrite(&(config.separator),sizeof(char),1, stdout);
                    }
                }
                fwrite(current_chunk_start, sizeof(char), current_chunk_length, stdout);
            }
            fwrite(config.newline, sizeof(char), config.newline_length, stdout);
            current_chunk_start = (current_cell + 1)->start;
            current_chunk_length = 0;
            current_chunk_start_id = 0;
            _current_cell_id = -1;
        }
        if (_current_cell_id >= config.column_count) {
            fprintf(stderr, "Too many cells in this row, expect: %d, got: %d (cell: %zu)\n", config.column_count, _current_cell_id, (size_t)(current_cell - _cells));
            exit(1);
            return;
        }
        else if (config.keep[_current_cell_id]) {
            current_chunk_length += 1 + current_cell->length;
        }
        else {
            // a column to drop, so lets write the previous chunk
            if (_current_cell_id >= config.first_cell && current_chunk_length > 0) {
                current_chunk_length--; // take away last seperator
                if (current_chunk_start != _buffer || !_half_printed) {
                    if (current_chunk_start_id != config.first_cell) {
                        fwrite(&(config.separator),sizeof(char),1, stdout);
                    }
                }
                fwrite(current_chunk_start, sizeof(char), current_chunk_length, stdout);
            }
            // begining of the line, nothing happening
            current_chunk_start = (current_cell + 1)->start;
            current_chunk_length = 0;
            current_chunk_start_id = _current_cell_id + 1;
        }

        _current_cell_id++;
        current_cell++;
    }
    if (current_chunk_length > 0) {
        current_chunk_length--; // fix of by one error 
        if (current_chunk_start != _buffer || !_half_printed) {
            if (current_chunk_start_id != config.first_cell) {
                fwrite(&(config.separator),sizeof(char),1, stdout);
            }
        }
        fwrite(current_chunk_start, sizeof(char), current_chunk_length, stdout);
    }
    if (!last_full) {

        _half_printed = true;
        _current_cell_id--;
    }
    else {
        _half_printed = false;
    }
    LOG_V("Exit: current_cell: %d\n", _current_cell_id);
}
