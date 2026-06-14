#include "navigate.h"

#include "layout.h"
#include "terminal.h"

void nav_clamp(int64_t *cur, int64_t *top, int64_t total, int rows) {
    if (total <= 0) {
        *cur = *top = 0;
        return;
    }
    if (*cur < 0) *cur = 0;
    if (*cur >= total) *cur = total - 1;
    if (*cur < *top) *top = *cur;
    if (*cur >= *top + rows) *top = *cur - rows + 1;
    if (*top < 0) *top = 0;
    if (*top > total - 1) *top = total - 1;
}

int64_t view_count(const App *a) {
    return (a->view == VIEW_DATA) ? a->row_count : (int64_t)a->ncols;
}

int visible_rows(void) {
    int H, W;
    term_size(&H, &W);
    (void)W;
    return data_rows_for(H);
}

void clamp_scroll(App *a) {
    nav_clamp(&a->cur, &a->top, view_count(a), visible_rows());
}

void move_cur(App *a, int64_t delta) {
    a->cur += delta;
    clamp_scroll(a);
}

void goto_row(App *a, int64_t row) {
    a->cur = row;
    clamp_scroll(a);
}

void set_view(App *a, View v) {
    a->view = v;
    a->cur = 0;
    a->top = 0;
    a->col_off = 0;
}
