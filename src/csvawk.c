#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>     
#include <stdbool.h>
#include <assert.h>
#include <string.h>
#include "debug.h"
#include "hints.h"

#define AWK_ROW_SEPARATOR '\x1E'
#define AWK_CELL_SEPARATOR '\x1F'

#if defined(_GNU_SOURCE) && !defined(SLOW_PATH)
    #define FAST_GNU_LIBC
#endif

static char _buffer[BUFFER_SIZE + 2];

static struct {
    FILE* source;
    char separator;
    bool drop_header;
#ifdef FAST_GNU_LIBC
    char scan_mask[4];
#endif
    char* script;
    FILE* target;
} config;

static void start_awk();
static void parse_config(int argc, char** argv);
static void do_pipe(size_t chars_read);
int main(int argc, char** argv) {
    parse_config(argc, argv);

    if (config.target != stdout) {
        start_awk();
    }

    size_t chars_read;
    SEQUENTIAL_HINT(config.source);
    while ((chars_read = fread(_buffer, sizeof(char), BUFFER_SIZE, config.source)) > 0) {
        _buffer[chars_read] = '\0';
        _buffer[chars_read+1] = '\"';
        do_pipe(chars_read);
    }
    if (config.source != stdin) {
        fclose(config.source);
    }
    if (config.target != stdout) {
        pclose(config.target);
    }
    return 0;
}

static void print_help() {
    fprintf(stderr,"usage: csvawk [OPTIONS] AWKSCRIPT [FILE]");
    fprintf(stderr,"options:");
    fprintf(stderr, "-s ,\n");
    fprintf(stderr, "  Which character to use as separator (default is ,)\n");
    fprintf(stderr, "-d\n");
    fprintf(stderr, "  drop header row\n");
}

static void parse_config(int argc, char** argv) {
    config.source = stdin;
    config.separator = ',';
    config.drop_header = false;

    char c;
    while ((c = getopt (argc, argv, "s:dp")) != -1) {
        switch (c) {
            case 's': 
                if (strcmp(optarg, "\\t") == 0)
                    config.separator = '\t';
                else
                    config.separator = optarg[0];
                break;
            case 'd':
                config.drop_header = true;
                break;
            case 'p':
                config.target = stdout;
                break;
            case '?':
            case 'h':
                print_help();
                exit(1);
                break;
        }
    }
    int args_left = argc - optind;
    switch(args_left) {
        case 0:
            fprintf(stderr, "Missing AWK script\n");
            print_help();
            exit(1);
            break;
        case 2:
            config.source = fopen(argv[argc - 1], "r");
            if (!config.source) {
                fprintf(stderr, "Could not open file %s for reading\n", argv[optind]);
                exit(1);
            }
            // fall through
        case 1:
            config.script = argv[optind];
            break;
        default:
            if (args_left > 2) {
                fprintf(stderr, "Too many arguments\n");
            }
            else {
                fprintf(stderr, "Missing AWK script\n");
            }
            print_help();
            exit(1);
            break;
    }

#ifdef FAST_GNU_LIBC
    config.scan_mask[0] = '\r';
    config.scan_mask[1] = '\n';
    config.scan_mask[2] = config.separator;
    config.scan_mask[3] = '\"';
#endif

    LOG_D("%s\n","Done parsing config params");    
}

enum tokenizer_state {
    FRESH,
    PREV_NEWLINE,
    PREV_QUOTE,
    IN_QUOTE,
};

static bool first_run = true;
static enum tokenizer_state _state = FRESH;

static void do_pipe(size_t chars_read) {
    char* restrict current_char = _buffer;
    char const* restrict char_end = _buffer + chars_read;
    char const* restrict current_start = _buffer;
    LOG_V("Piping: %zu state: %d first char: %c\n", chars_read, _state, *current_char);

#ifndef FAST_GNU_LIBC
    assert(CHAR_BIT == 8);
    bool cell_delimitor[256];
    memset(cell_delimitor, false, sizeof(bool) * 256);
    cell_delimitor[(unsigned char)config.separator] = true;
    cell_delimitor[(unsigned char)'\n'] = true;
    cell_delimitor[(unsigned char)'\r'] = true;
    cell_delimitor[(unsigned char)'\"'] = true;
#endif

    if (config.drop_header && first_run) {
        while (current_char < char_end) {
            if (*current_char == '\n' || *current_char == '\r') {
                if (*current_char == '\r') {
                    _state = PREV_NEWLINE; // handle the windows newlines correctly
                }
                current_start = ++current_char;
                first_run = false;
                break;
            }
            current_char++;
        }
        if (current_char == char_end) {
            return;
        }
    }

    switch(_state) {
        case PREV_QUOTE:
            _state = FRESH; // reset state
            if (*current_char == '"') {
                // we have two quotes
                // one in the previous block, one in the current
                goto IN_QUOTE;
            }
            // we were at the end of the quoted cell, so let's continue
            break;
        case IN_QUOTE:
            current_char--; // the loop starts with a increment
            goto IN_QUOTE;
        case PREV_NEWLINE:
            if (*current_char == '\n') {
                // we already had a newline, so lets eat this second windows
                // newline
                current_char++;
                current_start++;
            }
            _state = FRESH;
            break;
        default:
            break;
    }

    while (current_char < char_end) {
        if (*current_char == '"') {
IN_QUOTE:
            while (++current_char < char_end) {
                if (*current_char == '"') {
                    char const* peek = current_char + 1;
                    if (peek == char_end) {
                        current_char++;
                        _state = PREV_QUOTE;
                        // at the end of stream and not sure if escaped or not
                        break;
                    }
                    else if (*peek == '"') {
                        current_char++;
                        continue;
                    }
                    else {
                        break;
                    }
                }
            }
            if (current_char == char_end) {
                // we are at the end, let's write everything we've seen
                if (_state != PREV_QUOTE) {
                    _state = IN_QUOTE;
                }
                break;
            }
            else {
                current_char++;
                _state = FRESH;
            }
        }
        else if (*current_char == '\n') {
            *current_char = AWK_ROW_SEPARATOR;
            current_char++;
        }
        else if (*current_char == '\r') {
            *current_char = AWK_ROW_SEPARATOR;
            current_char++;
            if (current_char == char_end) {
                _state = PREV_NEWLINE;
                break;
            }
            else if (*current_char == '\n') {
                // we have windows new lines, so lets skip over this byte
                fwrite(current_start, sizeof(char), current_char - current_start, config.target);
                current_char++;
                current_start = current_char;
            }
        }
        else if (*current_char == config.separator) {
            *current_char = AWK_CELL_SEPARATOR;
            current_char++;
        }
        else {
            // all other chars, just skip until we find another interesting character
#ifdef FAST_GNU_LIBC
            do {
                current_char++;
                current_char += strcspn(current_char, config.scan_mask);
            } while (current_char < char_end && *current_char == '\0'); // strspn stops at 0 chars.
#else
            while (true) {
                if (cell_delimitor[(unsigned char)current_char[1]]) {
                    current_char += 1;
                    goto FOUND_CELL_END;
                }
                if (cell_delimitor[(unsigned char)current_char[2]]) {
                    current_char += 2;
                    goto FOUND_CELL_END;
                }
                if (cell_delimitor[(unsigned char)current_char[3]]) {
                    current_char += 3;
                    goto FOUND_CELL_END;
                }
                current_char += 4;
                if (cell_delimitor[(unsigned char)current_char[0]]) {
                    goto FOUND_CELL_END;
                }
            }
FOUND_CELL_END:;
#endif
            while (current_char > char_end) {
                // we added a \0 past the end just to detect the end, so let's revert to the actual end
                current_char--;
            }
        }
    }
    if (current_start < char_end) {
        fwrite(current_start, sizeof(char), char_end - current_start, config.target);
    }
    fflush(config.target);
}

void start_awk() {
    char* prefix = "awk \'BEGIN{ FS=\"\\x1F\"; RS=\"\\x1E\" } ";
    char* command = calloc(strlen(prefix) + strlen(config.script) + 2, sizeof(char));
    sprintf(command, "%s %s\'", prefix, config.script);
    config.target = popen(command, "w");
    if (!config.target) {
        fprintf(stderr, "Can't start \"%s\"\n", command);
        exit(1);
    }
    free(command);
}

