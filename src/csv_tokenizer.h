#pragma once
#include <stddef.h>
#include <stdbool.h>


struct csv_tokenizer;
typedef struct {
	char const * restrict start;
	size_t length;
} Cell;

struct csv_tokenizer* setup_tokenizer(char separator, const char* restrict buffer, Cell* restrict cells, size_t cell_size);
void tokenize_cells(struct csv_tokenizer* restrict tokenizer, size_t buffer_offset, size_t buffer_read, size_t* restrict buffer_consumed, size_t* restrict cells_found, bool* restrict last_full);
void free_tokenizer(struct csv_tokenizer* restrict tokenizer);
