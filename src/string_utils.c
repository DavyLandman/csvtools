#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
char **strsplit(const char* str, const char* delim, size_t* numtokens) {
    char *s = strdup(str);
    size_t tokens_alloc = 1;
    size_t tokens_used = 0;
    char **tokens = calloc(tokens_alloc, sizeof(char*));
    char *token, *rest = s;

    while ((token = strsep(&rest, delim)) != NULL) {
        if (tokens_used == tokens_alloc) {
            tokens_alloc *= 2;
            tokens = realloc(tokens, tokens_alloc * sizeof(char*));
        }
        tokens[tokens_used++] = strdup(token);
    }

    if (tokens_used == 0) {
        free(tokens);
        tokens = NULL;
    } else {
        tokens = realloc(tokens, tokens_used * sizeof(char*));
    }
    *numtokens = tokens_used;
    free(s);

    return tokens;
}

int *to_int(char** strs, size_t number_strs) {
	int* result = calloc(sizeof(int), number_strs);
	for (size_t i = 0; i < number_strs; i++) {
		result[i] = atoi(strs[i]);
	}
	return result;
}
bool contains(const char** list, size_t list_size, const char* needle) {
	for (size_t l=0; l < list_size; l++) {
		if (strcmp(needle, list[l]) == 0) {
			return true;
		}
	}
	return false;
}

bool contains_n(const char** list, size_t list_size, const char* needle, size_t needle_size) {
	char* s = calloc(sizeof(char), needle_size + 2);
	s[needle_size + 1] = '\0';
	memcpy(s, needle, needle_size);
	bool result = contains(list, list_size, s);
	free(s);
	return result;
}

