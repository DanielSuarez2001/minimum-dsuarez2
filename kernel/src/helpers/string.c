#include "string.h"

size_t strlen(const char *s) {
    size_t len = 0;
    while (*s != '\0') {
        len++;
        s++;
    }
    return len;
}
bool str_eq(const char *s1, const char *s2) {
    while (*s1 != '\0' && *s2 != '\0') {
        if (*s1 != *s2) {
            return false;
        }
        s1++;
        s2++;
    }
    return *s1 == '\0' && *s2 == '\0';
}

char * str_start_trim(char *s) {
    // Trim leading whitespace
    while (*s == ' ') {
        s++;
    }
    return s;
}

bool str_is_first_word(const char *s, const char *prefix) {
    while (*prefix != '\0') {
        if (*s != *prefix) {
            return false;
        }
        s++;
        prefix++;
    }
    if (*s == ' ' || *s == '\0') {
        return true;
    }
    return false;
}

char *str_split_on_space(char *s) {
    while (*s != '\0') {
        if (*s == ' ') {
            *s = '\0'; // Replace space with null terminator
            return s + 1; // Return pointer to the next character after the space
        }
        s++;
    }
    return NULL; // Space not found, return NULL
}

