#ifndef PV_UTIL_H
#define PV_UTIL_H

#include <stddef.h>
#include <stdint.h>

/* Print "parquet_viewer: <msg>" to stderr and exit(1). */
void die(const char *msg);

/* Pointer to the file name part of a path (handles '/' and, on Windows, '\\').
 * Returns a pointer into the input; does not allocate. */
const char *basename_of(const char *path);

/* Format a byte count as a human-readable size into out (e.g. "1.50 MB"). */
void human_size(uint64_t size, char *out, size_t n);

/* ---- UTF-8 / terminal display width ------------------------------------
 *
 * The renderer lays out fixed-width columns in *terminal cells*, not bytes.
 * These helpers translate between the UTF-8 byte stream and the number of
 * columns it occupies so that multi-byte text (Cyrillic, CJK, Devanagari,
 * emoji, ...) stays aligned. */

/* Decode one UTF-8 code point at s (NUL-terminated). Stores it in *cp and
 * returns the number of bytes consumed (>= 1). An invalid lead/continuation
 * byte consumes a single byte and yields U+FFFD. */
size_t utf8_decode(const char *s, uint32_t *cp);

/* Terminal columns for one code point: 0 (combining/zero-width), 2 (wide
 * East-Asian / emoji) or 1 (everything else, including controls). */
int cp_width(uint32_t cp);

/* Total terminal columns occupied by a UTF-8 string. */
int display_width(const char *s);

/* Byte length of the longest prefix of s that fits within max_cols terminal
 * columns without splitting a code point. The columns actually used are
 * written to *used (<= max_cols; may be less when a wide glyph would not
 * fit in the final column). *used may be NULL. */
size_t utf8_prefix_bytes(const char *s, int max_cols, int *used);

#endif /* PV_UTIL_H */
