#pragma once
#include <stddef.h>
#include <stdbool.h>


struct csv_tokenizer;

struct csv_tokenizer* setup_tokenizer(char separator, const char* restrict buffer, char const ** restrict cell_starts, size_t* restrict cell_lengths, size_t cell_buffers_size);
void tokenize_cells(struct csv_tokenizer* restrict tokenizer, size_t buffer_offset, size_t buffer_read, size_t* restrict buffer_consumed, size_t* restrict cells_found, bool* restrict last_full);
void free_tokenizer(struct csv_tokenizer* restrict tokenizer);
