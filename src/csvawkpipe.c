#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>     
#include <stdbool.h>
#include <assert.h>
#include <string.h>
#include "debug.h"

#define AWK_ROW_SEPARATOR '\x1E'
#define AWK_CELL_SEPARATOR '\x1F'


static char _buffer[BUFFER_SIZE];

static struct {
	FILE* source;
	char separator;
	bool drop_header;
} config;

static void parse_config(int argc, char** argv);
static void do_pipe(size_t chars_read);
int main(int argc, char** argv) {
	parse_config(argc, argv);

	size_t chars_read;
	while ((chars_read = fread(_buffer, sizeof(char), BUFFER_SIZE, config.source)) > 0) {
		do_pipe(chars_read);
	}
	if (config.source != stdin) {
		fclose(config.source);
	}
	return 0;
}

static void print_help() {
	fprintf(stderr,"usage: csvawkpipe [OPTIONS] [FILE]");
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
	while ((c = getopt (argc, argv, "s:d")) != -1) {
		switch (c) {
			case 's': 
				config.separator = optarg[0];
				break;
			case 'd':
				config.drop_header = true;
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
				fwrite(current_start, sizeof(char), current_char - current_start, stdout);
				current_char++;
				current_start = current_char;
			}
		}
		else if (*current_char == config.separator) {
			*current_char = AWK_CELL_SEPARATOR;
			current_char++;
		}
		else {
			// all other chars, just skip untill we find another
			while (++current_char < char_end && *current_char != '"' && *current_char != '\r' && *current_char != '\n' && *current_char != config.separator);
		}
	}
	if (current_start < char_end) {
		fwrite(current_start, sizeof(char), char_end - current_start, stdout);
	}
}

