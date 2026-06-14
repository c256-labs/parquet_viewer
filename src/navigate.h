#ifndef PV_NAVIGATE_H
#define PV_NAVIGATE_H

#include <stdint.h>

#include "model.h"

/* Pure scroll-clamping core, factored out for unit testing.
 *
 * Given a current row *cur and viewport top *top, a total item count and the
 * number of visible rows, clamp both so that:
 *   - 0 <= *cur < total
 *   - the viewport [*top, *top + rows) contains *cur
 * For total <= 0 both are reset to 0. */
void nav_clamp(int64_t *cur, int64_t *top, int64_t total, int rows);

/* Number of items in the current view (rows for DATA, columns otherwise). */
int64_t view_count(const App *a);

/* Visible data rows for the current terminal size. */
int visible_rows(void);

void clamp_scroll(App *a);
void move_cur(App *a, int64_t delta);
void goto_row(App *a, int64_t row);
void set_view(App *a, View v);

#endif /* PV_NAVIGATE_H */
