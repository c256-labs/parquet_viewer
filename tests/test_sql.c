#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <setjmp.h>
#include <stdlib.h>

#include <cmocka.h>

#include "sql.h"

static void test_literal_plain(void **state) {
    (void)state;
    char *s = sql_literal("abc");
    assert_string_equal(s, "'abc'");
    free(s);
}

static void test_literal_empty(void **state) {
    (void)state;
    char *s = sql_literal("");
    assert_string_equal(s, "''");
    free(s);
}

static void test_literal_quote_doubled(void **state) {
    (void)state;
    char *s = sql_literal("O'Brien");
    assert_string_equal(s, "'O''Brien'");
    free(s);
}

static void test_literal_only_quotes(void **state) {
    (void)state;
    char *s = sql_literal("''");
    assert_string_equal(s, "''''''");
    free(s);
}

static void test_ident_plain(void **state) {
    (void)state;
    char *s = quote_ident("col");
    assert_string_equal(s, "\"col\"");
    free(s);
}

static void test_ident_quote_doubled(void **state) {
    (void)state;
    char *s = quote_ident("we\"ird");
    assert_string_equal(s, "\"we\"\"ird\"");
    free(s);
}

/* A backslash is not special to SQL string literals; it passes through. */
static void test_literal_backslash_passthrough(void **state) {
    (void)state;
    char *s = sql_literal("C:\\tmp\\a");
    assert_string_equal(s, "'C:\\tmp\\a'");
    free(s);
}

/* Multi-byte UTF-8 must survive escaping byte-for-byte (only ' is doubled). */
static void test_literal_utf8_passthrough(void **state) {
    (void)state;
    char *s = sql_literal("zażółć");
    assert_string_equal(s, "'zażółć'");
    free(s);
}

/* A realistic path with an apostrophe and UTF-8: both behaviours combine. */
static void test_literal_utf8_with_quote(void **state) {
    (void)state;
    char *s = sql_literal("kraj'ść.parquet");
    assert_string_equal(s, "'kraj''ść.parquet'");
    free(s);
}

static void test_ident_empty(void **state) {
    (void)state;
    char *s = quote_ident("");
    assert_string_equal(s, "\"\"");
    free(s);
}

/* A column name that is entirely double quotes is fully doubled. */
static void test_ident_only_quotes(void **state) {
    (void)state;
    char *s = quote_ident("\"\"");
    assert_string_equal(s, "\"\"\"\"\"\"");
    free(s);
}

static void test_ident_utf8_passthrough(void **state) {
    (void)state;
    char *s = quote_ident("名前");
    assert_string_equal(s, "\"名前\"");
    free(s);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_literal_plain),
        cmocka_unit_test(test_literal_empty),
        cmocka_unit_test(test_literal_quote_doubled),
        cmocka_unit_test(test_literal_only_quotes),
        cmocka_unit_test(test_literal_backslash_passthrough),
        cmocka_unit_test(test_literal_utf8_passthrough),
        cmocka_unit_test(test_literal_utf8_with_quote),
        cmocka_unit_test(test_ident_plain),
        cmocka_unit_test(test_ident_quote_doubled),
        cmocka_unit_test(test_ident_empty),
        cmocka_unit_test(test_ident_only_quotes),
        cmocka_unit_test(test_ident_utf8_passthrough),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
