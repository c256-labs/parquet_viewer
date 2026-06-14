#ifndef PV_BUF_H
#define PV_BUF_H

#include <stddef.h>

/* Small growable string buffer (one allocation per frame, flushed once). */
typedef struct {
    char *data;
    size_t len;
    size_t cap;
} Buf;

void buf_reserve(Buf *b, size_t extra);
void buf_append(Buf *b, const char *s, size_t n);
void buf_str(Buf *b, const char *s);
void buf_fmt(Buf *b, const char *fmt, ...)
    __attribute__((__format__(__printf__, 2, 3)));
void buf_free(Buf *b);

#endif /* PV_BUF_H */
