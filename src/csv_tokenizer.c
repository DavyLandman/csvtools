#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <limits.h>
#include "debug.h"
#include "csv_tokenizer.h"
#define BIT_FIDDLING_HACK_SCAN

#ifdef BIT_FIDDLING_HACK_SCAN

#if UINT32_MAX == UINT_FAST32_MAX
    #define UINT_FAST32_C(v) UINT32_C(v)
#elif UINT64_MAX == UINT_FAST32_MAX
    #define UINT_FAST32_C(v) UINT64_C(v)
#endif


#define REPEAT(n) (~UINT_FAST32_C(0)/255 * (n))
#define HAS_ZERO(v) (((v) - REPEAT(0x10)) & ~(v) & REPEAT(0x80))

#define HAS_VALUE(x,m) (HAS_ZERO((x) ^ (m)))
#define IS_ALIGNED(p,s) (((uintptr_t)(const void*)(p)) % (s) == 0)

#define NEWLINE_MASK REPEAT('\n')
#define CARRIAGE_RETURN_MASK REPEAT('\r')
#endif

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

    unsigned long long records_processed;

	char separator;
#ifdef BIT_FIDDLING_HACK_SCAN
    uint_fast32_t separator_mask;
#endif

	enum tokenizer_state state;
};

struct csv_tokenizer* setup_tokenizer(char separator, const char* restrict buffer, Cell* restrict cells, size_t cell_size) {
	struct csv_tokenizer* tokenizer = malloc(sizeof(struct csv_tokenizer));
	tokenizer->separator = separator;
#ifdef BIT_FIDDLING_HACK_SCAN
    tokenizer->separator_mask = REPEAT(separator);
#endif
	tokenizer->buffer = buffer;
	tokenizer->cells = cells;
	tokenizer->cells_end = cells + cell_size - 2; // two room at the end
	assert(tokenizer->cells < tokenizer->cells_end);

    tokenizer->records_processed = 0;
	tokenizer->state = FRESH;
	return tokenizer;
}

void free_tokenizer(struct csv_tokenizer* restrict tokenizer) {
	free(tokenizer);
}

static void print_current_line(const char* restrict current_char,const char* restrict buffer_start, const char* restrict buffer_end) {
    const char* restrict start = current_char;
    const char* restrict end = current_char;

    // find surround newlines
    while (--start > buffer_start  && *start != '\n' && *start != '\r');
    start++;
    while (++end < buffer_end  && *end != '\n' && *end != '\r');
    end--;

    // copy string such that we can put a \0 at the end
    size_t line_length = end-start;
    char* printable_string = calloc(sizeof(char), line_length + 1);
    memcpy(printable_string, start, line_length);
    printable_string[line_length] = '\0';
    fprintf(stderr, "Current line: %s\n", printable_string);
    free(printable_string);
}

void tokenize_cells(struct csv_tokenizer* restrict tokenizer, size_t buffer_offset, size_t buffer_read, size_t* restrict buffer_consumed, size_t* restrict cells_found, bool* restrict last_full) {
	const char* restrict current_char = tokenizer->buffer + buffer_offset;
	const char* restrict char_end = tokenizer->buffer + buffer_read;
#ifdef BIT_FIDDLING_HACK_SCAN
	const uint_fast32_t* restrict char_end_big_steps = (const uint_fast32_t*)(char_end - sizeof(uint_fast32_t));
#endif
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
                tokenizer->records_processed++;
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
				fprintf(stderr, "Invalid character: \"%c (\\%d)\" found after end of cell (after the %lluth record)\n",*current_char, *current_char,tokenizer->records_processed);
                print_current_line(current_char, tokenizer->buffer, char_end);
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
            tokenizer->records_processed++;
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
#ifdef BIT_FIDDLING_HACK_SCAN
            while (++current_char < char_end && !IS_ALIGNED(current_char, sizeof(uint_fast32_t))) {
                if (*current_char == sep || *current_char == '\n' || *current_char == '\r') {
                    goto NORMAL_CELL_MATCH_FOUND;
                }
            }
	        const uint_fast32_t* restrict large_steps = (const uint_fast32_t*)(current_char);
            uint_fast32_t sep_mask = tokenizer->separator_mask;
			while (large_steps < char_end_big_steps) {
                if (HAS_VALUE(*large_steps, sep_mask) || HAS_VALUE(*large_steps, NEWLINE_MASK) || HAS_VALUE(*large_steps, NEWLINE_MASK)) {
                    // current part
                    break;
                }
                large_steps++;
			}
            current_char = (const char*)large_steps;
            current_char--; // rewind one so that we start at the beginning
#endif
			while (++current_char < char_end &&	*current_char != sep && *current_char != '\n' && *current_char != '\r');
NORMAL_CELL_MATCH_FOUND:;
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
                tokenizer->records_processed++;
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
				fprintf(stderr, "Invalid character: \"%c (\\%d)\" found after end of cell (after the %lluth record)\n",*current_char, *current_char,tokenizer->records_processed);
                print_current_line(current_char, tokenizer->buffer, char_end);
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

