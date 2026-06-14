#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>

#include <cmocka.h>

#include "buf.h"

static void test_append_and_str(void **state) {
    (void)state;
    Buf b = {0};
    buf_str(&b, "hello");
    buf_append(&b, " ", 1);
    buf_str(&b, "world");
    assert_int_equal((int)b.len, 11);
    assert_string_equal(b.data, "hello world");
    buf_free(&b);
    assert_null(b.data);
    assert_int_equal((int)b.len, 0);
    assert_int_equal((int)b.cap, 0);
}

static void test_fmt_small(void **state) {
    (void)state;
    Buf b = {0};
    buf_fmt(&b, "%d-%s", 42, "x");
    assert_string_equal(b.data, "42-x");
    buf_free(&b);
}

/* Exercise the > 1024-byte heap path inside buf_fmt. */
static void test_fmt_large(void **state) {
    (void)state;
    char big[2001];
    memset(big, 'a', sizeof(big) - 1);
    big[sizeof(big) - 1] = '\0';

    Buf b = {0};
    buf_fmt(&b, "%s", big);
    assert_int_equal((int)b.len, 2000);
    assert_string_equal(b.data, big);
    buf_free(&b);
}

/* buf_fmt switches from a 1024-byte stack buffer to the heap at n >= 1024.
 * Exercise both sides of that boundary. */
static void test_fmt_boundary(void **state) {
    (void)state;
    char s1023[1024];
    memset(s1023, 'b', 1023);
    s1023[1023] = '\0';
    Buf b1 = {0};
    buf_fmt(&b1, "%s", s1023); /* n == 1023, stack path */
    assert_int_equal((int)b1.len, 1023);
    assert_string_equal(b1.data, s1023);
    buf_free(&b1);

    char s1024[1025];
    memset(s1024, 'c', 1024);
    s1024[1024] = '\0';
    Buf b2 = {0};
    buf_fmt(&b2, "%s", s1024); /* n == 1024, heap path */
    assert_int_equal((int)b2.len, 1024);
    assert_string_equal(b2.data, s1024);
    buf_free(&b2);
}

/* Appending zero bytes is a no-op but still NUL-terminates. */
static void test_append_zero(void **state) {
    (void)state;
    Buf b = {0};
    buf_str(&b, "ab");
    buf_append(&b, "ignored", 0);
    assert_int_equal((int)b.len, 2);
    assert_string_equal(b.data, "ab");
    buf_free(&b);
}

/* buf_reserve is a no-op once capacity is sufficient (pointer unchanged). */
static void test_reserve_noop(void **state) {
    (void)state;
    Buf b = {0};
    buf_str(&b, "seed");
    char *before = b.data;
    size_t cap_before = b.cap;
    buf_reserve(&b, 1); /* well within existing capacity */
    assert_ptr_equal(b.data, before);
    assert_int_equal((int)b.cap, (int)cap_before);
    buf_free(&b);
}

/* Multi-byte UTF-8 is stored verbatim; len counts bytes, not code points. */
static void test_append_utf8(void **state) {
    (void)state;
    Buf b = {0};
    buf_str(&b, "zażółć"); /* 'z','a' + 4 two-byte chars = 10 bytes */
    assert_int_equal((int)b.len, 10);
    assert_string_equal(b.data, "zażółć");
    buf_free(&b);
}

/* Many appends force several reallocations; content must stay intact. */
static void test_growth(void **state) {
    (void)state;
    Buf b = {0};
    for (int i = 0; i < 5000; i++) {
        buf_str(&b, "x");
    }
    assert_int_equal((int)b.len, 5000);
    assert_true(b.cap > b.len);
    for (int i = 0; i < 5000; i++) {
        assert_int_equal(b.data[i], 'x');
    }
    assert_int_equal(b.data[5000], '\0');
    buf_free(&b);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_append_and_str),
        cmocka_unit_test(test_fmt_small),
        cmocka_unit_test(test_fmt_large),
        cmocka_unit_test(test_fmt_boundary),
        cmocka_unit_test(test_append_zero),
        cmocka_unit_test(test_reserve_noop),
        cmocka_unit_test(test_append_utf8),
        cmocka_unit_test(test_growth),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
