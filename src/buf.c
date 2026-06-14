#include "buf.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void buf_reserve(Buf *b, size_t extra) {
    if (b->len + extra + 1 <= b->cap) {
        return;
    }
    size_t cap = b->cap ? b->cap : 4096;
    while (b->len + extra + 1 > cap) {
        cap *= 2;
    }
    b->data = (char *)realloc(b->data, cap);
    b->cap = cap;
}

void buf_append(Buf *b, const char *s, size_t n) {
    buf_reserve(b, n);
    memcpy(b->data + b->len, s, n);
    b->len += n;
    b->data[b->len] = '\0';
}

void buf_str(Buf *b, const char *s) {
    buf_append(b, s, strlen(s));
}

void buf_fmt(Buf *b, const char *fmt, ...) {
    char tmp[1024];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);
    if (n < 0) {
        return;
    }
    if ((size_t)n < sizeof(tmp)) {
        buf_append(b, tmp, (size_t)n);
        return;
    }
    char *big = (char *)malloc((size_t)n + 1);
    va_start(ap, fmt);
    vsnprintf(big, (size_t)n + 1, fmt, ap);
    va_end(ap);
    buf_append(b, big, (size_t)n);
    free(big);
}

void buf_free(Buf *b) {
    free(b->data);
    b->data = NULL;
    b->len = b->cap = 0;
}
