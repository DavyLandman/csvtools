#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>     
#include <stdbool.h>
#include <assert.h>
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
#define CELL_BUFFER_SIZE BUFFER_SIZE / 4

#define FREEARRAY(l,c) { for(size_t ___i = 0; ___i< c; ___i++) { free(l[___i]); } }


static Configuration parse_config(int argc, char** argv);
static void output_cells(Cell* cells, int number_of_cells, Configuration* config, Context* context);

int main(int argc, char** argv) {
	Configuration config = parse_config(argc, argv);
	Context ctx;

	void* buf = malloc(BUFFER_SIZE);
	Cell* cells = calloc(sizeof(Cell), CELL_BUFFER_SIZE);
	size_t chars_read;
	while ((chars_read = fread(buf, 1, BUFFER_SIZE, stdin)) > 0) {
		ProcessResult processed;
		while (processed.buffer_read < chars_read) {
			processed = parse_cells(buf, processed.buffer_read, chars_read, cells, CELL_BUFFER_SIZE, &config, &ctx);
			output_cells(cells, processed.cells_read, &config, &ctx);
		}
	}
	return 0;
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

static Configuration parse_config(int argc, char** argv) {
	Configuration config;
	config.separator = ',';
	size_t chops_size;
	char** keeps = NULL;
	char** drops = NULL;
	int* index_keeps = NULL;
	int* index_drops = NULL;
	char c;
	while ((c = getopt (argc, argv, "s:k:d:K:D:h")) != -1) {
		switch (c) {
			case 's': 
				config.separator = optarg[0];
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
	char* header = calloc(sizeof(char), BUFFER_SIZE);
	if (fgets(header, BUFFER_SIZE, stdin) != NULL) {
		char** columns;
		config.column_count = parse_header(header, BUFFER_SIZE, &columns, config.separator);
		assert(columns != NULL);
		config.keep = calloc(sizeof(char),BITNSLOTS(config.column_count));
		if (keeps || drops) {
			const char** chops = (const char**)(keeps ? keeps : drops);
			for (int c = 0; c < config.column_count; c++) {
				bool cont = contains(chops, chops_size, columns[c]);
				if ((cont && keeps) || (!cont && drops)) {
					BITSET(config.keep, c);
				}
			}
		}
		else if (index_keeps) {
			for (size_t i = 0; i < chops_size; i++) {
				BITSET(config.keep, index_keeps[i]);
			}
		}
		else if (index_drops) {
			for (int c = 0; c < config.column_count; c++) {
				BITSET(config.keep, c);
			}
			for (size_t i = 0; i < chops_size; i++) {
				BITCLEAR(config.keep, index_drops[i]);
			}
		}
		print_header((const char**)columns, &config);
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
		free(header);
	}
	else {
		fprintf(stderr, "No valid header passed on stdin\n");
		free(header);
		exit(1);
	}
	return config;
}

static void output_cells(Cell* cells, int number_of_cells, Configuration* config, Context* context) {

}
