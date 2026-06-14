#include "sql.h"

#include <stdlib.h>
#include <string.h>

char *sql_literal(const char *s) {
    size_t n = strlen(s);
    char *out = (char *)malloc(n * 2 + 3);
    char *p = out;
    *p++ = '\'';
    for (size_t i = 0; i < n; i++) {
        if (s[i] == '\'') {
            *p++ = '\'';
        }
        *p++ = s[i];
    }
    *p++ = '\'';
    *p = '\0';
    return out;
}

char *quote_ident(const char *s) {
    size_t n = strlen(s);
    char *out = (char *)malloc(n * 2 + 3);
    char *p = out;
    *p++ = '"';
    for (size_t i = 0; i < n; i++) {
        if (s[i] == '"') {
            *p++ = '"';
        }
        *p++ = s[i];
    }
    *p++ = '"';
    *p = '\0';
    return out;
}
