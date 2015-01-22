#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>     
#include <stdbool.h>
#include <assert.h>
#include <string.h>
#include "string_utils.h"
#include "csv_parsing.h"
#include "csvchop_shared.h"


#define BITMASK(b) (1 << ((b) % CHAR_BIT))
#define BITSLOT(b) ((b) / CHAR_BIT)
#define BITSET(a, b) ((a)[BITSLOT(b)] |= BITMASK(b))
#define BITCLEAR(a, b) ((a)[BITSLOT(b)] &= ~BITMASK(b))
#define BITTEST(a, b) ((a)[BITSLOT(b)] & BITMASK(b))
#define BITNSLOTS(nb) ((nb + CHAR_BIT - 1) / CHAR_BIT)

#define BUFFER_SIZE 1024*1024
//#define BUFFER_SIZE 20
#define CELL_BUFFER_SIZE BUFFER_SIZE / 4

#define FREEARRAY(l,c) { for(size_t ___i = 0; ___i< c; ___i++) { free(l[___i]); } }

Context setup_context() {
	Context ctx;
	ctx.in_quote = false;
	ctx.next_cell = true;
	ctx.prev_newline = false;
	ctx.prev_quote = false;
	ctx.current_cell_id = 0;
	ctx.last_full = true;
	ctx.half_printed = false;
	return ctx;
}

static size_t parse_config(int argc, char** argv, char* buffer, size_t chars_read, Configuration* config);
static void output_cells(Cell* cells, int number_of_cells, Configuration* config, Context* context);
#ifdef MOREDEBUG
static void print_cells(Cell* cells, size_t total);
#endif

int main(int argc, char** argv) {
	Context ctx = setup_context();
	Configuration config;

	void* buf = malloc(BUFFER_SIZE);
	Cell* cells = calloc(sizeof(Cell), CELL_BUFFER_SIZE);
	size_t chars_read;
	bool first = true;

	while ((chars_read = fread(buf, 1, BUFFER_SIZE, stdin)) > 0) {
#ifdef DEBUG
		fprintf(stderr, "New data read: %zu\n", chars_read);
#endif
		ProcessResult processed;
		processed.buffer_read = 0;
		processed.cells_read = 0;
		if (first) {
			// first let's read the config
			processed.buffer_read = parse_config(argc, argv, buf, chars_read, &config);
			first = false;
#ifdef DEBUG
			fprintf(stderr, "Processed: %zu, (%zu) Cells: %d\n", processed.buffer_read, chars_read, processed.cells_read);
#endif
		}
		while (processed.buffer_read < chars_read) {
			processed = parse_cells(buf, chars_read, processed.buffer_read , cells, CELL_BUFFER_SIZE, &config, &ctx);
#ifdef DEBUG
			fprintf(stderr, "Processed: %zu, Cells: %d\n", processed.buffer_read, processed.cells_read);
#endif
#ifdef MOREDEBUG
			print_cells(cells,processed.cells_read);
#endif
			output_cells(cells, processed.cells_read, &config, &ctx);
		}
	}
	return 0;
}

#ifdef MOREDEBUG
static void print_cells(Cell* cells, size_t total) {
	for (size_t c = 0; c < total; c++) {
		if (cells[c].start == NULL) {
			fprintf(stderr, "Cell %zu : Newline\n", c);
		}
		else {
			char* s = calloc(sizeof(char), cells[c].length+1);
			s[cells[c].length] = '\0';
			memcpy(s, cells[c].start, cells[c].length);
			fprintf(stderr, "Cell %zu : %s\n", c, s);
			free(s);
		}

	}
}
#endif

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

static void print_header(const char ** columns, Configuration* config) {
	bool first = true;
	for (int c = 0; c < config->column_count; c++) {
		if (BITTEST(config->keep, c)) {
			if (!first) {
				fprintf(stdout, "%c", config->separator);
			}
			else {
				first = false;
			}
			fprintf(stdout, "%s", columns[c]);
		}
	}
	fprintf(stdout, "\n");
}

static size_t parse_config(int argc, char** argv, char * buffer, size_t chars_read, Configuration* config) {
	config->separator = ',';
	size_t chops_size;
	char** keeps = NULL;
	char** drops = NULL;
	int* index_keeps = NULL;
	int* index_drops = NULL;
	char c;
	while ((c = getopt (argc, argv, "s:k:d:K:D:h")) != -1) {
		switch (c) {
			case 's': 
				config->separator = optarg[0];
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

	size_t result;
	char** columns;
	config->newline = calloc(sizeof(char), 2);
	config->column_count = parse_header(buffer, chars_read, config->newline, &(config->newline_length), &result,  &columns, config->separator);
	assert(columns != NULL);
	config->keep = calloc(sizeof(char),BITNSLOTS(config->column_count + 1));
	memset(config->keep, 0, BITNSLOTS(config->column_count + 1));
	if (keeps || drops) {
		const char** chops = (const char**)(keeps ? keeps : drops);
		for (int c = 0; c < config->column_count; c++) {
			bool cont = contains(chops, chops_size, columns[c]);
			if ((cont && keeps) || (!cont && drops)) {
				BITSET(config->keep, c);
			}
		}
	}
	else if (index_keeps) {
		for (size_t i = 0; i < chops_size; i++) {
			BITSET(config->keep, index_keeps[i]);
		}
	}
	else if (index_drops) {
		for (int c = 0; c < config->column_count; c++) {
			BITSET(config->keep, c);
		}
		for (size_t i = 0; i < chops_size; i++) {
			BITCLEAR(config->keep, index_drops[i]);
		}
	}
	for (int c = 0; c < config->column_count; c++) {
		if (BITTEST(config->keep, c)) {
			config->first_cell = c;
			break;
		}
	}
	print_header((const char**)columns, config);
	if (keeps) {
		FREEARRAY(keeps, chops_size);
	}
	else if (drops) {
		FREEARRAY(drops, chops_size);
	}
	else if (index_keeps) {
		free(index_keeps);
	}
	else if (index_drops) {
		free(index_drops);
	}
	free(columns);
	return result;
}

static void output_cells(Cell* cells, int number_of_cells, Configuration* config, Context* context) {
	for (int c = 0; c < number_of_cells; c++) {
		if (cells[c].start == NULL) {
			if (context->current_cell_id == (config->column_count)) {
				fwrite(config->newline, sizeof(char), config->newline_length, stdout);
				context->current_cell_id = -1;
			}
			else {
				fprintf(stderr, "Not enough cells in this row, expect: %d, got: %d\n", config->column_count, context->current_cell_id);
				exit(1);
			}
		}
		else if (BITTEST(config->keep, context->current_cell_id)) {
			if (c != 0 || !context->half_printed) {
				if (context->current_cell_id != config->first_cell) {
					fwrite(&(config->separator),sizeof(char),1, stdout);
				}
			}
			fwrite(cells[c].start, sizeof(char), cells[c].length, stdout);
		}
		context->current_cell_id++;
	}
	if (!context->last_full) {
		context->half_printed = true;
		context->current_cell_id--;
	}
	else {
		context->half_printed = false;
	}
}
