#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>     
#include <stdbool.h>
#include <assert.h>
#include <string.h>
#include "string_utils.h"
#include "csv_tokenizer.h"
#include "debug.h"

#define BUFFER_SIZE 1024*1024
//#define BUFFER_SIZE 30
//#define BUFFER_SIZE 72
#define CELL_BUFFER_SIZE (BUFFER_SIZE / 2) + 2

#define FREEARRAY(l,c) { for(size_t ___i = 0; ___i< c; ___i++) { free(l[___i]); } }


struct csv_tokenizer* _tokenizer;

static char _buffer[BUFFER_SIZE];
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

	while ((chars_read = fread(_buffer, 1, BUFFER_SIZE, config.source)) > 0) {
		LOG_D("New data read: %zu\n", chars_read);
		size_t buffer_consumed = 0;
		size_t cells_found = 0;
		bool last_full = true;

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
}

static struct {
	size_t cuts_defined;
	char** keep_column_names;
	char** drop_column_names;
	int* keep_column_indexes;
	int* drop_column_indexes;
} preconfig;

static void parse_config(int argc, char** argv) {
	config.separator = ',';
	config.source = stdin;

	preconfig.cuts_defined = 0;
	preconfig.keep_column_names = NULL;
	preconfig.drop_column_names = NULL;
	preconfig.keep_column_indexes = NULL;
	preconfig.drop_column_indexes = NULL;

	char c;
	while ((c = getopt (argc, argv, "s:k:d:K:D:i:o:h")) != -1) {
		switch (c) {
			case 's': 
				config.separator = optarg[0];
				break;
			case 'k':
			case 'd':
			case 'K':
			case 'D':
				if (preconfig.keep_column_names || preconfig.drop_column_names || preconfig.keep_column_indexes || preconfig.drop_column_indexes) {
					fprintf(stderr, "Error, you can only pass one kind of cut option.\n");
					exit(1);
				}
				char** splitted = strsplit(optarg, ",", &(preconfig.cuts_defined));
				switch (c) {
				case 'k':
					preconfig.keep_column_names = splitted;
					break;
				case 'd':
					preconfig.drop_column_names = splitted;
					break;
				case 'D':
				case 'K': ;
					int* indexes = to_int(splitted, preconfig.cuts_defined);
					FREEARRAY(splitted, preconfig.cuts_defined);
					free(splitted);
					if (c == 'K') {
						preconfig.keep_column_indexes = indexes;
					}
					else {
						preconfig.drop_column_indexes = indexes;
					}
					break;
				}
				break;
			case '?':
			case 'h':
				print_help();
				exit(1);
				break;
		}
	}

	if (!(preconfig.keep_column_names || preconfig.drop_column_names || preconfig.keep_column_indexes || preconfig.drop_column_indexes)) {
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

	if (preconfig.keep_column_names || preconfig.drop_column_names) {
		char** cuts = preconfig.keep_column_names ? preconfig.keep_column_names : preconfig.drop_column_names; 
		for (int c = 0; c < config.column_count; c++) {
			bool cond = contains_n((const char**)cuts, preconfig.cuts_defined, _cells[c].start, _cells[c].length);
			if ((cond && preconfig.keep_column_names) || (!cond && preconfig.drop_column_names)) {
				config.keep[c] = true;
			}
		}
		FREEARRAY(cuts, preconfig.cuts_defined);
		free(cuts);
	}
	else if (preconfig.keep_column_indexes) {
		for (size_t i = 0; i < preconfig.cuts_defined; i++) {
			if (preconfig.keep_column_indexes[i] > config.column_count) {
				fprintf(stderr, "There are only %d columns, %d is to high", config.column_count + 1, preconfig.keep_column_indexes[i]);
				exit(1);
				return;
			}
			config.keep[preconfig.keep_column_indexes[i]] = true;
		}
		free(preconfig.keep_column_indexes);
	}
	else if (preconfig.drop_column_indexes) {
		for (int c = 0; c < config.column_count; c++) {
			config.keep[c] = true;
		}
		for (size_t i = 0; i < preconfig.cuts_defined; i++) {
			if (preconfig.drop_column_indexes[i] > config.column_count) {
				fprintf(stderr, "There are only %d columns, %d is to high", config.column_count + 1, preconfig.drop_column_indexes[i]);
				exit(1);
				return;
			}
			config.keep[preconfig.drop_column_indexes[i]] = false;
		}
		free(preconfig.drop_column_indexes);
	}
	else {
		assert(false);
	}
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
