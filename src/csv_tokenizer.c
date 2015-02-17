#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include "debug.h"


struct csv_tokenizer {
	const char* buffer;
	char const** cell_start;
	char const** cell_start_end;
	size_t* cell_length;
	size_t* cell_length_end;
	char separator;

	// tokenizer state
	bool prev_newline;
	bool prev_cell;

	bool prev_quote;
	bool in_quote;
};

struct csv_tokenizer* setup_tokenizer(char separator, const char* buffer, char const ** cell_starts, size_t* cell_lengths, size_t cell_buffers_size) {
	struct csv_tokenizer* tokenizer = malloc(sizeof(struct csv_tokenizer));
	tokenizer->separator = separator;
	tokenizer->buffer = buffer;
	tokenizer->cell_start = cell_starts;
	tokenizer->cell_start_end = cell_starts + cell_buffers_size - 2; // leave 2 at the end for newline and half record
	tokenizer->cell_length = cell_lengths;
	tokenizer->cell_length_end = cell_lengths + cell_buffers_size - 2;

	tokenizer->prev_newline = false;

	tokenizer->prev_quote = false;
	tokenizer->prev_cell = false;
	tokenizer->in_quote = false;
	return tokenizer;
}


void tokenize_cells(struct csv_tokenizer* tokenizer, size_t buffer_offset, size_t buffer_read, size_t* consumed, size_t* cells_found, bool* last_full) {
	const char* current_char = tokenizer->buffer + buffer_offset;
	const char* char_end = tokenizer->buffer + buffer_read;
	const char* char_end4 = tokenizer->buffer + buffer_read - 4;
	const char* current_start = current_char;

	char const** cell_start = tokenizer->cell_start;
	size_t* cell_length = tokenizer->cell_length;
	LOG_V("tokenizer-start\t%d, %d, %d, %d %c (%lu)\n", tokenizer->prev_quote, tokenizer->in_quote, tokenizer->prev_newline, tokenizer->prev_cell, *current_char, buffer_offset );

	*last_full = true;
	if (tokenizer->prev_quote) {
		tokenizer->prev_quote = false;
		tokenizer->in_quote = false;
		if (*current_char == '"') {
			// escaped quote so we don't have to decrease the first char
			goto IN_QUOTE;
		}
		current_char--; // jump back, since starts with increment
		goto AFTER_QUOTE;
	}
	if (tokenizer->in_quote) {
		tokenizer->in_quote = false;
		current_char--; // jump back, since the loops starts with increment
		goto IN_QUOTE;
	}
	else if (tokenizer->prev_newline) {
		tokenizer->prev_newline = false;
		if ((*current_char == '\n' || *current_char == '\r')) {
			while (++current_char < char_end && (*current_char == '\n' || *current_char == '\r'));
		}
	}
	else if (tokenizer->prev_cell) {
		tokenizer->prev_cell = false;
		current_char--; // jump back, since the loops starts with increment
		goto NORMAL_CELL;
	}
	else if (*current_char == tokenizer->separator) {
		*cell_start++ = current_start;
		*cell_length++ = 0;
		current_char++;
		current_start = current_char;
	}

	while (current_char < char_end) {
		if (*current_char == '"') {
IN_QUOTE:
			while (++current_char < char_end) {
				if (*current_char == '"') {
					const char* peek = current_char + 1;
					if (peek == char_end) {
						// at the end of stream and not sure if escaped or not
						tokenizer->prev_quote = true;
						*last_full = false;
						break;
					}
					else if (*peek == '"') {
						current_char++;
						continue;
					}
					else {
						break;
					}
				}
			}
AFTER_QUOTE:
			if (current_char != char_end) {
				current_char++;
			}
			*cell_start++ = current_start;
			*cell_length++ = (size_t)((current_char)-current_start);

			if (current_char == char_end) {
				if (*(current_char-1) != '"' || *(current_char-2) == '"' ||  current_char - 1 == current_start) {
					tokenizer->in_quote = true;
					*last_full = false;
					break;
				}
				*last_full = false; // is this correct? does it ever happen?
				break;
			}
			
			if (*current_char == '\n' || *current_char == '\r') {
				*cell_start++ = NULL;
				*cell_length++ = -1;
				// consume newline
				while (++current_char < char_end && (*current_char == '\n' || *current_char == '\r'));
				if (current_char == char_end) {
					// we stopped inside a new_line
					tokenizer->prev_newline = true;
					break;
				}
				current_start = current_char;
			}
			else if (*current_char == tokenizer->separator) {
				current_char++;
				current_start = current_char;
			}
			else {
				fprintf(stderr, "Invalid character: \"%c (\\%d)\" found after end of cell\n",*current_char, *current_char);
				exit(1);
				return;
			}

			if (cell_start >= tokenizer->cell_start_end) {
				break;
			}
		}
		else if (*current_char == tokenizer->separator) {
			// an empty cell somewhere in the middle
			*cell_start++ = current_start;
			*cell_length++ = 0;
			current_start = ++current_char;
			if (cell_start >= tokenizer->cell_start_end) {
				break;
			}
		}
		else if (*current_char == '\n' || *current_char == '\r') {
			// an newline means that we had an empty cell as last cell of the
			// row
			*cell_start++ = current_start;
			*cell_length++ = 0;

			*cell_start++ = NULL;
			*cell_length++ = -1;
			// consume newline
			while (++current_char < char_end && (*current_char == '\n' || *current_char == '\r'));
			if (current_char == char_end) {
				// we stopped inside a new_line
				tokenizer->prev_newline = true;
				break;
			}
			current_start = current_char;
			if (cell_start >= tokenizer->cell_start_end) {
				break;
			}
		}
		else {
			// start of a new field
NORMAL_CELL:;
			char sep = tokenizer->separator;
#define NEW_LINE_ROUGH_MATCH 0x0F // 0b00001111 is the or from \n and \r, if those are the only bits set, there might be a chance of a newline
			while (current_char < char_end4) {
				bool possibleEnd = current_char[1] == sep || (current_char[1] | NEW_LINE_ROUGH_MATCH) == NEW_LINE_ROUGH_MATCH;
				possibleEnd |= current_char[2] == sep || (current_char[2] | NEW_LINE_ROUGH_MATCH) == NEW_LINE_ROUGH_MATCH;
				possibleEnd |= current_char[3] == sep || (current_char[3] | NEW_LINE_ROUGH_MATCH) == NEW_LINE_ROUGH_MATCH;
				possibleEnd |= current_char[4] == sep || (current_char[4] | NEW_LINE_ROUGH_MATCH) == NEW_LINE_ROUGH_MATCH;
				if (possibleEnd) {
					break;
				}
				current_char += 4;
			}
			while (++current_char < char_end &&	*current_char != sep && *current_char != '\n' && *current_char != '\r');
			*cell_start++ = current_start;
			*cell_length++ = (size_t)((current_char)-current_start);

			if (current_char == char_end) {
				if (*(current_char-1) != tokenizer->separator) {
					tokenizer->prev_cell = true;
					*last_full = false;
					break;
				}
			}
			else if (*current_char == '\n' || *current_char == '\r') {
				*cell_start++ = NULL;
				*cell_length++ = -1;
				// consume newline
				while (++current_char < char_end && (*current_char == '\n' || *current_char == '\r'));
				if (current_char == char_end) {
					// we stopped inside a new_line
					tokenizer->prev_newline = true;
					break;
				}
				current_start = current_char;
			}
			else if (*current_char == tokenizer->separator) {
				current_char++;
				current_start = current_char;
			}
			else {
				fprintf(stderr, "Invalid character-2: \"%c\" found after end of cell\n",*current_char);
				exit(1);
				return;
			}

			if (cell_start >= tokenizer->cell_start_end) {
				break;
			}
		}
	}
	*consumed = (size_t)(current_char - tokenizer->buffer);
	*cells_found = (size_t)(cell_start - tokenizer->cell_start);

	LOG_V("tokenizer-done\t%d, %d, %d, %d %c (%lu) %d\n", tokenizer->prev_quote, tokenizer->in_quote, tokenizer->prev_newline, tokenizer->prev_cell, *(current_char-1), *consumed  , *last_full);
}
