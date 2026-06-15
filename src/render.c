#include "render.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#include <unistd.h>
#endif

#include "buf.h"
#include "duckdb.h"
#include "layout.h"
#include "source.h"
#include "terminal.h"
#include "util.h"

#define COL_MIN 3
#define COL_MAX 40

/* Colors (256-color ANSI). */
#define C_RESET  "\x1b[0m"
#define C_BORDER "\x1b[38;5;39m"            /* azure box border          */
#define C_TITLE  "\x1b[1;38;5;231m"         /* bright title in border    */
#define C_HDR    "\x1b[1;38;5;51m"          /* cyan column headers       */
#define C_SEL    "\x1b[48;5;24;1;38;5;231m" /* selected row (cyan bg)    */
#define C_LABEL  "\x1b[38;5;245m"           /* info labels (grey)        */
#define C_VAL    "\x1b[1;38;5;231m"         /* info values (white)       */
#define C_LOGO   "\x1b[1;38;5;208m"         /* logo / accent (orange)    */
#define C_FOOT   "\x1b[48;5;236;38;5;252m"  /* footer bar                */

/* Box-drawing glyphs (UTF-8, one display column each). */
#define BX_TL "\xe2\x94\x8c"
#define BX_TR "\xe2\x94\x90"
#define BX_BL "\xe2\x94\x94"
#define BX_BR "\xe2\x94\x98"
#define BX_H  "\xe2\x94\x80"
#define BX_V  "\xe2\x94\x82"

static const char *view_name(View v) {
    switch (v) {
        case VIEW_DATA: return "DATA";
        case VIEW_SCHEMA: return "SCHEMA";
        case VIEW_STATS: return "STATS";
    }
    return "?";
}

/* Append `s` truncated/padded to exactly `w` terminal columns. Width is
 * measured in display columns (not bytes), and truncation never splits a
 * UTF-8 code point, so multi-byte text stays column-aligned. */
static void cell(Buf *b, const char *s, int w) {
    if (w <= 0) {
        return;
    }
    int dw = display_width(s);
    if (dw <= w) {
        buf_str(b, s);
        for (int i = dw; i < w; i++) {
            buf_str(b, " ");
        }
    } else if (w == 1) {
        buf_str(b, ">");
    } else {
        /* Fill w-1 columns with whole code points, pad if a wide glyph left a
         * one-column gap, then a ">" marker for the truncation. */
        int used = 0;
        size_t nb = utf8_prefix_bytes(s, w - 1, &used);
        buf_append(b, s, nb);
        for (int i = used; i < w - 1; i++) {
            buf_str(b, " ");
        }
        buf_str(b, ">");
    }
}

static void move_to(Buf *b, int row) {
    buf_fmt(b, "\x1b[%d;1H", row + 1); /* terminal rows are 1-based */
}

static void clear_eol(Buf *b) {
    buf_str(b, "\x1b[K");
}

/* Top metadata info block (3 lines). */
static void render_header_block(App *a, Buf *b, int W) {
    (void)W;
    char size[32];
    human_size(a->file_size, size, sizeof(size));

    move_to(b, 0);
    buf_str(b, C_LOGO " parquet-viewer" C_RESET);
    buf_str(b, C_LABEL "  \xe2\x80\x94  a tiny parquet browser" C_RESET);
    clear_eol(b);

    move_to(b, 1);
    buf_fmt(b,
            " " C_LABEL "Rows: " C_VAL "%" PRId64 C_RESET
            "   " C_LABEL "Cols: " C_VAL "%d" C_RESET
            "   " C_LABEL "Size: " C_VAL "%s" C_RESET,
            a->row_count, a->ncols, size);
    clear_eol(b);

    move_to(b, 2);
    buf_fmt(b,
            " " C_LABEL "Row groups: " C_VAL "%" PRId64 C_RESET
            "   " C_LABEL "Format: " C_VAL "v%s" C_RESET
            "   " C_LABEL "Created by: " C_VAL "%.40s" C_RESET,
            a->num_row_groups,
            a->format_version ? a->format_version : "?",
            a->created_by ? a->created_by : "");
    clear_eol(b);
}

/* Top border with an embedded title: |- title -----| */
static void box_top(Buf *b, int W, const char *title) {
    int inner = W - 2;
    if (inner < 0) {
        inner = 0;
    }
    move_to(b, BOX_TOP_ROW);
    buf_str(b, C_BORDER);
    buf_str(b, BX_TL);
    int tcols = display_width(title);
    if (tcols > inner) {
        tcols = inner;
    }
    int dashes_after = inner - tcols - 1;
    if (dashes_after < 0) {
        dashes_after = 0;
    }
    if (inner > 0) {
        buf_str(b, BX_H);
    }
    buf_str(b, C_TITLE);
    size_t tbytes = utf8_prefix_bytes(title, tcols, NULL);
    buf_append(b, title, tbytes);
    buf_str(b, C_BORDER);
    for (int i = 0; i < dashes_after; i++) {
        buf_str(b, BX_H);
    }
    buf_str(b, BX_TR);
    buf_str(b, C_RESET);
    clear_eol(b);
}

static void box_bottom(Buf *b, int H, int W) {
    int inner = W - 2;
    if (inner < 0) {
        inner = 0;
    }
    move_to(b, H - 2);
    buf_str(b, C_BORDER);
    buf_str(b, BX_BL);
    for (int i = 0; i < inner; i++) {
        buf_str(b, BX_H);
    }
    buf_str(b, BX_BR);
    buf_str(b, C_RESET);
    clear_eol(b);
}

/* One content line inside the box: | <text padded/truncated to inner_w> | */
static void put_line(Buf *b, int row, int inner_w, const char *color,
                     const char *text) {
    if (inner_w < 0) {
        inner_w = 0;
    }
    move_to(b, row);
    buf_str(b, C_BORDER);
    buf_str(b, BX_V);
    buf_str(b, C_RESET);
    if (color && color[0]) {
        buf_str(b, color);
    }
    int dw = display_width(text);
    if (dw >= inner_w) {
        int used = 0;
        size_t nb = utf8_prefix_bytes(text, inner_w, &used);
        buf_append(b, text, nb);
        for (int i = used; i < inner_w; i++) {
            buf_str(b, " ");
        }
    } else {
        buf_str(b, text);
        for (int i = dw; i < inner_w; i++) {
            buf_str(b, " ");
        }
    }
    if (color && color[0]) {
        buf_str(b, C_RESET);
    }
    buf_str(b, C_BORDER);
    buf_str(b, BX_V);
    buf_str(b, C_RESET);
    clear_eol(b);
}

/* Lay out visible columns (with horizontal scroll) into a plain text line. */
static void build_header_cols(App *a, int *w, int inner_w, Buf *line) {
    int used = 0;
    for (int c = a->col_off; c < a->ncols && used < inner_w; c++) {
        int avail = inner_w - used;
        int cw = w[c] < avail ? w[c] : avail;
        cell(line, a->cols[c].name, cw);
        used += cw;
        if (used < inner_w) {
            buf_str(line, " ");
            used++;
        }
    }
}

static void build_data_cols(App *a, duckdb_result *res, idx_t r, int *w,
                            int inner_w, Buf *line) {
    int used = 0;
    for (int c = a->col_off; c < a->ncols && used < inner_w; c++) {
        const char *v;
        char *vv = NULL;
        if (duckdb_value_is_null(res, c, r)) {
            v = "NULL";
        } else {
            vv = duckdb_value_varchar(res, c, r);
            v = vv ? vv : "";
        }
        int avail = inner_w - used;
        int cw = w[c] < avail ? w[c] : avail;
        cell(line, v, cw);
        duckdb_free(vv);
        used += cw;
        if (used < inner_w) {
            buf_str(line, " ");
            used++;
        }
    }
}

static void render_footer(App *a, Buf *b, int H, int W) {
    move_to(b, H - 1);
    buf_str(b, C_FOOT);
    char line[512];
    if (a->cmd_mode) {
        snprintf(line, sizeof(line), " :%s", a->cmd);
    } else if (a->status[0]) {
        snprintf(line, sizeof(line), " %s", a->status);
    } else {
        snprintf(line, sizeof(line),
                 " <j/k> row  <h/l> cols  <g/G> top/bot  <wheel> scroll  "
                 "<d/s/S> views  <:> cmd  <q> quit");
    }
    cell(b, line, W);
    buf_str(b, C_RESET);
    clear_eol(b);
}

static void render_data(App *a, Buf *b, int H, int W) {
    int inner_w = W - 2;
    if (inner_w < 1) {
        inner_w = 1;
    }
    int data_rows = data_rows_for(H);

    duckdb_result res;
    int ok = a->ncols > 0 && load_window(a, a->top, data_rows, &res);
    idx_t nrows = ok ? duckdb_row_count(&res) : 0;

    /* Per-page column widths from header + visible cells (clamped). */
    int *w = (int *)calloc((size_t)(a->ncols > 0 ? a->ncols : 1), sizeof(int));
    for (int c = 0; c < a->ncols; c++) {
        int width = (int)strlen(a->cols[c].name);
        for (idx_t r = 0; r < nrows; r++) {
            const char *v;
            char *vv = NULL;
            if (duckdb_value_is_null(&res, c, r)) {
                v = "NULL";
            } else {
                vv = duckdb_value_varchar(&res, c, r);
                v = vv ? vv : "";
            }
            int l = (int)strlen(v);
            if (l > width) {
                width = l;
            }
            duckdb_free(vv);
        }
        if (width < COL_MIN) width = COL_MIN;
        if (width > COL_MAX) width = COL_MAX;
        w[c] = width;
    }

    Buf hl = {0};
    build_header_cols(a, w, inner_w, &hl);
    put_line(b, TABLE_HEADER_ROW, inner_w, C_HDR, hl.data ? hl.data : "");
    buf_free(&hl);

    for (int dr = 0; dr < data_rows; dr++) {
        int row = DATA_TOP_ROW + dr;
        int64_t abs_row = a->top + dr;
        int highlight = (abs_row == a->cur);
        if ((idx_t)dr < nrows) {
            Buf rl = {0};
            build_data_cols(a, &res, (idx_t)dr, w, inner_w, &rl);
            put_line(b, row, inner_w, highlight ? C_SEL : "",
                     rl.data ? rl.data : "");
            buf_free(&rl);
        } else {
            put_line(b, row, inner_w, "", "");
        }
    }

    free(w);
    if (ok) {
        duckdb_destroy_result(&res);
    }
}

static void render_schema(App *a, Buf *b, int H, int W) {
    int inner_w = W - 2;
    if (inner_w < 1) {
        inner_w = 1;
    }
    int data_rows = data_rows_for(H);

    char hdr[256];
    snprintf(hdr, sizeof(hdr), "%-5s %-30s %s", "#", "COLUMN", "TYPE");
    put_line(b, TABLE_HEADER_ROW, inner_w, C_HDR, hdr);

    for (int dr = 0; dr < data_rows; dr++) {
        int row = DATA_TOP_ROW + dr;
        int idx = (int)a->top + dr;
        int highlight = (idx == (int)a->cur);
        if (idx < a->ncols) {
            char line[512];
            snprintf(line, sizeof(line), "%-5d %-30s %s",
                     idx + 1, a->cols[idx].name, a->cols[idx].type);
            put_line(b, row, inner_w, highlight ? C_SEL : "", line);
        } else {
            put_line(b, row, inner_w, "", "");
        }
    }
}

static void render_stats(App *a, Buf *b, int H, int W) {
    load_stats(a);
    int inner_w = W - 2;
    if (inner_w < 1) {
        inner_w = 1;
    }
    int data_rows = data_rows_for(H);

    char hdr[256];
    snprintf(hdr, sizeof(hdr), "%-28s %-16s %12s %12s %8s",
             "COLUMN", "TYPE", "ROWS", "NULLS", "NULL%");
    put_line(b, TABLE_HEADER_ROW, inner_w, C_HDR, hdr);

    for (int dr = 0; dr < data_rows; dr++) {
        int row = DATA_TOP_ROW + dr;
        int idx = (int)a->top + dr;
        int highlight = (idx == (int)a->cur);
        if (idx < a->ncols) {
            Column *col = &a->cols[idx];
            double pct = (a->row_count > 0 && col->null_known)
                             ? (double)col->null_count /
                                   (double)a->row_count * 100.0
                             : 0.0;
            char nulls[32], pcts[32];
            if (col->null_known) {
                snprintf(nulls, sizeof(nulls), "%" PRId64, col->null_count);
                snprintf(pcts, sizeof(pcts), "%.1f%%", pct);
            } else {
                snprintf(nulls, sizeof(nulls), "n/a");
                snprintf(pcts, sizeof(pcts), "n/a");
            }
            char line[512];
            snprintf(line, sizeof(line), "%-28s %-16s %12" PRId64 " %12s %8s",
                     col->name, col->type, a->row_count, nulls, pcts);
            put_line(b, row, inner_w, highlight ? C_SEL : "", line);
        } else {
            put_line(b, row, inner_w, "", "");
        }
    }
}

void render(App *a) {
    int H, W;
    term_size(&H, &W);
    if (H < 8) H = 8;
    if (W < 12) W = 12;

    Buf b = {0};
    buf_str(&b, "\x1b[H");

    render_header_block(a, &b, W);

    char title[256];
    snprintf(title, sizeof(title), " %s [%s] ", a->basename,
             view_name(a->view));
    box_top(&b, W, title);
    switch (a->view) {
        case VIEW_DATA: render_data(a, &b, H, W); break;
        case VIEW_SCHEMA: render_schema(a, &b, H, W); break;
        case VIEW_STATS: render_stats(a, &b, H, W); break;
    }
    box_bottom(&b, H, W);
    render_footer(a, &b, H, W);

#ifndef _WIN32
    ssize_t wr = write(STDOUT_FILENO, b.data, b.len);
    (void)wr;
#else
    fwrite(b.data, 1, b.len, stdout);
    fflush(stdout);
#endif
    buf_free(&b);
}
