#pragma once
#include <stddef.h>
#include <stdbool.h>


struct csv_tokenizer;

struct csv_tokenizer* setup_tokenizer(char separator, const char* buffer, char const ** cell_starts, size_t* cell_lengths, size_t cell_buffers_size);
void tokenize_cells(struct csv_tokenizer* tokenizer, size_t buffer_offset, size_t buffer_read, size_t* buffer_consumed, size_t* cells_found, bool* last_full);
