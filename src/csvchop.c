#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>     

#define BITMASK(b) (1 << ((b) % CHAR_BIT))
#define BITSLOT(b) ((b) / CHAR_BIT)
#define BITSET(a, b) ((a)[BITSLOT(b)] |= BITMASK(b))
#define BITCLEAR(a, b) ((a)[BITSLOT(b)] &= ~BITMASK(b))
#define BITTEST(a, b) ((a)[BITSLOT(b)] & BITMASK(b))
#define BITNSLOTS(nb) ((nb + CHAR_BIT - 1) / CHAR_BIT)

#define BUFFER_SIZE 1024*1024
#define CELL_BUFFER_SIZE BUFFER_SIZE / 4

#define FREEARRAY(l,c) { for(int ___i = 0; ___i< c; ___i++) { free(l[___i]); } }

typedef struct Context { 
	
} Context;

typedef struct Cell {
	char* start;
	int length;
} Cell;

typedef struct Configuration {
	char* keep;
	int column_count;
	char separator;   
} Configuration;

typedef struct ProcessResult {
	int cells_read;
	size_t buffer_read;
} ProcessResult;

int main(int argc, char** argv) {
	Configuration config = parseConfig(argc, argv);
	Context ctx;

	void* buf = malloc(BUFSIZE);
	Cell[] cells = calloc(sizeof(Cell), CELL_BUFER_SIZE);
	size_t chars_read;
	while ((chars_read = fread(buf, 1, BUFSIZE, stdin)) > 0) {
		ProcessResult processed;
		while (processed.buffer_read < chars_read) {
			processed = parse_cells(buf, processed.buffer_read, chars_read, cells, &config, &ctx);
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
static Configuration parseConfig(int argc, char** argv) {
	Configuration config;
	config.separator = ',';
	int chops_size;
	char** keeps = NULL;
	char** drops = NULL;
	int* index_keeps = NULL;
	int* index_drops = NULL;
	while ((c = getopt (argc, argv, "skdKDh")) != -1) {
		switch (c) {
			case 's': 
				config.separator = optarg[0];
				break;
			case 'k':
				if (drops || index_keeps || index_drops) {
					fprintf(stderr, "Error, you can only pass one kind of chop option.\n");
					abort();
				}
				chop_size = split(',' optarg, &keeps);
				break;
			case 'd':
				if (keeps || index_keeps || index_drops) {
					fprintf(stderr, "Error, you can only pass one kind of chop option.\n");
					abort();
				}
				chop_size = split(',' optarg, &drops);
				break;
			case 'K':
				if (keeps || drops || index_drops) {
					fprintf(stderr, "Error, you can only pass one kind of chop option.\n");
					abort();
				}
			case 'D':
				if (keeps || drops || index_keeps) {
					fprintf(stderr, "Error, you can only pass one kind of chop option.\n");
					abort();
				}
				char** splitted;
				chop_size = split(',' optarg, &splitted);
				if (c == 'K') {
					to_int(splitted, &index_keeps, chop_size);
				}
				else {
					to_int(splitted, &index_drops, chop_size);
				}
				for (int i=0; i< chop_size; i++) {
					free(splitted[i]);
				}
				free(splitted);
				break;
			case '?':
			case 'h':
				print_help();
				abort();
				break;
		}
	}

	if (!(keeps || drops || index_keeps || index_drops)) {
		fprintf(stderr, "You should describe how you want to chop the csv\n");
		print_help();
		abort();
	}
	char* header = calloc(sizeof(char), BUFFER_SIZE);
	if (fgets(header, BUFFER_SIZE, stdin) != NULL) {
		char** columns;
		config.column_count = parseHeader(header, BUFFER_SIZE, &fields, config.separator);
		config.keep = calloc(sizeof(char),BITNSLOTS(config.column_count));
		if (keeps || drops) {
			char** chops = keeps ? keeps : drops;
			for (int c = 0; c < config.column_count; c++) {
				cont = contains(chops, chop_count, columns[c]);
				if ((cont && keeps) || (!cont && drops)) {
					BITSET(config.keep, c);
				}
			}
		}
		else if (index_keeps) {
			for (int i = 0; i < chop_size; i++) {
				BITSET(config.keep, index_keeps[i]);
			}
		}
		else if (index_drops) {
			for (int c = 0; c < config.column_count; c++) {
				BITSET(config.keep, c);
			}
			for (int i = 0; i < chop_size; i++) {
				BITCLEAR(config.keep, index_drops[i]);
			}
		}
		print_header(columns, config);
		if (keeps) {
			FREEARRAY(keeps, chop_count);
		}
		else if (drops) {
			FREEARRAY(drops, chop_count);
		}
		else if (index_keeps) {
			free(index_keeps);
		}
		else if (index_drops) {
			free(index_drops);
		}
		FREEARRAY(columns, config.column_count);
		free(columns);
		free(header);
	}
	else {
		fprint(stderr, "No valid header passed on stdin\n");
		free(header);
		abort();
	}
	return config;
}
