#ifndef PV_APP_H
#define PV_APP_H

#include "model.h"

/* Execute the buffered ':' command. Returns 1 if the app should quit. */
int run_command(App *a);

/* Run the interactive TUI loop. Returns the process exit code. */
int run_tui(App *a);

/* Non-interactive sanity check: print file/schema/rows/null counts. */
int probe(const char *path);

#endif /* PV_APP_H */
