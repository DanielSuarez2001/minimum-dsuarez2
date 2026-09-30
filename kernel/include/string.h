#ifndef STRING_H
#define STRING_H

#include <stdbool.h>
#include <stddef.h>

size_t strlen(const char *s);
bool str_eq(const char *s1, const char *s2);
char * str_start_trim(char *s);
bool str_is_first_word(const char *s, const char *prefix);
char *str_split_on_space(char *s);

#endif // STRING_H