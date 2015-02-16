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
#define CELL_BUFFER_SIZE BUFFER_SIZE / 4

struct csv_tokenizer* _tokenizer;

static char _buffer[BUFFER_SIZE];
static char _prev_line[BUFFER_SIZE * 2];
static size_t _prev_line_length = 0;
static char _prev_cell[BUFFER_SIZE];
static size_t _prev_cell_length = 0;

static char const* _cell_starts[CELL_BUFFER_SIZE];
static size_t _cell_lengths[CELL_BUFFER_SIZE];

static int _column_count = 0;
static int _current_cell_id = 0;
static char _separator = ',';
static char _newline[2];
static size_t _newline_length = 0;
static pcre** _patterns = NULL;
static pcre_extra** _patterns_extra = NULL;
static bool _half_line = false;
static bool _half_cell = false;
static bool _count_only = false;
static bool _prev_matches = true;

static size_t parse_config(int argc, char** argv, size_t chars_read);

static void output_cells(size_t cells_found, size_t offset, bool last_full);
static void debug_cells(size_t total);

int main(int argc, char** argv) {
	size_t chars_read;
	bool first = true;

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
	if (_tokenizer != NULL) {
		free(_tokenizer);
	}
	return 0;
}

static void debug_cells(size_t total) {
#ifdef MOREDEBUG
	char const ** current_cell_start = _cell_starts;
	char const ** cell_starts_end = _cell_starts + total;
	size_t* current_cell_length = _cell_lengths;

	while (current_cell_start < cell_starts_end) {
		if (*current_cell_start == NULL) {
			LOG_V("Cell %zu : Newline\n", (size_t)(current_cell_start - _cell_starts));
		}
		else if (*current_cell_length == 0) {
			LOG_V("Cell %zu : \n", (size_t)(current_cell_start - _cell_starts));
		}
		else {
			char* s = calloc(sizeof(char), *current_cell_length + 1);
			s[*current_cell_length] = '\0';
			memcpy(s, *current_cell_start, *current_cell_length);
			LOG_V("Cell %zu : %s\n", (size_t)(current_cell_start - _cell_starts), s);
			free(s);
		}
		current_cell_start++;
		current_cell_length++;
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
}

static size_t parse_config(int argc, char** argv, size_t chars_read) {
	_separator = ',';
	_count_only = false;

	size_t n_patterns = 0;
	char ** columns = malloc(sizeof(char*));
	char ** patterns = malloc(sizeof(char*));
	size_t * column_lengths = malloc(sizeof(size_t*));

	char c;
	while ((c = getopt (argc, argv, "s:p:c")) != -1) {
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
				if (optarg[0] == '\'' || optarg[0] == '\"') {
					optarg++;
					optarg[strlen(optarg)-2] = '\0';
				}
				LOG_V("Got pattern: %s\n", optarg);
				columns[n_patterns - 1] = strtok(optarg, "/");
				patterns[n_patterns - 1] = strtok(NULL, "/");
				column_lengths[n_patterns - 1] = strlen(columns[n_patterns - 1]);
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

	_tokenizer = setup_tokenizer(_separator, _buffer, _cell_starts, _cell_lengths, CELL_BUFFER_SIZE);

	size_t consumed, cells_found;
	bool last_full;
	tokenize_cells(_tokenizer, 0, chars_read, &consumed, &cells_found, &last_full);

	LOG_D("Processed: %zu, Cells: %zu\n", consumed, cells_found);
	debug_cells(cells_found);

	char const** current_cell = _cell_starts;
	size_t* current_cell_length = _cell_lengths;
	while (current_cell < (_cell_starts + cells_found) && *current_cell != NULL) {
		// also immediatly print the header
		if (current_cell != _cell_starts) {
			fwrite(&(_separator),sizeof(char),1, stdout);
		}
		fwrite(*current_cell, sizeof(char), *current_cell_length, stdout);
		current_cell++;
		current_cell_length++;
	}
	_column_count = (int)(current_cell - _cell_starts);

	const char* new_line = _cell_starts[_column_count-1] + _cell_lengths[_column_count - 1];
	_newline[0] = new_line[0];
	_newline_length = 1;
	if (new_line[1] == '\n' || new_line[0] == '\r') {
		_newline[1] = '\n';
		_newline_length = 2;
	}
	fwrite(_newline, sizeof(char), _newline_length, stdout);

	_patterns = calloc(sizeof(pcre*),_column_count);
	_patterns_extra = calloc(sizeof(pcre_extra*),_column_count);
	memset(_patterns, 0, sizeof(pcre*) * _column_count);
	memset(_patterns_extra, 0, sizeof(pcre_extra*) * _column_count);
	for (int c = 0; c < _column_count; c++) {
		for (size_t pat = 0;  pat < n_patterns; pat++) {
			if (_cell_lengths[c] == column_lengths[pat]) {
				if (strncmp(_cell_starts[c], columns[pat], column_lengths[pat])==0) {
					LOG_V("Adding pattern %s for column: %s (%d)\n", patterns[pat], columns[pat],c);
					// we have found the column
					const char *pcreErrorStr;
					int pcreErrorOffset;
					_patterns[c] = pcre_compile(patterns[pat],PCRE_DOLLAR_ENDONLY |  PCRE_DOTALL | PCRE_NO_UTF8_CHECK, &pcreErrorStr, &pcreErrorOffset, NULL); 
					if(_patterns[c] == NULL) {
						fprintf(stderr, "ERROR: Could not compile '%s': %s\n", patterns[pat], pcreErrorStr);
						exit(1);
					}
#ifdef SUPPORT_JIT
					_patterns_extra[c] = pcre_study(_patterns[c], PCRE_STUDY_JIT_COMPILE, &pcreErrorStr);
#else
					_patterns_extra[c] = pcre_study(_patterns[c], 0, &pcreErrorStr);
#endif
					if(_patterns_extra[c] == NULL) {
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

static char const* unquote(char const* quoted, size_t* length);

static void output_cells(size_t cells_found, size_t offset, bool last_full) {
	LOG_D("Starting output: %zu (%d)\n", cells_found, last_full);
	LOG_V("Entry: current_cell: %d\n", _current_cell_id);
	char const ** current_cell_start = _cell_starts + offset;
	char const ** cell_starts_end = _cell_starts + cells_found;
	size_t* current_cell_length = _cell_lengths + offset;
	bool matches = true;
	if (_half_line) {
		matches = _prev_matches;
	}
	bool first_line = true;
	char const* current_line_start = *current_cell_start;
	size_t current_line_length = 0;

	while (current_cell_start < cell_starts_end) {
		if (_current_cell_id > _column_count) {
			fprintf(stderr, "Too many cells in this row, expect: %d, got: %d (cell: %zu)\n", _column_count, _current_cell_id, (size_t)(current_cell_start - _cell_starts));
			exit(1);
			return;
		}
		if (*current_cell_start == NULL) {
			if (_current_cell_id == _column_count) {
				// end of the line
				if (matches) {
					if (first_line && _half_line) {
						fwrite(_prev_line, sizeof(char), _prev_line_length, stdout);
						first_line = false;
						_prev_line_length = 0;
					}
					fwrite(current_line_start, sizeof(char), current_line_length, stdout);
					fwrite(_newline, sizeof(char), _newline_length, stdout);
				}
				if (first_line) {
					first_line = false;
				}
				current_line_start = *(current_cell_start + 1);
				current_line_length = 0;
				_current_cell_id = -1;
				matches = true;
			}
			else if (_current_cell_id < _column_count) {
				fprintf(stderr, "Not enough cells in this row, expect: %d, got: %d (cell %zu)\n", _column_count, _current_cell_id,  (size_t)(current_cell_start - _cell_starts));
				exit(1);
				return;
			}
		}
		else if (matches) { // only if we have a match does it make sense to test other cells
			current_line_length += 1 + *current_cell_length;
			if (_current_cell_id == 0 || current_cell_start == (_cell_starts + offset)) {
				current_line_length--; // the first doesn't have a separator
			}
			if (_patterns[_current_cell_id] != NULL) {
				char const* cell = *current_cell_start;
				size_t length = *current_cell_length;
				if (current_cell_start == (cell_starts_end-1) && !last_full) {
					// we do not have the full cell at the moment, let's copy it
					size_t old_cell_length = _prev_cell_length;
					_prev_cell_length += *current_cell_length;
					memcpy(_prev_cell + old_cell_length, *current_cell_start, sizeof(char) * (*current_cell_length));
					_half_cell = true;
					_current_cell_id++;
					break;
				}
				if (_half_cell && current_cell_start == _cell_starts) {
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
					char const* c = cell-1;
					char const* cell_end = cell + length;
					while (++c < cell_end && *c != '"');
					if (c != cell_end) {
						// we have nested quotes
						cell = unquote(cell, &length);
					}
				}
				int ovector[255];
				int matchResult = pcre_exec(_patterns[_current_cell_id], _patterns_extra[_current_cell_id], cell, length, 0, 0, ovector, 255);
				matches &= matchResult >= 0;
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
		current_cell_start++;
		current_cell_length++;
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
static char const * unquote(char const* quoted, size_t* length) {
	char * result = _unquote_buffer;
	char const * current_char = quoted; // skip "
	char const * char_end = quoted + *length; // skip "
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
