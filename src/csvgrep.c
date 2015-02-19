#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>     
#include <stdbool.h>
#include <assert.h>
#include <string.h>
#include <pcre.h> 
#include "csv_tokenizer.h"
#include "debug.h"

#define BUFFER_SIZE 1024*1024
//#define BUFFER_SIZE 30
#define CELL_BUFFER_SIZE (BUFFER_SIZE / 2) + 2

typedef struct {
	pcre const* restrict pattern;
	pcre_extra const* restrict extra;
	
} Regex;

struct csv_tokenizer* _tokenizer;

static char _buffer[BUFFER_SIZE];
static char _prev_line[BUFFER_SIZE * 2];
static size_t _prev_line_length = 0;
static char _prev_cell[BUFFER_SIZE];
static size_t _prev_cell_length = 0;

static Cell _cells[CELL_BUFFER_SIZE];

static int _have_jit = 0;
static int _column_count = 0;
static int _current_cell_id = 0;
static char _separator = ',';
static char _newline[2];
static size_t _newline_length = 0;
static Regex* _patterns = NULL;
static bool _half_line = false;
static bool _half_cell = false;
static bool _count_only = false;
static bool _prev_matches = true;
static bool _negative = false;
static bool _or = false;

static long long _count = 0;

static size_t parse_config(int argc, char** argv, size_t chars_read);

static void output_cells(size_t cells_found, size_t offset, bool last_full);
static void debug_cells(size_t total);

int main(int argc, char** argv) {
	size_t chars_read;
	bool first = true;

	pcre_config(PCRE_CONFIG_JIT, &_have_jit);
	if (!_have_jit) {
		fprintf(stderr, "I am running without PCRE-JIT support, expect less performance.\n");
	}

	while ((chars_read = fread(_buffer, 1, BUFFER_SIZE, stdin)) > 0) {
		LOG_D("New data read: %zu\n", chars_read);
		size_t buffer_consumed = 0;
		size_t cells_found = 0;
		bool last_full = true;

		if (first) {
			first = false;
			buffer_consumed = parse_config(argc, argv, chars_read);
			LOG_D("Finished init : %zu\n", buffer_consumed);
		}

		while (buffer_consumed < chars_read) {
			tokenize_cells(_tokenizer, buffer_consumed, chars_read, &buffer_consumed, &cells_found, &last_full);

			LOG_D("Processed: %zu, Cells: %zu\n", buffer_consumed, cells_found);
			debug_cells(cells_found);
			output_cells(cells_found, 0, last_full);
		}
	}
	if (_count_only) {
		fprintf(stdout, "%llu\n", _count);
	}
	if (_tokenizer != NULL) {
		free_tokenizer(_tokenizer);
	}
	if (_patterns != NULL) {
		for (int c = 0; c < _column_count; c++) {
			pcre_free_study((pcre_extra*)_patterns[c].extra);
			pcre_free((pcre*)_patterns[c].pattern);

		}
	}
	return 0;
}

static void debug_cells(size_t total) {
#ifdef MOREDEBUG
	Cell* current_cell = _cells;
	Cell* cell_end = _cells + total;

	while (current_cell < cell_end) {
		if (current_cell->start == NULL) {
			LOG_V("Cell %zu : Newline\n", (size_t)(current_cell - _cells));
		}
		else if (current_cell->length == 0) {
			LOG_V("Cell %zu : \n", (size_t)(current_cell - _cells));
		}
		else {
			char* s = calloc(sizeof(char), current_cell->length + 1);
			s[*current_cell_length] = '\0';
			memcpy(s, current_cell->start, current_cell->length);
			LOG_V("Cell %zu : %s\n", (size_t)(current_cell - _cells), s);
			free(s);
		}
		current_cell++;
	}
#else
	(void)total;
#endif
}

static void print_help() {
	fprintf(stderr, "-s ,\n");
	fprintf(stderr, "  Which character to use as separator (default is ,)\n");
	fprintf(stderr, "-p column/pattern/\n");
	fprintf(stderr, "  Multiple -p are allowed, they work as an AND \n");
	fprintf(stderr, "-c ,\n");
	fprintf(stderr, "  Only count the rows that match\n");
	fprintf(stderr, "-o ,\n");
	fprintf(stderr, " Make the match into an OR, changes the behavior of -p and -v\n");
	fprintf(stderr, "-v ,\n");
	fprintf(stderr, " Print only the rows that did not match all patterns\n");
}

static size_t parse_config(int argc, char** argv, size_t chars_read) {
	size_t n_patterns = 0;
	char ** columns = malloc(sizeof(char*));
	char ** patterns = malloc(sizeof(char*));
	size_t * column_lengths = malloc(sizeof(size_t*));

	char c;
	while ((c = getopt (argc, argv, "s:p:cvo")) != -1) {
		switch (c) {
			case 's': 
				_separator = optarg[0];
				break;
			case 'c': 
				_count_only = true;
				break;
			case 'p':
				n_patterns++;
				if (n_patterns >= 1) {
					columns = realloc(columns, sizeof(char*) * n_patterns);
					patterns = realloc(patterns, sizeof(char*) * n_patterns);
					column_lengths = realloc(column_lengths, sizeof(size_t*) * n_patterns);
				}
				LOG_V("Got pattern: %s\n", optarg);
				columns[n_patterns - 1] = strtok(optarg, "/");
				patterns[n_patterns - 1] = strtok(NULL, "/");
				column_lengths[n_patterns - 1] = strlen(columns[n_patterns - 1]);
				break;
			case 'v':
				_negative = true;
				break;
			case 'o':
				_or = true;
				break;
			case '?':
			case 'h':
				print_help();
				exit(1);
				break;
		}
	}

	if (n_patterns == 0) {
		fprintf(stderr, "You should at least provide one pattern\n");
		print_help();
		exit(1);
	}

	LOG_D("%s\n","Done parsing config params");	

	_tokenizer = setup_tokenizer(_separator, _buffer, _cells);

	size_t consumed, cells_found;
	bool last_full;
	tokenize_cells(_tokenizer, 0, chars_read, &consumed, &cells_found, &last_full);

	LOG_D("Processed: %zu, Cells: %zu\n", consumed, cells_found);
	debug_cells(cells_found);

	Cell* current_cell = _cells;
	while (current_cell < (_cells + cells_found) && current_cell->start != NULL) {
		if (!_count_only) {
			// also immediatly print the header
			if (current_cell != _cells) {
				fwrite(&(_separator),sizeof(char),1, stdout);
			}
			fwrite(current_cell->start, sizeof(char), current_cell->length, stdout);
		}
		current_cell++;
	}
	_column_count = (int)(current_cell - _cells);

	const char* new_line = _cells[_column_count-1].start + _cells[_column_count - 1].length;
	_newline[0] = new_line[0];
	_newline_length = 1;
	if (new_line[1] == '\n' || new_line[0] == '\r') {
		_newline[1] = '\n';
		_newline_length = 2;
	}
	if (!_count_only) {
		fwrite(_newline, sizeof(char), _newline_length, stdout);
	}

	_patterns = calloc(sizeof(Regex),_column_count);
	memset(_patterns, 0, sizeof(Regex) * _column_count);
	for (int c = 0; c < _column_count; c++) {
		for (size_t pat = 0;  pat < n_patterns; pat++) {
			if (_cells[c].length == column_lengths[pat]) {
				if (strncmp(_cells[c].start, columns[pat], column_lengths[pat])==0) {
					LOG_V("Adding pattern %s for column: %s (%d)\n", patterns[pat], columns[pat],c);
					// we have found the column
					const char *pcreErrorStr;
					int pcreErrorOffset;
					_patterns[c].pattern = pcre_compile(patterns[pat],PCRE_DOLLAR_ENDONLY |  PCRE_DOTALL | PCRE_NO_UTF8_CHECK, &pcreErrorStr, &pcreErrorOffset, NULL); 
					if(_patterns[c].pattern == NULL) {
						fprintf(stderr, "ERROR: Could not compile '%s': %s\n", patterns[pat], pcreErrorStr);
						exit(1);
					}
					_patterns[c].extra = pcre_study(_patterns[c].pattern, _have_jit ? PCRE_STUDY_JIT_COMPILE : 0, &pcreErrorStr);
					if(_patterns[c].extra == NULL) {
						fprintf(stderr, "ERROR: Could not study '%s': %s\n", patterns[pat], pcreErrorStr);
						exit(1);
					}
					break;
				}
			}
		}
	}


	free(columns);
	free(patterns);
	free(column_lengths);

	output_cells(cells_found, _column_count + 1, last_full);
	return consumed;
}

static char const * unquote(char const* restrict quoted, size_t* restrict length);



static void output_cells(size_t cells_found, size_t offset, bool last_full) {
	LOG_D("Starting output: %zu (%d)\n", cells_found, last_full);
	LOG_V("Entry: current_cell: %d\n", _current_cell_id);

	Cell const* restrict current_cell = _cells + offset;
	Cell const* restrict cells_end = _cells + cells_found;

	bool matches = !_or;
	if (_half_line) {
		matches = _prev_matches;
	}
	bool first_line = true;
	char const* restrict current_line_start = current_cell->start;
	size_t current_line_length = 0;

	while (current_cell < cells_end) {
		if (_current_cell_id > _column_count) {
			fprintf(stderr, "Too many cells in this row, expect: %d, got: %d (cell: %zu)\n", _column_count, _current_cell_id, (size_t)(current_cell - _cells));
			exit(1);
			return;
		}
		if (current_cell->start == NULL) {
			if (_current_cell_id == _column_count) {
				// end of the line
				if (matches) {
					if (_count_only) {
						_prev_line_length = 0;
						_count++;
					}
					else {
						if (first_line && _half_line) {
							fwrite(_prev_line, sizeof(char), _prev_line_length, stdout);
							first_line = false;
							_prev_line_length = 0;
						}
						fwrite(current_line_start, sizeof(char), current_line_length, stdout);
						fwrite(_newline, sizeof(char), _newline_length, stdout);
					}
				}
				if (first_line) {
					first_line = false;
				}
				current_line_start = (current_cell + 1)->start;
				current_line_length = 0;
				_current_cell_id = -1;
				matches = !_or;
			}
			else if (_current_cell_id < _column_count) {
				fprintf(stderr, "Not enough cells in this row, expect: %d, got: %d (cell %zu)\n", _column_count, _current_cell_id,  (size_t)(current_cell - _cells));
				exit(1);
				return;
			}
		}
		else if (matches || _or) { // only if we have a match does it make sense to test other cells
			current_line_length += 1 + current_cell->length;
			if (_current_cell_id == 0 || current_cell == (_cells + offset)) {
				current_line_length--; // the first doesn't have a separator
			}
			if (_patterns[_current_cell_id].pattern != NULL) {
				char const* restrict cell = current_cell->start;
				size_t length = current_cell->length;
				if (current_cell == (cells_end-1) && !last_full) {
					// we do not have the full cell at the moment, let's copy it
					size_t old_cell_length = _prev_cell_length;
					_prev_cell_length += current_cell->length;
					memcpy(_prev_cell + old_cell_length, current_cell->start, sizeof(char) * current_cell->length);
					_half_cell = true;
					_current_cell_id++;
					break;
				}
				if (_half_cell && current_cell == _cells) {
					// append the current cell to the back of the previous one.
					assert(_prev_cell_length + length < BUFFER_SIZE);
					memcpy(_prev_cell + _prev_cell_length, cell, sizeof(char) * length);
					cell = _prev_cell;
					length +=  _prev_cell_length;
					_prev_cell_length = 0;
				}
				if (length > 1 && cell[0] == '"') {
					cell++;
					length -= 2;
					char const* restrict c = cell-1;
					char const* restrict cell_end = cell + length;
					while (++c < cell_end && *c != '"');
					if (c != cell_end) {
						// we have nested quotes
						cell = unquote(cell, &length);
					}
				}
				int ovector[255];
				int matchResult = pcre_exec(_patterns[_current_cell_id].pattern, _patterns[_current_cell_id].extra, cell, length, 0, 0, ovector, 255);
				if (_or) {
					matches |= (matchResult >= 0) ^ _negative;
				}
				else {
					matches &= (matchResult >= 0) ^ _negative;
				}
#ifdef MOREDEBUG
				if (matchResult < 0) {
					fprintf(stderr, "tried to match :'");
					fwrite(cell, sizeof(char), length, stderr);
					fprintf(stderr, "'\n");
					switch(matchResult) {
						case PCRE_ERROR_NOMATCH      : fprintf(stderr,"String did not match the pattern\n");        break;
						case PCRE_ERROR_NULL         : fprintf(stderr,"Something was null\n");                      break;
						case PCRE_ERROR_BADOPTION    : fprintf(stderr,"A bad option was passed\n");                 break;
						case PCRE_ERROR_BADMAGIC     : fprintf(stderr,"Magic number bad (compiled re corrupt?)\n"); break;
						case PCRE_ERROR_UNKNOWN_NODE : fprintf(stderr,"Something kooky in the compiled re\n");      break;
						case PCRE_ERROR_NOMEMORY     : fprintf(stderr,"Ran out of memory\n");                       break;
						default                      : fprintf(stderr,"Unknown error\n");                           break;
					}
				}
#endif
			}
		}

		_current_cell_id++;
		current_cell++;
	}
	if (_current_cell_id != 0) {
		// the last row wasn't completly printed, so we must be inside a row
		_prev_matches = matches;
		if (_prev_matches) {
			// it could still match, so let's copy the line
			size_t old_line_length = _prev_line_length;
			_prev_line_length += current_line_length;
			assert(_prev_line_length < (BUFFER_SIZE * 2));
			memcpy(_prev_line + old_line_length, current_line_start, sizeof(char) * current_line_length);
			if (last_full && _current_cell_id != _column_count) { // the , gets eaten away
				_prev_line[_prev_line_length++] = _separator;
			}
#ifdef MOREDEBUG
			fprintf(stderr, "current prev line :'");
			fwrite(_prev_line, sizeof(char), _prev_line_length, stderr);
			fprintf(stderr, "'\n");
#endif
		}
		_half_line = true;
		if (!last_full) {
			_current_cell_id--;
		}
	}
	else {
		_half_line = false;
	}
	LOG_V("Exit: current_cell: %d\n", _current_cell_id);
}


static char _unquote_buffer[BUFFER_SIZE];
static char const * unquote(char const* restrict quoted, size_t* restrict length) {
	char * restrict result = _unquote_buffer;
	char const * restrict current_char = quoted; 
	char const * restrict char_end = quoted + *length; 
	while (current_char < char_end) {
		if (*current_char == '"') {
			// must be an escaped "
			current_char++;
			(*length)--;
		}
		*result++ = *current_char++;
	}
	return _unquote_buffer;
}
