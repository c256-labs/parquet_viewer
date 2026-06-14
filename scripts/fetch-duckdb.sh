#!/usr/bin/env bash
#
# Fetch the DuckDB C library (header + shared lib) into third_party/duckdb/.
#
# Usage:   scripts/fetch-duckdb.sh
# Override: DUCKDB_VERSION=v1.1.3 scripts/fetch-duckdb.sh
set -euo pipefail

DUCKDB_VERSION="${DUCKDB_VERSION:-v1.1.3}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DEST="$ROOT/third_party/duckdb"

os="$(uname -s)"
arch="$(uname -m)"

case "$os" in
  Darwin)
    asset="libduckdb-osx-universal.zip"
    ;;
  Linux)
    case "$arch" in
      x86_64|amd64) asset="libduckdb-linux-amd64.zip" ;;
      aarch64|arm64) asset="libduckdb-linux-aarch64.zip" ;;
      *) echo "Unsupported Linux arch: $arch" >&2; exit 1 ;;
    esac
    ;;
  MINGW*|MSYS*|CYGWIN*)
    echo "On Windows, download libduckdb-windows-amd64.zip manually from:" >&2
    echo "  https://github.com/duckdb/duckdb/releases/download/${DUCKDB_VERSION}/libduckdb-windows-amd64.zip" >&2
    echo "and unzip it into: ${DEST}" >&2
    exit 1
    ;;
  *)
    echo "Unsupported OS: $os" >&2
    exit 1
    ;;
esac

url="https://github.com/duckdb/duckdb/releases/download/${DUCKDB_VERSION}/${asset}"

echo "DuckDB version: ${DUCKDB_VERSION}"
echo "Asset:          ${asset}"
echo "Destination:    ${DEST}"

mkdir -p "$DEST"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

echo "Downloading ${url} ..."
curl -L --fail -o "${tmp}/duckdb.zip" "$url"

echo "Unzipping ..."
unzip -o "${tmp}/duckdb.zip" -d "$DEST" >/dev/null

echo "Done. Contents of ${DEST}:"
ls -la "$DEST"
