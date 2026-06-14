#ifndef PV_MODEL_H
#define PV_MODEL_H

#include <stdint.h>

#include "duckdb.h"

/* One Parquet column: name, type and (lazily computed) null count. */
typedef struct {
    char *name;
    char *type;
    int64_t null_count; /* valid only when null_known */
    int null_known;
} Column;

typedef enum { VIEW_DATA, VIEW_SCHEMA, VIEW_STATS } View;

/* Whole application / data-model state shared across modules. */
typedef struct {
    duckdb_database db;
    duckdb_connection conn;
    char *path;
    char *basename;
    char *lit; /* cached sql_literal(path) */
    int64_t row_count;
    uint64_t file_size;
    int64_t num_row_groups;
    char *format_version;
    char *created_by;
    Column *cols;
    int ncols;

    View view;
    int64_t top;   /* first visible data row (absolute)        */
    int64_t cur;   /* current/highlighted data row (absolute)  */
    int col_off;   /* first visible column (horizontal scroll)  */
    int stats_loaded;

    int cmd_mode;
    char cmd[256];
    int cmd_len;
    char status[256];
} App;

#endif /* PV_MODEL_H */
