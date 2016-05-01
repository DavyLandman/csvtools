#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <string.h>
#include "csv_tokenizer.h"
#include "debug.h"

#define CELL_BUFFER_SIZE (BUFFER_SIZE / 2) + 2
struct csv_tokenizer* _tokenizer;
static char _buffer[BUFFER_SIZE + BUFFER_TOKENIZER_POSTFIX];
static Cell _cells[CELL_BUFFER_SIZE];

int main(int argc, char** argv) {
    (void)argv;
    if (argc > 1) {
        fprintf(stderr, "This tool is for testing only, pipe a csv into it\n");
        return 0;
    }
    size_t chars_read;
    uint64_t cell_total = 0;
    _tokenizer = setup_tokenizer(',', _buffer, _cells, CELL_BUFFER_SIZE);
    while ((chars_read = fread(_buffer, 1, BUFFER_SIZE, stdin)) > 0) {
        LOG_D("New data read: %zu\n", chars_read);
        prepare_tokenization(_tokenizer, _buffer, chars_read);
        size_t buffer_consumed = 0;
        size_t cells_found = 0;
        bool last_full = true;

        while (buffer_consumed < chars_read) {
            tokenize_cells(_tokenizer, buffer_consumed, chars_read, &buffer_consumed, &cells_found, &last_full);
            LOG_D("Processed: %zu, Cells: %zu\n", buffer_consumed, cells_found);
            cell_total += cells_found;
            if (!last_full) {
                cell_total--;
            }
        }
    }
    fprintf(stdout, "%llu cells\n", cell_total);
    return 0;
}
