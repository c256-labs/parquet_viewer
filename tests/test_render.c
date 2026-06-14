/* Integration-style test: render real frames and inspect the captured output.
 *
 * The Parquet path comes from argv[1], then $PV_TEST_PARQUET, then a default
 * relative to the project root (works for `make test`). */
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <cmocka.h>

#include "model.h"
#include "render.h"
#include "source.h"

static const char *g_path = "testdata/weather.parquet";

/* Render one view with stdout redirected to a temp file; return the bytes. */
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

static void test_render_data(void **state) {
    (void)state;
    App a = {0};
    open_parquet(&a, g_path);
    char *out = capture_render(&a, VIEW_DATA);
    assert_non_null(strstr(out, "DATA"));
    assert_non_null(strstr(out, "Rows:"));
    assert_non_null(strstr(out, a.cols[0].name));
    free(out);
    close_parquet(&a);
}

static void test_render_schema(void **state) {
    (void)state;
    App a = {0};
    open_parquet(&a, g_path);
    char *out = capture_render(&a, VIEW_SCHEMA);
    assert_non_null(strstr(out, "SCHEMA"));
    assert_non_null(strstr(out, "COLUMN"));
    assert_non_null(strstr(out, "TYPE"));
    free(out);
    close_parquet(&a);
}

static void test_render_stats(void **state) {
    (void)state;
    App a = {0};
    open_parquet(&a, g_path);
    char *out = capture_render(&a, VIEW_STATS);
    assert_non_null(strstr(out, "STATS"));
    /* "NULLS" stays within the default 80-col width; "NULL%" can be clipped. */
    assert_non_null(strstr(out, "NULLS"));
    free(out);
    close_parquet(&a);
}

int main(int argc, char **argv) {
    if (argc > 1) {
        g_path = argv[1];
    } else {
        const char *env = getenv("PV_TEST_PARQUET");
        if (env) {
            g_path = env;
        }
    }
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_render_data),
        cmocka_unit_test(test_render_schema),
        cmocka_unit_test(test_render_stats),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
