#ifndef PV_LAYOUT_H
#define PV_LAYOUT_H

/* Screen geometry shared by the renderer and navigation. */

/* 3-line info block on top, then a boxed table, then a footer. */
#define HEADER_ROWS 3
#define BOX_TOP_ROW HEADER_ROWS
#define TABLE_HEADER_ROW (HEADER_ROWS + 1)
#define DATA_TOP_ROW (HEADER_ROWS + 2)

/* Number of data rows that fit for a terminal of height H. */
static inline int data_rows_for(int H) {
    int r = H - HEADER_ROWS - 4;
    return r < 1 ? 1 : r;
}

#endif /* PV_LAYOUT_H */
