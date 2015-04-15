#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <assert.h>
#include "debug.h"


#define BUFFER_SIZE 1024*1024
//#define BUFFER_SIZE 3


static char _buffer[BUFFER_SIZE];

enum PipingMode {
	NEWLINE = (1 << 0),
	NESTED_SEPARATOR = (1 << 1),
	NESTED_QUOTES = (1 << 2)
};
struct {
	FILE* source;
	enum PipingMode mode;
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
	return 0;
}

static void print_help() {
	fprintf(stderr,"usage: csvpipe [OPTIONS] [FILE]");
	fprintf(stderr,"options:");
	fprintf(stderr, "-s character\n");
	fprintf(stderr, "  Which character is used as separator (default is ,)\n");
	fprintf(stderr, "-c\n");
	fprintf(stderr, "  escape nested separators by \\1 \n");
	fprintf(stderr, "-q\n");
	fprintf(stderr, "  escape nested quotes by \\2 \n");
	fprintf(stderr, "-d\n");
	fprintf(stderr, " drop header row\n");
}




static void parse_config(int argc, char** argv) {
	config.source = stdin;
	config.mode = NEWLINE;
	config.separator = ',';
	config.drop_header = false;
	char c;
	while ((c = getopt (argc, argv, "s:cqd")) != -1) {
		switch (c) {
			case 's': 
				config.separator = optarg[0];
				break;
			case 'c':
				config.mode |= NESTED_SEPARATOR;
				break;
			case 'q':
				config.mode |= NESTED_QUOTES;
				break;
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
	PREV_CELL,
	PREV_QUOTE,
	IN_QUOTE,
	IN_CELL
};

enum tokenizer_state _state = FRESH;

static void do_pipe(size_t chars_read) {
	char* restrict current_char = _buffer;
	char const* restrict char_end = _buffer + chars_read;
	char const* restrict current_start = _buffer;

	while (current_char < char_end) {
		if (*current_char == '"') {
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
				fwrite(current_start, sizeof(char), current_char - current_start, stdout);
				current_start = current_char;
				if (_state != PREV_QUOTE) {
					_state = IN_QUOTE;
				}
				break;
			}
		}
		else if (*current_char == config.separator) {
			current_char++;
		}
		else if (*current_char == '\n') {
			*current_char = '\0';
			current_char++;
		}
		else if (*current_char == '\r') {
			*current_char = '\0';
			current_char++;
			if (*current_char == '\n') {
				// we have windows new lines, so lets skip over this byte
				fwrite(current_start, sizeof(char), current_char - current_start, stdout);
				current_char++;
				current_start = current_char;
			}
		}
		else {
			// normal field
IN_CELL:
			while (++current_char < char_end &&	*current_char != config.separator && *current_char != '\n' && *current_char != '\r');
			if (current_char == char_end) {
				// we reached the end
				fwrite(current_start, sizeof(char), current_char - current_start, stdout);
				current_start = current_char;
				_state = IN_CELL;
				break;
			}
		}
	}
	if (current_start < char_end) {
		fwrite(current_start, sizeof(char), current_char - current_start, stdout);
	}
}

