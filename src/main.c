/*
 * parquet_viewer - a small terminal UI for browsing Parquet files.
 *
 * C99 + the DuckDB C API. Only the visible window of rows is ever read, so
 * memory use is independent of file size. Logic is split across modules:
 *   buf/sql/util  - leaf utilities
 *   source        - DuckDB access and the data model
 *   terminal      - raw mode, key/mouse input, signals
 *   navigate      - scrolling/clamping
 *   render        - frame drawing
 *   app           - command/event loop and the non-interactive probe
 */

#include <stdio.h>
#include <string.h>

#include "app.h"
#include "model.h"
#include "source.h"

#ifndef PV_VERSION
#define PV_VERSION "0.1.0"
#endif

static void usage(const char *prog) {
    fprintf(stderr,
            "usage: %s [--probe] <file.parquet>\n"
            "       %s --version | --help\n",
            prog, prog);
}

int main(int argc, char **argv) {
    const char *path = NULL;
    int probe_mode = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--probe") == 0) {
            probe_mode = 1;
        } else if (strcmp(argv[i], "-h") == 0 ||
                   strcmp(argv[i], "--help") == 0) {
            usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "-v") == 0 ||
                   strcmp(argv[i], "--version") == 0) {
            printf("parquet_viewer %s\n", PV_VERSION);
            return 0;
        } else {
            path = argv[i];
        }
    }
    if (!path) {
        usage(argv[0]);
        return 1;
    }

    if (probe_mode) {
        return probe(path);
    }

    App a = {0};
    open_parquet(&a, path);
    a.view = VIEW_DATA;
    int rc = run_tui(&a);
    close_parquet(&a);
    return rc;
}
