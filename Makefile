CC ?= cc
# Location of the vendored DuckDB C library (header + shared lib in one dir).
DUCKDB_DIR ?= third_party/duckdb
# Header and library dirs can be overridden separately, e.g. to link against a
# Homebrew-provided DuckDB (brew --prefix duckdb) where include/ and lib/ split.
DUCKDB_INCLUDE ?= $(DUCKDB_DIR)
DUCKDB_LIB ?= $(DUCKDB_DIR)
BUILD ?= build
# Reported by `parquet_viewer --version`; override to match the release tag.
VERSION ?= 0.1.0

CFLAGS ?= -O2 -std=gnu99 -Wall -Wextra
CPPFLAGS += -I$(DUCKDB_INCLUDE) -DPV_VERSION='"$(VERSION)"'
LDFLAGS += -L$(DUCKDB_LIB)
LDLIBS += -lduckdb

TARGET := $(BUILD)/parquet_viewer
SRC := $(wildcard src/*.c)
# Everything except the entry point; unit tests link against these modules.
LIB_SRC := $(filter-out src/main.c,$(SRC))

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
	DUCKDB_RPATH ?= @loader_path/../$(DUCKDB_DIR)
	LDFLAGS += -Wl,-rpath,$(DUCKDB_RPATH)
endif
ifeq ($(UNAME_S),Linux)
	# Escape the $ so the linker (not the shell) sees a literal $ORIGIN.
	DUCKDB_RPATH ?= \$$ORIGIN/../$(DUCKDB_DIR)
	LDFLAGS += -Wl,-rpath,$(DUCKDB_RPATH)
endif

# ---- tests (CMocka) -------------------------------------------------------
# cmocka location (Homebrew by default); override CMOCKA_PREFIX if elsewhere.
CMOCKA_PREFIX ?= $(shell brew --prefix cmocka 2>/dev/null)
TEST_SRC := $(wildcard tests/test_*.c)
TEST_BINS := $(patsubst tests/%.c,$(BUILD)/%,$(TEST_SRC))
TEST_CPPFLAGS := $(CPPFLAGS) -Isrc
TEST_LDFLAGS := $(LDFLAGS)
ifneq ($(CMOCKA_PREFIX),)
	TEST_CPPFLAGS += -I$(CMOCKA_PREFIX)/include
	TEST_LDFLAGS += -L$(CMOCKA_PREFIX)/lib
endif
TEST_LDLIBS := -lcmocka $(LDLIBS)

# Sample data passed to tests that need a real file (e.g. test_render).
TEST_DATA ?= testdata/weather.parquet
# Multilingual fixture for the UTF-8 tests (read via $PV_TEST_I18N).
TEST_I18N ?= testdata/i18n.parquet

.PHONY: all clean test

all: $(TARGET)

$(TARGET): $(SRC) | $(BUILD)
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ $(SRC) $(LDFLAGS) $(LDLIBS)

$(BUILD):
	mkdir -p $(BUILD)

# Each unit test is linked with all library modules (minus main).
$(BUILD)/test_%: tests/test_%.c $(LIB_SRC) | $(BUILD)
	$(CC) $(CFLAGS) $(TEST_CPPFLAGS) -o $@ $< $(LIB_SRC) \
		$(TEST_LDFLAGS) $(TEST_LDLIBS)

# Build + run unit tests (CMocka) and integration checks (probe/help).
# The sample-data argument is ignored by tests that take no arguments.
test: $(TARGET) $(TEST_BINS)
	@fail=0; \
	for t in $(TEST_BINS); do \
		echo "== unit: $$t =="; \
		PV_TEST_I18N="$(TEST_I18N)" "$$t" "$(TEST_DATA)" || fail=1; \
	done; \
	echo "== integration: --help =="; \
	$(TARGET) --help >/dev/null 2>&1 || fail=1; \
	echo "== integration: --probe weather.parquet =="; \
	$(TARGET) --probe testdata/weather.parquet >/dev/null || fail=1; \
	if [ -f $(TEST_I18N) ]; then \
		echo "== integration: --probe i18n.parquet =="; \
		$(TARGET) --probe $(TEST_I18N) >/dev/null || fail=1; \
	fi; \
	if [ -f testdata/types.parquet ]; then \
		echo "== integration: --probe types.parquet =="; \
		$(TARGET) --probe testdata/types.parquet >/dev/null || fail=1; \
	fi; \
	if [ $$fail -ne 0 ]; then echo "TESTS FAILED"; exit 1; fi; \
	echo "All tests passed."

clean:
	rm -rf $(BUILD)
