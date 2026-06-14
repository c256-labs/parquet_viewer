/* UTF-8 / internationalization tests against testdata/i18n.parquet.
 *
 * The fixture holds Polish, Russian, Chinese and Hindi text, an emoji string,
 * a value with embedded SQL quotes, an empty string and several NULLs. These
 * tests assert that multi-byte text round-trips byte-for-byte through the
 * DuckDB read path (load_window), that NULL counts are correct (load_stats),
 * and that the renderer emits the UTF-8 bytes verbatim.
 *
 * The Parquet path comes from $PV_TEST_I18N, then a default relative to the
 * project root (works for `make test`). argv is intentionally ignored because
 * the Makefile passes the weather sample to every test binary.
 *
 * Regenerate the fixture with scripts/gen-testdata.sh.
 */
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <cmocka.h>

#include "duckdb.h"
#include "model.h"
#include "render.h"
#include "source.h"
#include "util.h"

/* Expected text, stored verbatim as UTF-8 in this source file. */
#define PL_GREETING "zażółć gęślą jaźń"
#define RU_GREETING "Съешь же ещё этих мягких французских булок, да выпей чаю"
#define ZH_GREETING "視野無限廣，窗外有藍天"
#define HI_GREETING \
    "ऋषियों को सताने वाले दुष्ट राक्षसों के राजा रावण का सर्वनाश करने वाले श्रीराम"
#define EMOJI_GREETING "Hello 👋 World 🌍 — café"
#define QUOTES_GREETING "O'Brien said \"hi\""

static const char *g_path = "testdata/i18n.parquet";

/* malloc'd VARCHAR for (col,row); "NULL" sentinel preserved as actual string. */
static char *cell_str(duckdb_result *res, idx_t col, idx_t row) {
    if (duckdb_value_is_null(res, col, row)) {
        return NULL;
    }
    char *v = duckdb_value_varchar(res, col, row);
    return v;
}

/* Find the data row index whose lang column equals `lang`. -1 if absent. */
static int find_row_by_lang(duckdb_result *res, const char *lang) {
    idx_t n = duckdb_row_count(res);
    for (idx_t r = 0; r < n; r++) {
        char *v = cell_str(res, 1, r);
        int hit = v && strcmp(v, lang) == 0;
        duckdb_free(v);
        if (hit) {
            return (int)r;
        }
    }
    return -1;
}

static void test_schema(void **state) {
    (void)state;
    App a = {0};
    open_parquet(&a, g_path);
    assert_int_equal(a.ncols, 5);
    assert_int_equal((int)a.row_count, 7);
    assert_string_equal(a.cols[0].name, "id");
    assert_string_equal(a.cols[1].name, "lang");
    assert_string_equal(a.cols[2].name, "greeting");
    assert_string_equal(a.cols[3].name, "note");
    assert_string_equal(a.cols[4].name, "value");
    assert_string_equal(a.cols[0].type, "BIGINT");
    assert_string_equal(a.cols[1].type, "VARCHAR");
    assert_string_equal(a.cols[4].type, "DOUBLE");
    close_parquet(&a);
}

/* The heart of the suite: every multi-byte string must survive intact. */
static void test_utf8_roundtrip(void **state) {
    (void)state;
    App a = {0};
    open_parquet(&a, g_path);

    duckdb_result res;
    assert_true(load_window(&a, 0, a.row_count, &res));
    assert_int_equal((int)duckdb_row_count(&res), 7);

    const struct {
        const char *lang;
        const char *greeting;
    } expect[] = {
        {"Polish", PL_GREETING},
        {"Russian", RU_GREETING},
        {"Chinese", ZH_GREETING},
        {"Hindi", HI_GREETING},
        {"Emoji", EMOJI_GREETING},
        {"Quotes", QUOTES_GREETING},
    };

    for (size_t i = 0; i < sizeof(expect) / sizeof(expect[0]); i++) {
        int row = find_row_by_lang(&res, expect[i].lang);
        assert_int_not_equal(row, -1);
        char *g = cell_str(&res, 2, (idx_t)row);
        assert_non_null(g);
        assert_string_equal(g, expect[i].greeting);
        /* No truncation/replacement: byte length must match exactly. */
        assert_int_equal((int)strlen(g), (int)strlen(expect[i].greeting));
        duckdb_free(g);
    }

    duckdb_destroy_result(&res);
    close_parquet(&a);
}

/* Empty strings stay empty (not turned into NULL) and keep their byte length. */
static void test_empty_string_row(void **state) {
    (void)state;
    App a = {0};
    open_parquet(&a, g_path);
    duckdb_result res;
    assert_true(load_window(&a, 0, a.row_count, &res));
    int row = find_row_by_lang(&res, "Empty");
    assert_int_not_equal(row, -1);
    char *g = cell_str(&res, 2, (idx_t)row);
    assert_non_null(g);
    assert_string_equal(g, "");
    duckdb_free(g);
    duckdb_destroy_result(&res);
    close_parquet(&a);
}

/* NULLs in the value/note columns are reported by load_stats. */
static void test_null_counts(void **state) {
    (void)state;
    App a = {0};
    open_parquet(&a, g_path);
    load_stats(&a);
    for (int c = 0; c < a.ncols; c++) {
        assert_true(a.cols[c].null_known);
        if (strcmp(a.cols[c].name, "note") == 0) {
            assert_int_equal((int)a.cols[c].null_count, 3);
        } else if (strcmp(a.cols[c].name, "value") == 0) {
            assert_int_equal((int)a.cols[c].null_count, 2);
        } else {
            assert_int_equal((int)a.cols[c].null_count, 0);
        }
    }
    close_parquet(&a);
}

/* The window honours LIMIT/OFFSET, so a single-row window starting at the
 * Polish row (id 1) returns exactly that row. */
static void test_window_offset(void **state) {
    (void)state;
    App a = {0};
    open_parquet(&a, g_path);
    duckdb_result res;
    assert_true(load_window(&a, 0, 1, &res));
    assert_int_equal((int)duckdb_row_count(&res), 1);
    char *id = cell_str(&res, 0, 0);
    assert_non_null(id);
    assert_string_equal(id, "1");
    duckdb_free(id);
    duckdb_destroy_result(&res);
    close_parquet(&a);
}

/* Render one view to a temp file and return its bytes (caller frees). */
static char *capture_render(App *a, View v) {
    a->view = v;
    a->top = 0;
    a->cur = 0;
    a->col_off = 0;
    fflush(stdout);
    int saved = dup(STDOUT_FILENO);
    FILE *tf = tmpfile();
    assert_non_null(tf);
    dup2(fileno(tf), STDOUT_FILENO);
    render(a);
    fflush(stdout);
    dup2(saved, STDOUT_FILENO);
    close(saved);
    fseek(tf, 0, SEEK_END);
    long sz = ftell(tf);
    fseek(tf, 0, SEEK_SET);
    char *buf = (char *)malloc((size_t)sz + 1);
    size_t rd = fread(buf, 1, (size_t)sz, tf);
    buf[rd] = '\0';
    fclose(tf);
    return buf;
}

/* The renderer must emit the multi-byte bytes verbatim for short strings that
 * fit within the column width (no escaping or substitution). */
static void test_render_emits_utf8(void **state) {
    (void)state;
    App a = {0};
    open_parquet(&a, g_path);
    char *out = capture_render(&a, VIEW_DATA);
    assert_non_null(strstr(out, "Polish"));
    assert_non_null(strstr(out, "Chinese"));
    assert_non_null(strstr(out, PL_GREETING)); /* 26 bytes, under COL_MAX */
    assert_non_null(strstr(out, ZH_GREETING)); /* 33 bytes, under COL_MAX */
    free(out);
    close_parquet(&a);
}

/* Box border glyph U+2502 (one display column). */
#define BX_V "\xe2\x94\x82"

/* Columns must line up regardless of the multi-byte content in each row: every
 * boxed table row must occupy exactly the terminal width (80 when stdout is
 * not a tty, as it is here) in *display columns*, not bytes. This is the
 * regression guard for the byte-vs-column misalignment. */
static void test_render_columns_aligned(void **state) {
    (void)state;
    App a = {0};
    open_parquet(&a, g_path);
    char *out = capture_render(&a, VIEW_DATA);

    /* Walk the output, dropping ANSI CSI escapes. A cursor-position escape
     * (CSI ... 'H') ends the current screen row. Rows that begin with the
     * vertical border are inside the table and must be exactly 80 columns. */
    char row[4096];
    size_t rlen = 0;
    int checked = 0;
    const char *p = out;
    for (;;) {
        int boundary = (*p == '\0');
        if (*p == '\x1b' && p[1] == '[') {
            const char *q = p + 2;
            while (*q && !(*q >= 0x40 && *q <= 0x7e)) {
                q++;
            }
            char final = *q;
            if (*q) {
                q++;
            }
            p = q;
            if (final != 'H') {
                continue; /* colour / clear-to-eol: skip, same row */
            }
            boundary = 1;
        }
        if (boundary) {
            row[rlen] = '\0';
            if (rlen >= 3 && memcmp(row, BX_V, 3) == 0) {
                assert_int_equal(display_width(row), 80);
                checked++;
            }
            rlen = 0;
            if (*p == '\0') {
                break;
            }
            continue;
        }
        if (rlen < sizeof(row) - 1) {
            row[rlen++] = *p;
        }
        p++;
    }

    assert_true(checked >= 2); /* header-columns row + >= 1 data row */
    free(out);
    close_parquet(&a);
}

int main(void) {
    const char *env = getenv("PV_TEST_I18N");
    if (env && env[0]) {
        g_path = env;
    }
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_schema),
        cmocka_unit_test(test_utf8_roundtrip),
        cmocka_unit_test(test_empty_string_row),
        cmocka_unit_test(test_null_counts),
        cmocka_unit_test(test_window_offset),
        cmocka_unit_test(test_render_emits_utf8),
        cmocka_unit_test(test_render_columns_aligned),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
