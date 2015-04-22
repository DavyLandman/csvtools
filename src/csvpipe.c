#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <assert.h>
#include "debug.h"
#include "hints.h"


#define NULL_ENCODED '\x1a'

//#define BUFFER_SIZE 3
static char _buffer[BUFFER_SIZE];

struct {
	FILE* source;
	bool drop_header;
} config;

static void parse_config(int argc, char** argv);
static void do_pipe(size_t chars_read);

int main(int argc, char** argv) {
	parse_config(argc, argv);

	size_t chars_read;
	SEQUENTIAL_HINT(config.source);
	while ((chars_read = fread(_buffer, sizeof(char), BUFFER_SIZE, config.source)) > 0) {
		do_pipe(chars_read);
	}
	return 0;
}

static void print_help() {
	fprintf(stderr, "usage: csvpipe [OPTIONS] [FILE]");
	fprintf(stderr, "options:");
	fprintf(stderr, "-d\n");
	fprintf(stderr, "  drop header row\n");
}

static void parse_config(int argc, char** argv) {
	config.source = stdin;
	config.drop_header = false;
	char c;
	while ((c = getopt (argc, argv, "d")) != -1) {
		switch (c) {
			case 'd':
				config.drop_header = true;
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
		config.source = fopen(argv[optind], "r");
		if (!config.source) {
			fprintf(stderr, "Could not open file %s for reading\n", argv[optind]);
			exit(1);
		}
	}
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
				else if (*current_char == '\0') {
					*current_char = NULL_ENCODED;
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
			*current_char = '\0';
			current_char++;
		}
		else if (*current_char == '\r') {
			*current_char = '\0';
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
		else if (*current_char == '\0') {
			*current_char = NULL_ENCODED;
			current_char++;
		}
		else {
			// all other chars, just skip one
			current_char++;
		}
	}
	if (current_start < char_end) {
		fwrite(current_start, sizeof(char), char_end - current_start, stdout);
	}
}

