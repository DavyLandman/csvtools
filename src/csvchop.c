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

// config info
static int _column_count = 0;
static int _current_cell_id = 0;
static char _separator = ',';
static char _newline[2];
static size_t _newline_length = 0;
static bool* _keep = NULL;
static int _first_cell = 0;
static bool _half_printed = false;


static size_t parse_config(int argc, char** argv, size_t chars_read);

static void output_cells(size_t cells_found, bool last_full);
static void debug_cells(size_t total);

int main(int argc, char** argv) {
	size_t chars_read;
	bool first = true;

	while ((chars_read = fread(_buffer, 1, BUFFER_SIZE, stdin)) > 0) {
		LOG_D("New data read: %zu\n", chars_read);
		size_t buffer_consumed = 0;
		size_t cells_found = 0;
		bool last_full = true;

		if (first) {
			first = false;
			buffer_consumed = parse_config(argc, argv, chars_read);
			LOG_D("Finished init : %zu\n", buffer_consumed);
		}

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
	if (_keep != NULL) {
		free(_keep);
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
	fprintf(stderr, "-s ,\n");
	fprintf(stderr, "\tWhich character to use as separator (default is ,)\n");
	fprintf(stderr, "-k column,names,to,keep\n");
	fprintf(stderr, "-d column,names,to,drop\n");
	fprintf(stderr, "-K 0,1,3\n");
	fprintf(stderr, "\tWhich columns to keep\n");
	fprintf(stderr, "-D 0,1,3\n");
	fprintf(stderr, "\tWhich columns to drop\n");
}

static size_t parse_config(int argc, char** argv, size_t chars_read) {
	_separator = ',';
	size_t chops_size;
	char** keeps = NULL;
	char** drops = NULL;
	int* index_keeps = NULL;
	int* index_drops = NULL;
	char c;
	while ((c = getopt (argc, argv, "s:k:d:K:D:h")) != -1) {
		switch (c) {
			case 's': 
				_separator = optarg[0];
				break;
			case 'k':
				if (drops || index_keeps || index_drops) {
					fprintf(stderr, "Error, you can only pass one kind of chop option.\n");
					exit(1);
				}
				keeps = strsplit(optarg, ",", &chops_size);
				break;
			case 'd':
				if (keeps || index_keeps || index_drops) {
					fprintf(stderr, "Error, you can only pass one kind of chop option.\n");
					exit(1);
				}
				drops = strsplit(optarg, ",", &chops_size);
				break;
			case 'K':
				if (keeps || drops || index_drops) {
					fprintf(stderr, "Error, you can only pass one kind of chop option.\n");
					exit(1);
				}
			case 'D':
				if (keeps || drops || index_keeps) {
					fprintf(stderr, "Error, you can only pass one kind of chop option.\n");
					exit(1);
				}
				char** splitted = strsplit(optarg, ",", &chops_size);
				if (c == 'K') {
					index_keeps = to_int(splitted, chops_size);
				}
				else {
					index_drops = to_int(splitted, chops_size);
				}
				FREEARRAY(splitted, chops_size);
				free(splitted);
				break;
			case '?':
			case 'h':
				print_help();
				exit(1);
				break;
		}
	}

	if (!(keeps || drops || index_keeps || index_drops)) {
		fprintf(stderr, "You should describe how you want to chop the csv\n");
		print_help();
		exit(1);
	}

	LOG_D("%s\n","Done parsing config params");	

	_tokenizer = setup_tokenizer(_separator, _buffer, _cells,CELL_BUFFER_SIZE);

	size_t consumed, cells_found;
	bool last_full;
	tokenize_cells(_tokenizer, 0, chars_read, &consumed, &cells_found, &last_full);

	LOG_D("Processed: %zu, Cells: %zu\n", consumed, cells_found);
	debug_cells(cells_found);

	Cell const* current_cell = _cells;
	while (current_cell < (_cells + cells_found) && current_cell->start != NULL) {
		current_cell++;
	}
	_column_count = (int)(current_cell - _cells);

	const char* new_line = _cells[_column_count-1].start + _cells[_column_count - 1].length;
	_newline[0] = new_line[0];
	_newline_length = 1;
	if (new_line[1] == '\n' || new_line[0] == '\r') {
		_newline[1] = '\n';
		_newline_length = 2;
	}

	_keep = calloc(sizeof(bool),_column_count);
	for (int c = 0; c < _column_count; c++) {
		_keep[c] =  false;
	}

	if (keeps || drops) {
		const char** chops = (const char**)(keeps ? keeps : drops);
		for (int c = 0; c < _column_count; c++) {
			bool cont = contains_n(chops, chops_size, _cells[c].start, _cells[c].length);
			if ((cont && keeps) || (!cont && drops)) {
				_keep[c] = true;
			}
		}
	}
	else if (index_keeps) {
		for (size_t i = 0; i < chops_size; i++) {
			_keep[index_keeps[i]] = true;
		}
	}
	else if (index_drops) {
		for (int c = 0; c < _column_count; c++) {
			_keep[c] = true;
		}
		for (size_t i = 0; i < chops_size; i++) {
			_keep[index_drops[i]] = false;
		}
	}
	for (int c = 0; c < _column_count; c++) {
		if (_keep[c]) {
			_first_cell = c;
			break;
		}
	}


	if (keeps) {
		FREEARRAY(keeps, chops_size);
		free(keeps);
	}
	else if (drops) {
		FREEARRAY(drops, chops_size);
		free(drops);
	}
	else if (index_keeps) {
		free(index_keeps);
	}
	else if (index_drops) {
		free(index_drops);
	}

	output_cells(cells_found, last_full);
	return consumed;
}


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
			if (_current_cell_id < _column_count) {
				fprintf(stderr, "Not enough cells in this row, expect: %d, got: %d (cell %zu)\n", _column_count, _current_cell_id,  (size_t)(current_cell - _cells));
				exit(1);
				return;
			}
			if (current_chunk_start != NULL) {
				current_chunk_length--; // take away newline 
				if (current_chunk_start != _buffer || !_half_printed) {
					if (current_chunk_start_id != _first_cell) {
						fwrite(&(_separator),sizeof(char),1, stdout);
					}
				}
				fwrite(current_chunk_start, sizeof(char), current_chunk_length, stdout);
			}
			fwrite(_newline, sizeof(char), _newline_length, stdout);
			current_chunk_start = (current_cell + 1)->start;
			current_chunk_length = 0;
			current_chunk_start_id = 0;
			_current_cell_id = -1;
		}
		if (_current_cell_id >= _column_count) {
			fprintf(stderr, "Too many cells in this row, expect: %d, got: %d (cell: %zu)\n", _column_count, _current_cell_id, (size_t)(current_cell - _cells));
			exit(1);
			return;
		}
		else if (_keep[_current_cell_id]) {
			current_chunk_length += 1 + current_cell->length;
		}
		else {
			// a column to drop, so lets write the previous chunk
			if (_current_cell_id >= _first_cell && current_chunk_length > 0) {
				current_chunk_length--; // take away last seperator
				if (current_chunk_start != _buffer || !_half_printed) {
					if (current_chunk_start_id != _first_cell) {
						fwrite(&(_separator),sizeof(char),1, stdout);
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
			if (current_chunk_start_id != _first_cell) {
				fwrite(&(_separator),sizeof(char),1, stdout);
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

static void output_cells2(size_t cells_found, bool last_full) {
	LOG_D("Starting output: %zu (%d)\n", cells_found, last_full);
	LOG_V("Entry: current_cell: %d\n", _current_cell_id);
	Cell const * current_cell = _cells;
	Cell const * cell_end = _cells + cells_found;
	while (current_cell < cell_end) {
		LOG_D("Current cell: %d %p\n", _current_cell_id,current_cell->start);
		if (_current_cell_id > _column_count) {
			fprintf(stderr, "Too many cells in this row, expect: %d, got: %d (cell: %zu)\n", _column_count, _current_cell_id, (size_t)(current_cell - _cells));
			exit(1);
			return;
		}
		if (current_cell->start == NULL) {
			if (_current_cell_id == _column_count) {
				fwrite(_newline, sizeof(char), _newline_length, stdout);
				_current_cell_id = -1;
			}
			else if (_current_cell_id < _column_count) {
				fprintf(stderr, "Not enough cells in this row, expect: %d, got: %d (cell %zu)\n", _column_count, _current_cell_id,  (size_t)(current_cell - _cells));
				exit(1);
				return;
			}
		}
		else if (_keep[_current_cell_id]) {
			if (current_cell != _cells || !_half_printed) {
				if (_current_cell_id != _first_cell) {
					fwrite(&(_separator),sizeof(char),1, stdout);
				}
			}
			fwrite(current_cell->start, sizeof(char), current_cell->length, stdout);
		}

		_current_cell_id++;
		current_cell++;
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
