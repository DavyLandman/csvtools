#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <assert.h>
#include <string.h>
#include <pcre.h> 
#include "csv_tokenizer.h"
#include "debug.h"
#include "hints.h"
#ifndef PCRE_STUDY_JIT_COMPILE
#define PCRE_STUDY_JIT_COMPILE 0
#endif

#ifndef PCRE_CONFIG_JIT
#define pcre_free_study pcre_free
#endif



//#define BUFFER_SIZE 30
#define CELL_BUFFER_SIZE (BUFFER_SIZE / 2) + 2

typedef struct {
    pcre const* restrict pattern;
    pcre_extra const* restrict extra;
} Regex;

struct csv_tokenizer* _tokenizer;
static char _buffer[BUFFER_SIZE + BUFFER_TOKENIZER_POSTFIX];
static Cell _cells[CELL_BUFFER_SIZE];

static int _have_jit = 0;

static struct {
    FILE* source;
    char separator;
    char newline[2];
    size_t newline_length;

    bool count_only;
    bool negative;
    bool or;
    bool case_insensitive;
    Regex* patterns;

    int column_count;
} config;

static long long _count;

static char const * unquote(char const* restrict quoted, size_t* restrict length);
static void parse_config(int argc, char** argv);
static size_t finish_config(size_t cells_found);

static void output_cells(size_t cells_found, size_t offset, bool last_full);
static void debug_cells(size_t total);

int main(int argc, char** argv) {

    parse_config(argc, argv);
#ifdef PCRE_CONFIG_JIT
    pcre_config(PCRE_CONFIG_JIT, &_have_jit);
#else
    _have_jit = false;
#endif
    if (!_have_jit) {
        fprintf(stderr, "I am running without PCRE-JIT support, expect less performance.\n");
    }


    size_t chars_read;
    bool first = true;
    SEQUENTIAL_HINT(config.source);
    while ((chars_read = fread(_buffer, 1, BUFFER_SIZE, config.source)) > 0) {
        LOG_D("New data read: %zu\n", chars_read);
        prepare_tokenization(_tokenizer, _buffer, chars_read);
        size_t buffer_consumed = 0;
        size_t cells_found = 0;
        bool last_full = true;


        while (buffer_consumed < chars_read) {
            tokenize_cells(_tokenizer, buffer_consumed, chars_read, &buffer_consumed, &cells_found, &last_full);
            LOG_D("Processed: %zu, Cells: %zu\n", buffer_consumed, cells_found);
            debug_cells(cells_found);

            size_t cell_offset = 0;
            if (first) {
                first = false;
                cell_offset = finish_config(cells_found);
            }
            output_cells(cells_found, cell_offset, last_full);
        }
    }
    if (config.count_only) {
        fprintf(stdout, "%llu\n", _count);
    }
    if (_tokenizer != NULL) {
        free_tokenizer(_tokenizer);
    }
    if (config.patterns != NULL) {
        for (int c = 0; c < config.column_count; c++) {
            pcre_free_study((pcre_extra*)(config.patterns[c].extra));
            pcre_free((pcre*)(config.patterns[c].pattern));
        }
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
    fprintf(stderr,"usage: csvgrep [OPTIONS] [FILE]");
    fprintf(stderr,"options:");
    fprintf(stderr, "-s ,\n");
    fprintf(stderr, "\tWhich character to use as separator (default is ,)\n");
    fprintf(stderr, "-p column/pattern/\n");
    fprintf(stderr, "\tMultiple -p are allowed, they work as an AND \n");
    fprintf(stderr, "-i\n");
    fprintf(stderr, "\tuse case insensitive matching\n");
    fprintf(stderr, "-c\n");
    fprintf(stderr, "\tOnly count the rows that match\n");
    fprintf(stderr, "-o\n");
    fprintf(stderr, "\tMake the match into an OR, changes the behavior of -p and -v\n");
    fprintf(stderr, "-v\n");
    fprintf(stderr, "\tPrint only the rows that did not match all patterns\n");
}

static struct {
    size_t n_patterns;
    char ** columns;
    char ** patterns;
    size_t * column_lengths;
} half_config;

static void parse_config(int argc, char** argv) {
    config.source = stdin;
    config.separator = ',';
    config.count_only = false;
    config.negative = false;
    config.case_insensitive = false;
    config.or = false;

    half_config.n_patterns = 0;
    half_config.columns = malloc(sizeof(char*));
    half_config.patterns = malloc(sizeof(char*));
    half_config.column_lengths = malloc(sizeof(size_t));

    char c;
    while ((c = getopt (argc, argv, "s:p:cvio")) != -1) {
        switch (c) {
            case 's': 
                config.separator = optarg[0];
                break;
            case 'c': 
                config.count_only = true;
                break;
            case 'i':
                config.case_insensitive = true;
                break;
            case 'v':
                config.negative = true;
                break;
            case 'o':
                config.or = true;
                break;
            case 'p':
                LOG_V("Got pattern: %s\n", optarg);
                char* column_name = strtok(optarg, "/"); 
                char* column_pattern = strtok(NULL, "/");
                for (size_t pat = 0;  pat < half_config.n_patterns; pat++) {
                    if (strcasecmp(column_name, half_config.columns[pat]) == 0) {
                        fprintf(stderr, "You can only define one pattern per column (column: %s)\n", column_name);
                        exit(1);
                    }
                }
                half_config.n_patterns++;
                if (half_config.n_patterns >= 1) {
                    half_config.columns = realloc(half_config.columns, sizeof(char*) * half_config.n_patterns);
                    half_config.patterns = realloc(half_config.patterns, sizeof(char*) * half_config.n_patterns);
                    half_config.column_lengths = realloc(half_config.column_lengths, sizeof(size_t) * half_config.n_patterns);
                }
                half_config.columns[half_config.n_patterns - 1] = column_name;
                half_config.patterns[half_config.n_patterns - 1] = column_pattern;
                half_config.column_lengths[half_config.n_patterns - 1] = strlen(column_name);
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

    if (half_config.n_patterns == 0) {
        fprintf(stderr, "You should at least provide one pattern\n");
        print_help();
        exit(1);
    }

    LOG_D("%s\n","Done parsing config params");    

    _tokenizer = setup_tokenizer(config.separator, _buffer, _cells, CELL_BUFFER_SIZE);

}

static size_t finish_config(size_t cells_found) {

    Cell* current_cell = _cells;
    while (current_cell < (_cells + cells_found) && current_cell->start != NULL) {
        if (!config.count_only) {
            // also immediatly print the header
            if (current_cell != _cells) {
                fwrite(&(config.separator),sizeof(char),1, stdout);
            }
            fwrite(current_cell->start, sizeof(char), current_cell->length, stdout);
        }
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
    if (!config.count_only) {
        fwrite(config.newline, sizeof(char), config.newline_length, stdout);
    }

    bool* used = calloc(sizeof(bool), half_config.n_patterns);
    memset(used, 0, sizeof(bool) * half_config.n_patterns);
    config.patterns = calloc(sizeof(Regex),config.column_count);
    memset(config.patterns, 0, sizeof(Regex) * config.column_count);
    for (int c = 0; c < config.column_count; c++) {
        const char* column = _cells[c].start;
        size_t length = _cells[c].length;
        if (*column == '"') {
            column++;
            length -= 2;
            column = unquote(column, &length);
        }
        for (size_t pat = 0;  pat < half_config.n_patterns; pat++) {
            if (!used[pat] && length == half_config.column_lengths[pat]) {
                if (strncasecmp(column, half_config.columns[pat], half_config.column_lengths[pat])==0) {
                    used[pat] = true;
                    LOG_V("Adding pattern %s for column: %s (%d)\n", half_config.patterns[pat], half_config.columns[pat],c);
                    // we have found the column
                    const char *pcreErrorStr;
                    int pcreErrorOffset;
                    config.patterns[c].pattern = pcre_compile(half_config.patterns[pat], PCRE_DOLLAR_ENDONLY |  PCRE_DOTALL | PCRE_NO_UTF8_CHECK | (config.case_insensitive ? PCRE_CASELESS : 0), &pcreErrorStr, &pcreErrorOffset, NULL); 
                    if(config.patterns[c].pattern == NULL) {
                        fprintf(stderr, "ERROR: Could not compile '%s': %s\n", half_config.patterns[pat], pcreErrorStr);
                        exit(1);
                    }
                    config.patterns[c].extra = pcre_study(config.patterns[c].pattern, _have_jit ? PCRE_STUDY_JIT_COMPILE : 0, &pcreErrorStr);
                    if(config.patterns[c].extra == NULL) {
                        fprintf(stderr, "ERROR: Could not study '%s': %s\n", half_config.patterns[pat], pcreErrorStr);
                        exit(1);
                    }
                    break;
                }
            }
        }
    }

    bool stop = false;
    for (size_t pat = 0;  pat < half_config.n_patterns; pat++) {
        if (!used[pat]) {
            fprintf(stderr, "ERROR: The column \"%s\" was not found in the header\n", half_config.columns[pat]);
            stop = true;
        }
    }
    if (stop) {
        fprintf(stderr, "Exiting\n");
        exit(1);
    }

    free(used);
    free(half_config.columns);
    free(half_config.patterns);
    free(half_config.column_lengths);

    return config.column_count + 1 ;
}


// data for around the edges
static char _prev_line[BUFFER_SIZE * 2];
static size_t _prev_line_length = 0;
static char _prev_cell[BUFFER_SIZE];
static size_t _prev_cell_length = 0;

// state of the output
static int _current_cell_id = 0;
static bool _half_line = false;
static bool _half_cell = false;
static bool _prev_matches = true;

static void output_cells(size_t cells_found, size_t offset, bool last_full) {
    LOG_D("Starting output: %zu (%d)\n", cells_found, last_full);
    LOG_V("Entry: current_cell: %d\n", _current_cell_id);

    Cell const* restrict current_cell = _cells + offset;
    Cell const* restrict cells_end = _cells + cells_found;

    bool matches = !config.or;
    if (_half_line) {
        matches = _prev_matches;
    }
    char const* restrict current_line_start = current_cell->start;
    size_t current_line_length = 0;

    while (current_cell < cells_end) {
        if (_current_cell_id > config.column_count) {
            fprintf(stderr, "Too many cells in this row, expect: %d, got: %d (cell: %zu)\n", config.column_count, _current_cell_id, (size_t)(current_cell - _cells));
            exit(1);
            return;
        }
        if (current_cell->start == NULL) {
            if (_current_cell_id == config.column_count) {
                // end of the line
                if (matches) {
                    if (config.count_only) {
                        _half_line = false;
                        _prev_line_length = 0;
                        _count++;
                    }
                    else {
                        if (_half_line) {
                            LOG_V("Printed previous half line %zu\n", _prev_line_length);
                            fwrite(_prev_line, sizeof(char), _prev_line_length, stdout);
                            _half_line = false;
                            _prev_line_length = 0;
                        }
                        fwrite(current_line_start, sizeof(char), current_line_length, stdout);
                        fwrite(config.newline, sizeof(char), config.newline_length, stdout);
                    }
                }
                else if (_half_line) {
                    // we stored the previos part of this line, but it can be dropped
                    _half_line = false;  
                    _prev_line_length = 0;
                }
                current_line_start = (current_cell + 1)->start;
                current_line_length = 0;
                _current_cell_id = -1;
                matches = !config.or;
            }
            else if (_current_cell_id < config.column_count) {
                fprintf(stderr, "Not enough cells in this row, expect: %d, got: %d (cell %zu)\n", config.column_count, _current_cell_id,  (size_t)(current_cell - _cells));
                exit(1);
                return;
            }
        }
        else if (matches || config.or) { // only if we have a match does it make sense to test other cells
            current_line_length += 1 + current_cell->length;
            if (_current_cell_id == 0 || current_cell == (_cells + offset)) {
                current_line_length--; // the first doesn't have a separator
            }
            if (config.patterns[_current_cell_id].pattern != NULL) {
                char const* restrict cell = current_cell->start;
                size_t length = current_cell->length;
                if (current_cell == (cells_end-1) && !last_full) {
                    // we do not have the full cell at the moment, let's copy it
                    size_t old_cell_length = _prev_cell_length;
                    _prev_cell_length += current_cell->length;
                    memcpy(_prev_cell + old_cell_length, current_cell->start, sizeof(char) * current_cell->length);
                    _half_cell = true;
                    _current_cell_id++;
                    break;
                }
                if (_half_cell && current_cell == _cells) {
                    // append the current cell to the back of the previous one.
                    assert(_prev_cell_length + length < BUFFER_SIZE);
                    memcpy(_prev_cell + _prev_cell_length, cell, sizeof(char) * length);
                    cell = _prev_cell;
                    length +=  _prev_cell_length;
                    _prev_cell_length = 0;
                }
                if (length > 1 && cell[0] == '"') {
                    cell++;
                    length -= 2;
                    char const* restrict c = cell-1;
                    char const* restrict cell_end = cell + length;
                    while (++c < cell_end && *c != '"');
                    if (c != cell_end) {
                        // we have nested quotes
                        cell = unquote(cell, &length);
                    }
                }
                int ovector[255];
                int matchResult = pcre_exec(config.patterns[_current_cell_id].pattern, config.patterns[_current_cell_id].extra, cell, length, 0, 0, ovector, 255);
                if (config.or) {
                    matches |= (matchResult >= 0) ^ config.negative;
                }
                else {
                    matches &= (matchResult >= 0) ^ config.negative;
                }
#ifdef MOREDEBUG
                if (matchResult < 0) {
                    fprintf(stderr, "tried to match :'");
                    fwrite(cell, sizeof(char), length, stderr);
                    fprintf(stderr, "'\n");
                    switch(matchResult) {
                        case PCRE_ERROR_NOMATCH      : fprintf(stderr,"String did not match the pattern\n");        break;
                        case PCRE_ERROR_NULL         : fprintf(stderr,"Something was null\n");                      break;
                        case PCRE_ERROR_BADOPTION    : fprintf(stderr,"A bad option was passed\n");                 break;
                        case PCRE_ERROR_BADMAGIC     : fprintf(stderr,"Magic number bad (compiled re corrupt?)\n"); break;
                        case PCRE_ERROR_UNKNOWN_NODE : fprintf(stderr,"Something kooky in the compiled re\n");      break;
                        case PCRE_ERROR_NOMEMORY     : fprintf(stderr,"Ran out of memory\n");                       break;
                        default                      : fprintf(stderr,"Unknown error\n");                           break;
                    }
                }
#endif
            }
        }

        _current_cell_id++;
        current_cell++;
    }
    if (_current_cell_id != 0) {
        // the last row wasn't completly printed, so we must be inside a row
        _prev_matches = matches;
        if (_prev_matches) {
            // it could still match, so let's copy the line
            size_t old_line_length = _prev_line_length;
            _prev_line_length += current_line_length;
            assert(_prev_line_length < (BUFFER_SIZE * 2));
            memcpy(_prev_line + old_line_length, current_line_start, sizeof(char) * current_line_length);
            if (last_full && _current_cell_id != config.column_count) { // the , gets eaten away
                _prev_line[_prev_line_length++] = config.separator;
            }
#ifdef MOREDEBUG
            fprintf(stderr, "current prev line :'");
            fwrite(_prev_line, sizeof(char), _prev_line_length, stderr);
            fprintf(stderr, "'\n");
#endif
        }
        _half_line = true;
        if (!last_full) {
            _current_cell_id--;
        }
    }
    else {
        _half_line = false;
    }
    LOG_V("Exit: current_cell: %d\n", _current_cell_id);
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
