#!/usr/bin/env bash
#
# Regenerate the multilingual sample Parquet (testdata/i18n.parquet).
#
# Compiles scripts/gen_i18n_parquet.c against the vendored DuckDB C library and
# runs it. Requires that scripts/fetch-duckdb.sh has populated third_party/duckdb.
#
# Usage: scripts/gen-testdata.sh [output_path]
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DUCKDB_DIR="${DUCKDB_DIR:-$ROOT/third_party/duckdb}"
OUT="${1:-$ROOT/testdata/i18n.parquet}"
CC="${CC:-cc}"

if [ ! -e "$DUCKDB_DIR/duckdb.h" ]; then
  echo "DuckDB not found in $DUCKDB_DIR; run scripts/fetch-duckdb.sh first." >&2
  exit 1
fi

tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
bin="$tmp/gen_i18n_parquet"

rpath_flag=()
case "$(uname -s)" in
  Darwin|Linux) rpath_flag=(-Wl,-rpath,"$DUCKDB_DIR") ;;
esac

"$CC" -std=c99 -Wall -Wextra -I"$DUCKDB_DIR" \
  -o "$bin" "$ROOT/scripts/gen_i18n_parquet.c" \
  -L"$DUCKDB_DIR" "${rpath_flag[@]}" -lduckdb

mkdir -p "$(dirname "$OUT")"
DYLD_LIBRARY_PATH="$DUCKDB_DIR" LD_LIBRARY_PATH="$DUCKDB_DIR" "$bin" "$OUT"
