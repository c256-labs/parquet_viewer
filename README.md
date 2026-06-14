# parquet_viewer

A terminal UI for browsing Parquet files. One small C binary that
browses a Parquet file full-screen in the terminal — efficient on memory and
CPU because it never loads the whole file.

The viewer reads the row count from the Parquet footer, the schema via
`DESCRIBE`, and only the **visible window** of rows (`LIMIT/OFFSET`); scrolling
re-reads just the new window. A 50 GB file uses roughly the same memory as a
5 MB one.

## Install

### Homebrew (macOS / Linux)

```bash
brew install c256-labs/tap/parquet_viewer
```

This auto-taps `c256-labs/tap`, pulls in `duckdb` from Homebrew, and builds
`parquet_viewer` from source against it. Because it compiles, the first install
needs a C compiler (on macOS the Xcode Command Line Tools, which Homebrew
installs automatically) and is not instant.

### Try it

```bash
# grab a small sample Parquet file
curl -L -o /tmp/weather.parquet \
  https://github.com/c256-labs/parquet_viewer/raw/main/testdata/weather.parquet

# non-interactive sanity check (no TUI)
parquet_viewer --probe /tmp/weather.parquet

# the full TUI — j/k to scroll, d/s/S to switch views, q to quit
parquet_viewer /tmp/weather.parquet
```

`parquet_viewer` is a terminal UI, so it needs a real interactive terminal;
use `--probe` in scripts or pipelines.

### Upgrade / uninstall

```bash
brew update && brew upgrade parquet_viewer   # update to the latest release
brew uninstall parquet_viewer                # remove the binary
brew untap c256-labs/tap                      # remove the tap
```

The Homebrew formula lives in the separate
[`c256-labs/homebrew-tap`](https://github.com/c256-labs/homebrew-tap) repo; see
its [PUBLISHING.md](https://github.com/c256-labs/homebrew-tap/blob/main/PUBLISHING.md)
for how the tap and formula are maintained.

## Requirements (build from source)

- A C99 compiler (`cc`/`clang`/`gcc`), `make`
- `curl` + `unzip` (to fetch DuckDB)
- The DuckDB C library — the only external dependency

## Build

```bash
./scripts/fetch-duckdb.sh   # downloads duckdb.h + libduckdb into third_party/duckdb/
make                        # builds build/parquet_viewer
```

`fetch-duckdb.sh` picks the right release asset for your platform (macOS
universal, Linux amd64/aarch64). On Windows, download
`libduckdb-windows-amd64.zip` from the DuckDB releases page into
`third_party/duckdb/` and build with your toolchain. Override the version with
`DUCKDB_VERSION=v1.1.3 ./scripts/fetch-duckdb.sh`.

To build against a Homebrew-provided DuckDB instead of the vendored copy:

```bash
DUCKDB="$(brew --prefix duckdb)"
make DUCKDB_INCLUDE="$DUCKDB/include" \
     DUCKDB_LIB="$DUCKDB/lib" \
     DUCKDB_RPATH="$DUCKDB/lib"
```

## Run

```bash
./build/parquet_viewer weather.parquet
```

The library path is baked in via rpath (relative to the binary:
`@loader_path/../third_party/duckdb` on macOS, `$ORIGIN/../third_party/duckdb`
on Linux), so the binary finds the vendored `libduckdb`. The Homebrew build
overrides the rpath to point at the brew-provided `duckdb` instead.

### Non-interactive probe

To sanity-check a file without a TUI (useful in scripts/CI):

```bash
./build/parquet_viewer --probe weather.parquet
```

This prints file size, row count, schema, the first few rows, and per-column
null counts, then exits.

## Views

- **DATA** (default) — scrollable table with current-row highlight, dynamic
  per-page column widths, and horizontal column scrolling.
- **SCHEMA** — `# | COLUMN | TYPE`.
- **STATS** — `COLUMN | TYPE | ROWS | NULLS | NULL%` (null counts computed
  lazily on first entry, then cached).

## Keys

| Key | Action |
| --- | --- |
| `j` / `k` / `↓` / `↑` | move row |
| mouse wheel | scroll rows |
| `h` / `l` / `←` / `→` | scroll columns (DATA view) |
| `PgUp` / `PgDn` | page up / down |
| `Ctrl-d` / `Ctrl-u` | half page down / up |
| `g` / `Home` | jump to top |
| `G` / `End` | jump to bottom |
| `d` / `s` / `S` | DATA / SCHEMA / STATS view |
| `:` | command mode |
| `q` / `Esc` | quit |

Mouse-wheel scrolling uses xterm SGR mouse reporting, supported by most modern
terminals (iTerm2, Terminal.app, Windows Terminal, Kitty, Alacritty, etc.).

Command mode (`:`): `:q` / `:quit`, `:data`, `:schema`, `:stats`,
`:<rownum>` (go to a 1-based row).

## Layout

k9s-style: a metadata info block on top, a bordered table box, and a footer.

- Info block: file rows/cols/size and Parquet footer metadata (row groups,
  format version, created-by).
- Boxed table with a titled top border (`┌─ <file> [VIEW] ─┐`), a colored
  column-header row, and the data/meta rows with the current row highlighted.
- Footer: key hints, the `:command` line, or a status message.

## Project layout

```
src/                    modular C99 TUI (DuckDB C API + raw ANSI/termios)
  main.c                entry point + arg parsing
  app.c                 command/event loop and the non-interactive probe
  source.c              DuckDB access and the data model
  render.c              frame drawing
  terminal.c            raw mode, key/mouse input, signals
  navigate.c            scrolling/clamping
  buf.c sql.c util.c    leaf utilities (growable buffer, SQL quoting, helpers)
CMakeLists.txt / Makefile  two interchangeable build systems
scripts/fetch-duckdb.sh downloads the DuckDB C library into third_party/duckdb/
scripts/gen_i18n_parquet.c  generator for the multilingual UTF-8 fixture
scripts/gen-testdata.sh     compiles + runs the generator
third_party/duckdb/     vendored duckdb.h + libduckdb (gitignored)
build/                  build output (gitignored)
testdata/               sample Parquet files
tests/                  CMocka unit tests + integration probes
```

The Homebrew formula lives in the separate `homebrew-tap` repo
(`Formula/parquet_viewer.rb`); see [PUBLISHING.md](https://github.com/c256-labs/homebrew-tap/blob/main/PUBLISHING.md).

## Tests

`make test` builds and runs the CMocka unit tests plus a few integration
probes; `cmake --build build && ctest` works too.

`testdata/i18n.parquet` is a small multilingual fixture (Polish, Russian,
Chinese, Hindi, emoji, embedded quotes, empty strings and NULLs) used by
`tests/test_i18n.c` to verify that multi-byte UTF-8 round-trips byte-for-byte
through the DuckDB read and render paths. Regenerate it with:

```bash
./scripts/gen-testdata.sh
```
