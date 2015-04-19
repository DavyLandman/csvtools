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

#define AWK_ROW_SEPARATOR '\x1E'
#define AWK_CELL_SEPARATOR '\x1F'
#define CELL_BUFFER_SIZE (BUFFER_SIZE / 2) + 2

struct csv_tokenizer* _tokenizer;

static char _buffer[BUFFER_SIZE];
static Cell _cells[CELL_BUFFER_SIZE];

static struct {
	FILE* source;
} config;

static void parse_config(int argc, char** argv);

static void output_cells(size_t cells_found, bool last_full);
static void debug_cells(size_t total);

int main(int argc, char** argv) {
	size_t chars_read;

	parse_config(argc, argv);

	while ((chars_read = fread(_buffer, 1, BUFFER_SIZE, config.source)) > 0) {
		LOG_D("New data read: %zu\n", chars_read);
		size_t buffer_consumed = 0;
		size_t cells_found = 0;
		bool last_full = true;

		while (buffer_consumed < chars_read) {
			tokenize_cells(_tokenizer, buffer_consumed, chars_read, &buffer_consumed, &cells_found, &last_full);
			LOG_D("Processed: %zu, Cells: %zu\n", buffer_consumed, cells_found);
			debug_cells(cells_found);
			output_cells(cells_found, last_full);
		}
	}
	if (_tokenizer != NULL) {
		free_tokenizer(_tokenizer);
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
	fprintf(stderr,"usage: csvawkpipe [OPTIONS] [FILE]");
	fprintf(stderr,"options:");
	fprintf(stderr, "-s ,\n");
	fprintf(stderr, "  Which character to use as separator (default is ,)\n");
}

static void parse_config(int argc, char** argv) {
	char separator = ',';
	config.source = stdin;
	char c;
	while ((c = getopt (argc, argv, "s:")) != -1) {
		switch (c) {
			case 's': 
				separator = optarg[0];
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

	_tokenizer = setup_tokenizer(separator, _buffer, _cells,CELL_BUFFER_SIZE);
}

static bool _half_printed = false;
static bool _prev_newline = true;

static void output_cells(size_t cells_found, bool last_full) {
	LOG_D("Starting output: %zu (%d)\n", cells_found, last_full);

	Cell const * restrict current_cell = _cells;
	Cell const * restrict cell_end = _cells + cells_found;

	char const* restrict current_start = _buffer;
	size_t current_length = 0;

	if (current_cell->start == NULL) {
		_half_printed = false;
	}

	while (current_cell < cell_end) {
		if (current_cell->start == NULL) {
			Cell const * restrict previous_cell = (current_cell - 1);
			char *newline = _buffer;
			if (previous_cell >= _cells) {
				// we are not at the beginning of the buffer
				newline = (char*)(previous_cell->start + previous_cell->length);
			}
			bool windows_newline = newline[0] == '\r' && newline[1] == '\n';
			*newline = AWK_ROW_SEPARATOR;
			current_length++;

			if (windows_newline) {
				LOG_V("Writing output (windows newline): %p (%p) %zu\n", current_start, _buffer, current_length);
				fwrite(current_start, sizeof(char), current_length, stdout);
				current_start = newline + 2;
				current_length = 0;
			}
			_prev_newline = true;
		}
		else {
			current_length += 1 + current_cell->length;
			if (_prev_newline || _half_printed) {
				_prev_newline = false;
				_half_printed = false;
				current_length--; // no separator
			}
			else if (current_cell->start != _buffer){
				((char*)current_cell->start)[-1] = AWK_CELL_SEPARATOR;
			}
		}
		current_cell++;
	}
	_half_printed = !last_full;

	LOG_V("Writing output: %p (%p) %zu\n", current_start, _buffer, current_length);
	fwrite(current_start, sizeof(char), current_length, stdout);
}
