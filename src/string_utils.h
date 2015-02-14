#pragma once
#include <stdbool.h>
char **strsplit(const char* str, const char* delim, size_t* numtokens);
int *to_int(char** strs, size_t number_strs);
bool contains(const char** list, size_t list_size, const char* needle);
bool contains_n(const char** list, size_t list_size, const char* needle, size_t needle_size);
