#ifndef PV_SOURCE_H
#define PV_SOURCE_H

#include <stdint.h>

#include "duckdb.h"
#include "model.h"

/* Open a Parquet file: connect DuckDB, load schema, row count and metadata.
 * Aborts (via die) on fatal errors. */
void open_parquet(App *a, const char *path);

/* Release everything acquired by open_parquet. */
void close_parquet(App *a);

/* Build and run the visible-window SELECT (all columns cast to VARCHAR, NULLs
 * preserved). On success returns 1 and the caller must destroy *res. */
int load_window(App *a, int64_t top, int64_t limit, duckdb_result *res);

/* Compute per-column null counts lazily (footer first, scan fallback). */
void load_stats(App *a);

#endif /* PV_SOURCE_H */
