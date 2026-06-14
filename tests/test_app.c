#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <setjmp.h>
#include <string.h>

#include <cmocka.h>

#include "app.h"
#include "model.h"

static void set_cmd(App *a, const char *s) {
    strncpy(a->cmd, s, sizeof(a->cmd) - 1);
    a->cmd[sizeof(a->cmd) - 1] = '\0';
}

static void test_quit(void **state) {
    (void)state;
    App a = {0};
    set_cmd(&a, "q");
    assert_int_equal(run_command(&a), 1);
    set_cmd(&a, "quit");
    assert_int_equal(run_command(&a), 1);
}

static void test_view_commands(void **state) {
    (void)state;
    App a = {0};
    a.row_count = 10;
    a.ncols = 5;
    a.view = VIEW_DATA;
    a.cur = 7;

    set_cmd(&a, "schema");
    assert_int_equal(run_command(&a), 0);
    assert_int_equal(a.view, VIEW_SCHEMA);
    assert_int_equal((int)a.cur, 0); /* set_view resets the cursor */

    set_cmd(&a, "stats");
    run_command(&a);
    assert_int_equal(a.view, VIEW_STATS);

    set_cmd(&a, "data");
    run_command(&a);
    assert_int_equal(a.view, VIEW_DATA);
}

static void test_leading_spaces(void **state) {
    (void)state;
    App a = {0};
    a.ncols = 3;
    set_cmd(&a, "   schema");
    assert_int_equal(run_command(&a), 0);
    assert_int_equal(a.view, VIEW_SCHEMA);
}

static void test_goto_numeric(void **state) {
    (void)state;
    App a = {0};
    a.view = VIEW_DATA;
    a.row_count = 100;

    set_cmd(&a, "5");
    assert_int_equal(run_command(&a), 0);
    assert_int_equal((int)a.cur, 4); /* input is 1-based */

    set_cmd(&a, "0");
    run_command(&a);
    assert_int_equal((int)a.cur, 0);
}

static void test_goto_one_based_and_clamp(void **state) {
    (void)state;
    App a = {0};
    a.view = VIEW_DATA;
    a.row_count = 100;

    set_cmd(&a, "1"); /* first row is index 0 */
    run_command(&a);
    assert_int_equal((int)a.cur, 0);

    set_cmd(&a, "500"); /* past the end clamps to the last row */
    run_command(&a);
    assert_int_equal((int)a.cur, 99);
}

/* strtoll stops at the first non-digit, so "3x" jumps to row 3 (index 2). */
static void test_goto_trailing_junk(void **state) {
    (void)state;
    App a = {0};
    a.view = VIEW_DATA;
    a.row_count = 100;
    set_cmd(&a, "3x");
    assert_int_equal(run_command(&a), 0);
    assert_int_equal((int)a.cur, 2);
}

/* Commands are case-sensitive: "Q"/"DATA" are not recognised. */
static void test_case_sensitive(void **state) {
    (void)state;
    App a = {0};
    set_cmd(&a, "Q");
    assert_int_equal(run_command(&a), 0);
    assert_non_null(strstr(a.status, "unknown command"));
}

/* Only leading spaces are stripped; a trailing space is part of the token. */
static void test_trailing_space_not_stripped(void **state) {
    (void)state;
    App a = {0};
    a.ncols = 3;
    a.view = VIEW_DATA;
    set_cmd(&a, "schema ");
    assert_int_equal(run_command(&a), 0);
    assert_int_equal(a.view, VIEW_DATA); /* unchanged */
    assert_non_null(strstr(a.status, "unknown command"));
}

/* :<n> works in SCHEMA view too, clamping against the column count. */
static void test_goto_in_schema_view(void **state) {
    (void)state;
    App a = {0};
    a.view = VIEW_SCHEMA;
    a.ncols = 4;
    set_cmd(&a, "99");
    run_command(&a);
    assert_int_equal((int)a.cur, 3); /* last column index */
}

static void test_unknown(void **state) {
    (void)state;
    App a = {0};
    set_cmd(&a, "bogus");
    assert_int_equal(run_command(&a), 0);
    assert_non_null(strstr(a.status, "unknown command"));
}

static void test_empty(void **state) {
    (void)state;
    App a = {0};
    a.status[0] = '\0';
    set_cmd(&a, "");
    assert_int_equal(run_command(&a), 0);
    assert_int_equal((int)strlen(a.status), 0);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_quit),
        cmocka_unit_test(test_view_commands),
        cmocka_unit_test(test_leading_spaces),
        cmocka_unit_test(test_goto_numeric),
        cmocka_unit_test(test_goto_one_based_and_clamp),
        cmocka_unit_test(test_goto_trailing_junk),
        cmocka_unit_test(test_case_sensitive),
        cmocka_unit_test(test_trailing_space_not_stripped),
        cmocka_unit_test(test_goto_in_schema_view),
        cmocka_unit_test(test_unknown),
        cmocka_unit_test(test_empty),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
