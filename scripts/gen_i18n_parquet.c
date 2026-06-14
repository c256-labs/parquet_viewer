/* Generate testdata/i18n.parquet: a small fixture exercising multi-byte UTF-8
 * (Polish, Russian, Chinese, Hindi), emoji, embedded SQL quotes, empty strings
 * and NULLs across several column types.
 *
 * Reproducible, dependency-free generation: links the vendored DuckDB C library
 * and writes the file via `COPY ... TO (FORMAT PARQUET)`. Built and run by
 * scripts/gen-testdata.sh; the resulting Parquet is committed under testdata/.
 *
 * This source file is UTF-8; the non-ASCII text below is stored verbatim in the
 * narrow string literal. In SQL string literals a single quote is escaped by
 * doubling it (''), which is why O'Brien appears as O''Brien.
 *
 * Usage: gen_i18n_parquet [output_path]   (default: testdata/i18n.parquet)
 */
#include <stdio.h>
#include <stdlib.h>

#include "duckdb.h"

int main(int argc, char **argv) {
    const char *out = (argc > 1) ? argv[1] : "testdata/i18n.parquet";

    duckdb_database db;
    duckdb_connection conn;
    if (duckdb_open(NULL, &db) == DuckDBError) {
        fprintf(stderr, "gen: could not open in-memory DuckDB\n");
        return 1;
    }
    if (duckdb_connect(db, &conn) == DuckDBError) {
        fprintf(stderr, "gen: could not connect to DuckDB\n");
        duckdb_close(&db);
        return 1;
    }

    const char *sql =
        "COPY ("
        "SELECT * FROM (VALUES "
        "(CAST(1 AS BIGINT), 'Polish',  'zażółć gęślą jaźń', 'pangram', CAST(3.14 AS DOUBLE)), "
        "(CAST(2 AS BIGINT), 'Russian', 'Съешь же ещё этих мягких французских булок, да выпей чаю', NULL, NULL), "
        "(CAST(3 AS BIGINT), 'Chinese', '視野無限廣，窗外有藍天', 'CJK sample', CAST(2.71 AS DOUBLE)), "
        "(CAST(4 AS BIGINT), 'Hindi',   'ऋषियों को सताने वाले दुष्ट राक्षसों के राजा रावण का सर्वनाश करने वाले श्रीराम', NULL, NULL), "
        "(CAST(5 AS BIGINT), 'Emoji',   'Hello 👋 World 🌍 — café', NULL, CAST(0.0 AS DOUBLE)), "
        "(CAST(6 AS BIGINT), 'Quotes',  'O''Brien said \"hi\"', 'apostrophe '' and quote \"', CAST(-1.5 AS DOUBLE)), "
        "(CAST(7 AS BIGINT), 'Empty',   '', '', CAST(9.99 AS DOUBLE))"
        ") AS t(id, lang, greeting, note, value)"
        ") TO '%s' (FORMAT PARQUET)";

    char query[4096];
    snprintf(query, sizeof(query), sql, out);

    duckdb_result res;
    if (duckdb_query(conn, query, &res) == DuckDBError) {
        fprintf(stderr, "gen: query failed: %s\n", duckdb_result_error(&res));
        duckdb_destroy_result(&res);
        duckdb_disconnect(&conn);
        duckdb_close(&db);
        return 1;
    }
    duckdb_destroy_result(&res);
    duckdb_disconnect(&conn);
    duckdb_close(&db);

    printf("wrote %s\n", out);
    return 0;
}
