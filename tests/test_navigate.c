#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <setjmp.h>

#include <cmocka.h>

#include "navigate.h"

static void test_empty_resets(void **state) {
    (void)state;
    int64_t cur = 7, top = 3;
    nav_clamp(&cur, &top, 0, 10);
    assert_int_equal((int)cur, 0);
    assert_int_equal((int)top, 0);
}

static void test_cur_clamped_to_range(void **state) {
    (void)state;
    int64_t cur = 100, top = 0;
    nav_clamp(&cur, &top, 10, 5);
    assert_int_equal((int)cur, 9);

    cur = -4;
    top = 0;
    nav_clamp(&cur, &top, 10, 5);
    assert_int_equal((int)cur, 0);
}

static void test_scroll_down_moves_top(void **state) {
    (void)state;
    int64_t cur = 5, top = 0;
    nav_clamp(&cur, &top, 100, 3);
    /* viewport of 3 rows must end at cur: top = 5 - 3 + 1 */
    assert_int_equal((int)top, 3);
    assert_int_equal((int)cur, 5);
}

static void test_scroll_up_moves_top(void **state) {
    (void)state;
    int64_t cur = 2, top = 5;
    nav_clamp(&cur, &top, 100, 3);
    assert_int_equal((int)top, 2);
    assert_int_equal((int)cur, 2);
}

static void test_last_row_viewport(void **state) {
    (void)state;
    int64_t cur = 9, top = 0;
    nav_clamp(&cur, &top, 10, 3);
    assert_int_equal((int)cur, 9);
    assert_int_equal((int)top, 7);
}

/* When the viewport is larger than the data set, top pins to 0. */
static void test_viewport_larger_than_total(void **state) {
    (void)state;
    int64_t cur = 2, top = 0;
    nav_clamp(&cur, &top, 5, 100);
    assert_int_equal((int)cur, 2);
    assert_int_equal((int)top, 0);
}

/* A single row: both indices collapse to 0 regardless of starting values. */
static void test_single_row(void **state) {
    (void)state;
    int64_t cur = 9, top = 9;
    nav_clamp(&cur, &top, 1, 10);
    assert_int_equal((int)cur, 0);
    assert_int_equal((int)top, 0);
}

/* A stale top past the end is pulled back so the cursor stays visible. */
static void test_stale_top_clamped(void **state) {
    (void)state;
    int64_t cur = 4, top = 999;
    nav_clamp(&cur, &top, 10, 3);
    assert_int_equal((int)cur, 4);
    assert_true(top <= cur);
    assert_true(cur < top + 3);
    assert_true(top >= 0 && top <= 9);
}

/* Already-consistent state is left untouched. */
static void test_idempotent_when_in_range(void **state) {
    (void)state;
    int64_t cur = 5, top = 4;
    nav_clamp(&cur, &top, 100, 3);
    assert_int_equal((int)cur, 5);
    assert_int_equal((int)top, 4);
}

static void test_view_count(void **state) {
    (void)state;
    App a = {0};
    a.row_count = 1234;
    a.ncols = 7;
    a.view = VIEW_DATA;
    assert_int_equal((int)view_count(&a), 1234);
    a.view = VIEW_SCHEMA;
    assert_int_equal((int)view_count(&a), 7);
    a.view = VIEW_STATS;
    assert_int_equal((int)view_count(&a), 7);
}

/* An empty DATA view reports zero items and clamps the cursor to 0. */
static void test_empty_data_view(void **state) {
    (void)state;
    App a = {0};
    a.view = VIEW_DATA;
    a.row_count = 0;
    a.cur = 5;
    a.top = 3;
    assert_int_equal((int)view_count(&a), 0);
    clamp_scroll(&a);
    assert_int_equal((int)a.cur, 0);
    assert_int_equal((int)a.top, 0);
}

static void test_set_view_resets(void **state) {
    (void)state;
    App a = {0};
    a.cur = 50;
    a.top = 40;
    a.col_off = 3;
    a.view = VIEW_DATA;
    set_view(&a, VIEW_STATS);
    assert_int_equal(a.view, VIEW_STATS);
    assert_int_equal((int)a.cur, 0);
    assert_int_equal((int)a.top, 0);
    assert_int_equal(a.col_off, 0);
}

/* move_cur/goto_row depend on visible_rows() (terminal size), so assert the
 * viewport invariants rather than exact top values. */
static void test_goto_and_move_invariants(void **state) {
    (void)state;
    App a = {0};
    a.view = VIEW_DATA;
    a.row_count = 100;
    int rows = visible_rows();
    assert_true(rows >= 1);

    goto_row(&a, 50);
    assert_int_equal((int)a.cur, 50);
    assert_true(a.top <= a.cur);
    assert_true(a.cur < a.top + rows);

    goto_row(&a, 1000); /* past the end clamps to last row */
    assert_int_equal((int)a.cur, 99);
    assert_true(a.top <= a.cur && a.cur < a.top + rows);

    move_cur(&a, -1000); /* before the start clamps to the top */
    assert_int_equal((int)a.cur, 0);
    assert_int_equal((int)a.top, 0);

    move_cur(&a, 3);
    assert_int_equal((int)a.cur, 3);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_empty_resets),
        cmocka_unit_test(test_cur_clamped_to_range),
        cmocka_unit_test(test_scroll_down_moves_top),
        cmocka_unit_test(test_scroll_up_moves_top),
        cmocka_unit_test(test_last_row_viewport),
        cmocka_unit_test(test_viewport_larger_than_total),
        cmocka_unit_test(test_single_row),
        cmocka_unit_test(test_stale_top_clamped),
        cmocka_unit_test(test_idempotent_when_in_range),
        cmocka_unit_test(test_view_count),
        cmocka_unit_test(test_empty_data_view),
        cmocka_unit_test(test_set_view_resets),
        cmocka_unit_test(test_goto_and_move_invariants),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
