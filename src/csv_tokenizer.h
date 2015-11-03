#pragma once
#include <stddef.h>
#include <stdbool.h>

// make sure the buffer passed is actually this amount bigger
#define BUFFER_TOKENIZER_POSTFIX 4

struct csv_tokenizer;
typedef struct {
    char const * restrict start;
    size_t length;
} Cell;

void prepare_tokenization(struct csv_tokenizer* restrict tokenizer, char* restrict buffer, size_t buffer_read);

struct csv_tokenizer* setup_tokenizer(char separator, const char* restrict buffer, Cell* restrict cells, size_t cell_size);
void tokenize_cells(struct csv_tokenizer* restrict tokenizer, size_t buffer_offset, size_t buffer_read, size_t* restrict buffer_consumed, size_t* restrict cells_found, bool* restrict last_full);
void free_tokenizer(struct csv_tokenizer* restrict tokenizer);
