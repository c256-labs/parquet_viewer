#include "app.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#include <unistd.h>
#endif

#include "duckdb.h"
#include "navigate.h"
#include "render.h"
#include "source.h"
#include "terminal.h"
#include "util.h"

int run_command(App *a) {
    const char *c = a->cmd;
    while (*c == ' ') c++;
    if (strcmp(c, "q") == 0 || strcmp(c, "quit") == 0) {
        return 1;
    } else if (strcmp(c, "data") == 0) {
        set_view(a, VIEW_DATA);
    } else if (strcmp(c, "schema") == 0) {
        set_view(a, VIEW_SCHEMA);
    } else if (strcmp(c, "stats") == 0) {
        set_view(a, VIEW_STATS);
    } else if (c[0] >= '0' && c[0] <= '9') {
        int64_t row = strtoll(c, NULL, 10);
        if (row > 0) row -= 1; /* 1-based input */
        goto_row(a, row);
    } else if (c[0] != '\0') {
        snprintf(a->status, sizeof(a->status), "unknown command: %.238s", c);
    }
    return 0;
}

int run_tui(App *a) {
#ifndef _WIN32
    if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO)) {
        fprintf(stderr,
                "parquet_viewer: stdin/stdout is not a terminal; "
                "use --probe for a non-interactive check.\n");
        return 1;
    }
    term_install_signal_handlers();
#endif

    enter_raw();
    enter_alt_screen();

    int quit = 0;
    while (!quit) {
        render(a);
        int k = read_key();
        a->status[0] = '\0';

        if (a->cmd_mode) {
            if (k == K_ENTER) {
                a->cmd_mode = 0;
                quit = run_command(a);
                a->cmd[0] = '\0';
                a->cmd_len = 0;
            } else if (k == K_ESC) {
                a->cmd_mode = 0;
                a->cmd[0] = '\0';
                a->cmd_len = 0;
            } else if (k == K_BACKSPACE) {
                if (a->cmd_len > 0) {
                    a->cmd[--a->cmd_len] = '\0';
                }
            } else if (k >= 32 && k < 127 &&
                       a->cmd_len < (int)sizeof(a->cmd) - 1) {
                a->cmd[a->cmd_len++] = (char)k;
                a->cmd[a->cmd_len] = '\0';
            }
            continue;
        }

        int rows = visible_rows();
        switch (k) {
            case 'q':
            case K_ESC:
                quit = 1;
                break;
            case ':':
                a->cmd_mode = 1;
                a->cmd[0] = '\0';
                a->cmd_len = 0;
                break;
            case 'j':
            case K_DOWN:
                move_cur(a, 1);
                break;
            case 'k':
            case K_UP:
                move_cur(a, -1);
                break;
            case 'h':
            case K_LEFT:
                if (a->view == VIEW_DATA && a->col_off > 0) {
                    a->col_off--;
                }
                break;
            case 'l':
            case K_RIGHT:
                if (a->view == VIEW_DATA && a->col_off < a->ncols - 1) {
                    a->col_off++;
                }
                break;
            case K_PGDN:
                move_cur(a, rows);
                break;
            case K_PGUP:
                move_cur(a, -rows);
                break;
            case 0x04: /* Ctrl-d */
                move_cur(a, rows / 2);
                break;
            case 0x15: /* Ctrl-u */
                move_cur(a, -(rows / 2));
                break;
            case K_MOUSE_DOWN:
                move_cur(a, 3);
                break;
            case K_MOUSE_UP:
                move_cur(a, -3);
                break;
            case 'g':
            case K_HOME:
                goto_row(a, 0);
                break;
            case 'G':
            case K_END:
                goto_row(a, view_count(a) - 1);
                break;
            case 'd':
                set_view(a, VIEW_DATA);
                break;
            case 's':
                set_view(a, VIEW_SCHEMA);
                break;
            case 'S':
                set_view(a, VIEW_STATS);
                break;
            case K_RESIZE:
            default:
                break;
        }
    }

    leave_alt_screen();
    leave_raw();
    return 0;
}

int probe(const char *path) {
    App a = {0};
    open_parquet(&a, path);
    char size[32];
    human_size(a.file_size, size, sizeof(size));
    printf("file:    %s\n", a.path);
    printf("size:    %s\n", size);
    printf("rows:    %" PRId64 "\n", a.row_count);
    printf("columns: %d\n", a.ncols);
    for (int c = 0; c < a.ncols; c++) {
        printf("  %2d  %-30s %s\n", c + 1, a.cols[c].name, a.cols[c].type);
    }
    int64_t lim = a.row_count < 3 ? a.row_count : 3;
    if (lim > 0) {
        duckdb_result res;
        if (load_window(&a, 0, lim, &res)) {
            idx_t got = duckdb_row_count(&res);
            printf("first %" PRId64 " row(s):\n", (int64_t)got);
            for (idx_t r = 0; r < got; r++) {
                printf("  ");
                for (int c = 0; c < a.ncols; c++) {
                    char *v = duckdb_value_is_null(&res, c, r)
                                  ? NULL
                                  : duckdb_value_varchar(&res, c, r);
                    printf("%s%s", c ? " | " : "", v ? v : "NULL");
                    duckdb_free(v);
                }
                printf("\n");
            }
            duckdb_destroy_result(&res);
        }
    }
    load_stats(&a);
    printf("null counts:\n");
    for (int c = 0; c < a.ncols; c++) {
        if (a.cols[c].null_known) {
            printf("  %-30s %" PRId64 "\n", a.cols[c].name,
                   a.cols[c].null_count);
        } else {
            printf("  %-30s n/a\n", a.cols[c].name);
        }
    }
    close_parquet(&a);
    return 0;
}
