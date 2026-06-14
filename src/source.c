#include "source.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <io.h>
#include <sys/stat.h>
#else
#include <sys/stat.h>
#endif

#include "buf.h"
#include "sql.h"
#include "util.h"

/* Run a query; on error print and abort. Caller destroys the result. */
static void run_query(App *a, const char *sql, duckdb_result *res) {
    if (duckdb_query(a->conn, sql, res) == DuckDBError) {
        char msg[512];
        snprintf(msg, sizeof(msg), "query failed: %s",
                 duckdb_result_error(res));
        duckdb_destroy_result(res);
        die(msg);
    }
}

static void load_schema(App *a) {
    char sql[1024];
    snprintf(sql, sizeof(sql),
             "DESCRIBE SELECT * FROM read_parquet(%s)", a->lit);
    duckdb_result res;
    run_query(a, sql, &res);

    idx_t rows = duckdb_row_count(&res);
    a->ncols = (int)rows;
    a->cols = (Column *)calloc(rows ? rows : 1, sizeof(Column));
    for (idx_t i = 0; i < rows; i++) {
        char *name = duckdb_value_varchar(&res, 0, i);
        char *type = duckdb_value_varchar(&res, 1, i);
        a->cols[i].name = name ? strdup(name) : strdup("");
        a->cols[i].type = type ? strdup(type) : strdup("");
        a->cols[i].null_known = 0;
        duckdb_free(name);
        duckdb_free(type);
    }
    duckdb_destroy_result(&res);
}

static int64_t row_count_from_metadata(App *a, int *ok) {
    char sql[1024];
    snprintf(sql, sizeof(sql),
             "SELECT CAST(COALESCE(SUM(row_group_num_rows),0) AS BIGINT) "
             "FROM (SELECT DISTINCT row_group_id, row_group_num_rows "
             "FROM parquet_metadata(%s))",
             a->lit);
    duckdb_result res;
    if (duckdb_query(a->conn, sql, &res) == DuckDBError) {
        duckdb_destroy_result(&res);
        *ok = 0;
        return 0;
    }
    int64_t n = 0;
    if (duckdb_row_count(&res) > 0 && !duckdb_value_is_null(&res, 0, 0)) {
        n = duckdb_value_int64(&res, 0, 0);
        *ok = 1;
    } else {
        *ok = 0;
    }
    duckdb_destroy_result(&res);
    return n;
}

static int64_t row_count_from_scan(App *a) {
    char sql[1024];
    snprintf(sql, sizeof(sql),
             "SELECT CAST(COUNT(*) AS BIGINT) FROM read_parquet(%s)", a->lit);
    duckdb_result res;
    run_query(a, sql, &res);
    int64_t n = duckdb_value_int64(&res, 0, 0);
    duckdb_destroy_result(&res);
    return n;
}

static void load_row_count(App *a) {
    int ok = 0;
    int64_t n = row_count_from_metadata(a, &ok);
    a->row_count = ok ? n : row_count_from_scan(a);
}

/* File-level Parquet metadata for the info block. */
static void load_file_metadata(App *a) {
    a->num_row_groups = 0;
    a->format_version = strdup("?");
    a->created_by = strdup("");

    char sql[1024];
    snprintf(sql, sizeof(sql),
             "SELECT CAST(num_row_groups AS BIGINT), "
             "CAST(format_version AS VARCHAR), COALESCE(created_by, '') "
             "FROM parquet_file_metadata(%s)",
             a->lit);
    duckdb_result res;
    if (duckdb_query(a->conn, sql, &res) == DuckDBSuccess &&
        duckdb_row_count(&res) > 0) {
        if (!duckdb_value_is_null(&res, 0, 0)) {
            a->num_row_groups = duckdb_value_int64(&res, 0, 0);
        }
        char *fv = duckdb_value_varchar(&res, 1, 0);
        if (fv) {
            free(a->format_version);
            a->format_version = strdup(fv);
            duckdb_free(fv);
        }
        char *cb = duckdb_value_varchar(&res, 2, 0);
        if (cb) {
            free(a->created_by);
            a->created_by = strdup(cb);
            duckdb_free(cb);
        }
    }
    duckdb_destroy_result(&res);
}

int load_window(App *a, int64_t top, int64_t limit, duckdb_result *res) {
    Buf sb = {0};
    buf_str(&sb, "SELECT ");
    for (int c = 0; c < a->ncols; c++) {
        char *q = quote_ident(a->cols[c].name);
        if (c) {
            buf_str(&sb, ", ");
        }
        buf_fmt(&sb,
                "CASE WHEN %s IS NULL THEN NULL ELSE CAST(%s AS VARCHAR) END",
                q, q);
        free(q);
    }
    buf_fmt(&sb, " FROM read_parquet(%s) LIMIT %" PRId64 " OFFSET %" PRId64,
            a->lit, limit, top);

    int rc = duckdb_query(a->conn, sb.data, res) == DuckDBSuccess;
    buf_free(&sb);
    return rc;
}

void load_stats(App *a) {
    if (a->stats_loaded) {
        return;
    }
    char sql[1024];
    snprintf(sql, sizeof(sql),
             "SELECT path_in_schema, CAST(SUM(stats_null_count) AS BIGINT), "
             "bool_and(stats_null_count IS NOT NULL) "
             "FROM parquet_metadata(%s) GROUP BY path_in_schema",
             a->lit);
    duckdb_result res;
    if (duckdb_query(a->conn, sql, &res) == DuckDBSuccess) {
        idx_t rows = duckdb_row_count(&res);
        for (idx_t i = 0; i < rows; i++) {
            char *name = duckdb_value_varchar(&res, 0, i);
            int complete = !duckdb_value_is_null(&res, 2, i) &&
                           duckdb_value_boolean(&res, 2, i);
            int64_t nulls = duckdb_value_is_null(&res, 1, i)
                                ? 0
                                : duckdb_value_int64(&res, 1, i);
            if (name && complete) {
                for (int c = 0; c < a->ncols; c++) {
                    if (strcmp(a->cols[c].name, name) == 0) {
                        a->cols[c].null_count = nulls;
                        a->cols[c].null_known = 1;
                    }
                }
            }
            duckdb_free(name);
        }
    }
    duckdb_destroy_result(&res);

    for (int c = 0; c < a->ncols; c++) {
        if (a->cols[c].null_known) {
            continue;
        }
        char *q = quote_ident(a->cols[c].name);
        char fsql[1024];
        snprintf(fsql, sizeof(fsql),
                 "SELECT CAST(COUNT(*) FILTER (WHERE %s IS NULL) AS BIGINT) "
                 "FROM read_parquet(%s)",
                 q, a->lit);
        free(q);
        duckdb_result fres;
        if (duckdb_query(a->conn, fsql, &fres) == DuckDBSuccess) {
            a->cols[c].null_count = duckdb_value_int64(&fres, 0, 0);
            a->cols[c].null_known = 1;
        }
        duckdb_destroy_result(&fres);
    }
    a->stats_loaded = 1;
}

void open_parquet(App *a, const char *path) {
    if (duckdb_open(NULL, &a->db) == DuckDBError) {
        die("could not open in-memory DuckDB");
    }
    if (duckdb_connect(a->db, &a->conn) == DuckDBError) {
        die("could not connect to DuckDB");
    }
    a->path = strdup(path);
    a->basename = strdup(basename_of(path));
    a->lit = sql_literal(path);

#ifdef _WIN32
    struct _stat64 st;
    a->file_size = (_stat64(path, &st) == 0) ? (uint64_t)st.st_size : 0;
#else
    struct stat st;
    a->file_size = (stat(path, &st) == 0) ? (uint64_t)st.st_size : 0;
#endif

    load_schema(a);
    load_row_count(a);
    load_file_metadata(a);
}

void close_parquet(App *a) {
    for (int c = 0; c < a->ncols; c++) {
        free(a->cols[c].name);
        free(a->cols[c].type);
    }
    free(a->cols);
    free(a->path);
    free(a->basename);
    free(a->lit);
    free(a->format_version);
    free(a->created_by);
    if (a->conn) {
        duckdb_disconnect(&a->conn);
    }
    if (a->db) {
        duckdb_close(&a->db);
    }
}
