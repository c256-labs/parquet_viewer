#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <setjmp.h>
#include <string.h>

#include <cmocka.h>

#include "util.h"

static void test_human_size_bytes(void **state) {
    (void)state;
    char out[32];
    human_size(0, out, sizeof(out));
    assert_string_equal(out, "0 B");
    human_size(512, out, sizeof(out));
    assert_string_equal(out, "512 B");
    human_size(1023, out, sizeof(out));
    assert_string_equal(out, "1023 B");
}

static void test_human_size_scaled(void **state) {
    (void)state;
    char out[32];
    human_size(1024, out, sizeof(out));
    assert_string_equal(out, "1.00 KB");
    human_size(1536, out, sizeof(out));
    assert_string_equal(out, "1.50 KB");
    human_size(1048576, out, sizeof(out));
    assert_string_equal(out, "1.00 MB");
    human_size((uint64_t)1024 * 1024 * 1024, out, sizeof(out));
    assert_string_equal(out, "1.00 GB");
}

/* Large magnitudes: TB, PB and the clamp at PB (u stops growing past index 5). */
static void test_human_size_large(void **state) {
    (void)state;
    char out[32];
    human_size((uint64_t)1024 * 1024 * 1024 * 1024, out, sizeof(out));
    assert_string_equal(out, "1.00 TB");
    human_size((uint64_t)1024 * 1024 * 1024 * 1024 * 1024, out, sizeof(out));
    assert_string_equal(out, "1.00 PB");
    /* 1024 PB does not roll over to a 7th unit; it stays in PB. */
    human_size((uint64_t)1024 * 1024 * 1024 * 1024 * 1024 * 1024, out,
               sizeof(out));
    assert_string_equal(out, "1024.00 PB");
}

/* Values just under the next unit round to two decimals. */
static void test_human_size_rounding(void **state) {
    (void)state;
    char out[32];
    human_size(1234, out, sizeof(out)); /* 1.205 KB -> 1.21 KB */
    assert_string_equal(out, "1.21 KB");
}

/* A tiny output buffer must be truncated safely (snprintf bounds the write). */
static void test_human_size_small_buffer(void **state) {
    (void)state;
    char out[4];
    human_size(1048576, out, sizeof(out)); /* would be "1.00 MB" */
    assert_true(strlen(out) < sizeof(out));
}

static void test_basename(void **state) {
    (void)state;
    assert_string_equal(basename_of("/a/b/c.parquet"), "c.parquet");
    assert_string_equal(basename_of("file.parquet"), "file.parquet");
    assert_string_equal(basename_of("./x.parquet"), "x.parquet");
    /* A trailing slash yields an empty basename. */
    assert_string_equal(basename_of("/a/b/"), "");
}

static void test_basename_edge(void **state) {
    (void)state;
    assert_string_equal(basename_of(""), "");
    assert_string_equal(basename_of("/"), "");
    /* Consecutive slashes: only the part after the last one is kept. */
    assert_string_equal(basename_of("a//b.parquet"), "b.parquet");
    /* A UTF-8 file name passes through unchanged. */
    assert_string_equal(basename_of("/dane/zażółć.parquet"),
                        "zażółć.parquet");
}

static void test_utf8_decode(void **state) {
    (void)state;
    uint32_t cp;
    assert_int_equal((int)utf8_decode("A", &cp), 1);
    assert_int_equal((int)cp, 0x41);
    assert_int_equal((int)utf8_decode("ż", &cp), 2); /* U+017C */
    assert_int_equal((int)cp, 0x17C);
    assert_int_equal((int)utf8_decode("視", &cp), 3); /* U+8996 */
    assert_int_equal((int)cp, 0x8996);
    assert_int_equal((int)utf8_decode("👋", &cp), 4); /* U+1F44B */
    assert_int_equal((int)cp, 0x1F44B);
    /* A stray continuation byte is consumed as one byte -> U+FFFD. */
    assert_int_equal((int)utf8_decode("\xff", &cp), 1);
    assert_int_equal((int)cp, 0xFFFD);
}

static void test_cp_width(void **state) {
    (void)state;
    assert_int_equal(cp_width('A'), 1);
    assert_int_equal(cp_width(0x0301), 0);  /* combining acute accent  */
    assert_int_equal(cp_width(0x094D), 0);  /* Devanagari virama       */
    assert_int_equal(cp_width(0x8996), 2);  /* CJK 視                  */
    assert_int_equal(cp_width(0x1F44B), 2); /* waving hand emoji       */
}

static void test_display_width(void **state) {
    (void)state;
    assert_int_equal(display_width(""), 0);
    assert_int_equal(display_width("abc"), 3);
    /* Polish uses precomposed Latin letters: one column each (17 total). */
    assert_int_equal(display_width("zażółć gęślą jaźń"), 17);
    /* Cyrillic letters are single-width. */
    assert_int_equal(display_width("Съешь"), 5);
    /* CJK: 11 characters at two columns each. */
    assert_int_equal(display_width("視野無限廣，窗外有藍天"), 22);
    /* Emoji counts as two columns. */
    assert_int_equal(display_width("Hello 👋"), 8);
    /* base letter + combining mark = one column. */
    assert_int_equal(display_width("e\xcc\x81"), 1);
}

static void test_utf8_prefix_bytes(void **state) {
    (void)state;
    int used = 0;
    /* ASCII: trivial 1 byte = 1 column. */
    assert_int_equal((int)utf8_prefix_bytes("hello", 3, &used), 3);
    assert_int_equal(used, 3);
    /* A wide glyph is never split across the column budget: 視(2)+野 would be
     * 4 cols, so a budget of 3 keeps only 視 (3 bytes, 2 columns). */
    assert_int_equal((int)utf8_prefix_bytes("視野", 3, &used), 3);
    assert_int_equal(used, 2);
    /* When the next glyph is wide and only one column remains, stop early. */
    assert_int_equal((int)utf8_prefix_bytes("a視b", 2, &used), 1);
    assert_int_equal(used, 1);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_human_size_bytes),
        cmocka_unit_test(test_human_size_scaled),
        cmocka_unit_test(test_human_size_large),
        cmocka_unit_test(test_human_size_rounding),
        cmocka_unit_test(test_human_size_small_buffer),
        cmocka_unit_test(test_basename),
        cmocka_unit_test(test_basename_edge),
        cmocka_unit_test(test_utf8_decode),
        cmocka_unit_test(test_cp_width),
        cmocka_unit_test(test_display_width),
        cmocka_unit_test(test_utf8_prefix_bytes),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
