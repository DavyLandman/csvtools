#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include "debug.h"
#include "csv_tokenizer.h"

enum tokenizer_state {
	FRESH,
	PREV_NEWLINE,
	PREV_CELL,
	PREV_QUOTE,
	IN_QUOTE
};

struct csv_tokenizer {
	const char* restrict buffer;
	Cell* restrict cells;
	Cell const* restrict cells_end;

	char separator;

	enum tokenizer_state state;
};

struct csv_tokenizer* setup_tokenizer(char separator, const char* restrict buffer, Cell* restrict cells, size_t cell_size) {
	struct csv_tokenizer* tokenizer = malloc(sizeof(struct csv_tokenizer));
	tokenizer->separator = separator;
	tokenizer->buffer = buffer;
	tokenizer->cells = cells;
	tokenizer->cells_end = cells + cell_size - 2; // two room at the end

	tokenizer->state = FRESH;
	return tokenizer;
}

void free_tokenizer(struct csv_tokenizer* restrict tokenizer) {
	free(tokenizer);
}

void tokenize_cells(struct csv_tokenizer* restrict tokenizer, size_t buffer_offset, size_t buffer_read, size_t* restrict buffer_consumed, size_t* restrict cells_found, bool* restrict last_full) {
	const char* restrict current_char = tokenizer->buffer + buffer_offset;
	const char* restrict char_end = tokenizer->buffer + buffer_read;
	const char* restrict char_end4 = tokenizer->buffer + buffer_read - 4;
	const char* restrict current_start = current_char;

	Cell* restrict cell = tokenizer->cells;
	LOG_V("tokenizer-start\t%d %c (%lu)\n", tokenizer->state, *current_char, buffer_offset );

	*last_full = true;
	enum tokenizer_state old_state = tokenizer->state;
	tokenizer->state = FRESH;
	switch (old_state) {
	case PREV_QUOTE:
		if (*current_char == '"') {
			// escaped quote so we don't have to decrease the first char
			goto IN_QUOTE;
		}
		current_char--; // jump back, since starts with increment
		goto AFTER_QUOTE;

	case IN_QUOTE:
		current_char--; // jump back, since the loops starts with increment
		goto IN_QUOTE;
	
	case PREV_NEWLINE:
		if ((*current_char == '\n' || *current_char == '\r')) {
			while (++current_char < char_end && (*current_char == '\n' || *current_char == '\r'));
		}
		break;
		
	case PREV_CELL:
		current_char--; // jump back, since the loops starts with increment
		goto NORMAL_CELL;

	default:
		if (*current_char == tokenizer->separator) {
			cell->start = current_start;
			cell->length = 0;
			cell++;
			current_char++;
			current_start = current_char;
		}
	}

	while (current_char < char_end) {
		if (*current_char == '"') {
IN_QUOTE:
			while (++current_char < char_end) {
				if (*current_char == '"') {
					const char* peek = current_char + 1;
					if (peek == char_end) {
						// at the end of stream and not sure if escaped or not
						tokenizer->state = PREV_QUOTE;
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
			cell->start = current_start;
			cell->length = (size_t)((current_char)-current_start);
			cell++;

			if (current_char == char_end) {
				if (*(current_char-1) != '"' || *(current_char-2) == '"' ||  current_char - 1 == current_start) {
					if (tokenizer->state == FRESH) {
						tokenizer->state = IN_QUOTE;
					}
					*last_full = false;
					break;
				}
				*last_full = false; // is this correct? does it ever happen?
				break;
			}
			
			if (*current_char == '\n' || *current_char == '\r') {
				cell->start = NULL;
				cell->length = -1;
				cell++;
				// consume newline
				while (++current_char < char_end && (*current_char == '\n' || *current_char == '\r'));
				if (current_char == char_end) {
					// we stopped inside a new_line
					tokenizer->state = PREV_NEWLINE;
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
			if (cell >= tokenizer->cells_end) {
				break;
			}
		}
		else if (*current_char == tokenizer->separator) {
			// an empty cell somewhere in the middle
			cell->start = current_start;
			cell->length = 0;
			cell++;
			current_start = ++current_char;
			if (cell >= tokenizer->cells_end) {
				break;
			}
		}
		else if (*current_char == '\n' || *current_char == '\r') {
			// an newline means that we had an empty cell as last cell of the
			// row
			cell->start = current_start;
			cell->length = 0;
			cell++;

			cell->start = NULL;
			cell->length = -1;
			cell++;
			// consume newline
			while (++current_char < char_end && (*current_char == '\n' || *current_char == '\r'));
			if (current_char == char_end) {
				// we stopped inside a new_line
				tokenizer->state = PREV_NEWLINE;
				break;
			}
			current_start = current_char;
			if (cell >= tokenizer->cells_end) {
				break;
			}
		}
		else {
			// start of a new field
NORMAL_CELL:;
			char sep = tokenizer->separator;
			while (current_char < char_end4) {
				if (current_char[1] == sep ||current_char[1] == '\n' || current_char[1] == '\r')  {
					current_char += 1;
					goto FOUND_CELL_END;
				}
				if (current_char[2] == sep ||current_char[2] == '\n' || current_char[2] == '\r')  {
					current_char += 2;
					goto FOUND_CELL_END;
				}
				if (current_char[3] == sep ||current_char[3] == '\n' || current_char[3] == '\r')  {
					current_char += 3;
					goto FOUND_CELL_END;
				}
				if (current_char[4] == sep ||current_char[4] == '\n' || current_char[4] == '\r')  {
					current_char += 4;
					goto FOUND_CELL_END;
				}
				current_char += 4;
			}
			while (++current_char < char_end &&	*current_char != tokenizer->separator && *current_char != '\n' && *current_char != '\r');
FOUND_CELL_END:
			cell->start = current_start;
			cell->length = (size_t)((current_char)-current_start);
			cell++;

			if (current_char == char_end) {
				if (*(current_char-1) != tokenizer->separator) {
					tokenizer->state = PREV_CELL;
					*last_full = false;
					break;
				}
			}
			else if (*current_char == '\n' || *current_char == '\r') {
				cell->start = NULL;
				cell->length = -1;
				cell++;
				// consume newline
				while (++current_char < char_end && (*current_char == '\n' || *current_char == '\r'));
				if (current_char == char_end) {
					// we stopped inside a new_line
					tokenizer->state = PREV_NEWLINE;
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
			if (cell >= tokenizer->cells_end) {
				break;
			}
		}
	}
	*buffer_consumed = (size_t)(current_char - tokenizer->buffer);
	*cells_found = (size_t)(cell - tokenizer->cells);

	LOG_V("tokenizer-done\t%d, %c (%lu) %d\n", tokenizer->state,  *(current_char-1), *buffer_consumed  , *last_full);
}
