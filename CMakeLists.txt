cmake_minimum_required(VERSION 3.16)
project(parquet_viewer VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

option(SANITIZE "Build with AddressSanitizer + UndefinedBehaviorSanitizer" OFF)

# ---------------------------------------------------------------------------
# DuckDB discovery
#
# Defaults to the vendored copy in third_party/duckdb (header + lib in one
# directory). Override for a system/Homebrew install, e.g.:
#   cmake -B build -DDUCKDB_ROOT=$(brew --prefix duckdb)
# ---------------------------------------------------------------------------
set(DUCKDB_ROOT "${CMAKE_SOURCE_DIR}/third_party/duckdb"
    CACHE PATH "DuckDB prefix (contains duckdb.h and libduckdb)")

find_path(DUCKDB_INCLUDE_DIR duckdb.h
    HINTS "${DUCKDB_ROOT}" "${DUCKDB_ROOT}/include")
find_library(DUCKDB_LIBRARY NAMES duckdb
    HINTS "${DUCKDB_ROOT}" "${DUCKDB_ROOT}/lib")

if(NOT DUCKDB_INCLUDE_DIR OR NOT DUCKDB_LIBRARY)
    message(FATAL_ERROR
        "DuckDB not found. Set -DDUCKDB_ROOT=<prefix> "
        "(searched: ${DUCKDB_ROOT}).")
endif()

get_filename_component(DUCKDB_LIBDIR "${DUCKDB_LIBRARY}" DIRECTORY)

# ---------------------------------------------------------------------------
# Core library: every module except the main() entry point. Both the
# executable and the unit tests link against this.
# ---------------------------------------------------------------------------
add_library(pv_core STATIC
    src/buf.c
    src/sql.c
    src/util.c
    src/source.c
    src/terminal.c
    src/navigate.c
    src/render.c
    src/app.c)
target_include_directories(pv_core PUBLIC src "${DUCKDB_INCLUDE_DIR}")
target_link_libraries(pv_core PUBLIC "${DUCKDB_LIBRARY}")
target_compile_options(pv_core PRIVATE -Wall -Wextra)

if(SANITIZE)
    set(PV_SAN -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_compile_options(pv_core PUBLIC ${PV_SAN})
    target_link_options(pv_core PUBLIC ${PV_SAN})
endif()

# ---------------------------------------------------------------------------
# Executable
# ---------------------------------------------------------------------------
add_executable(parquet_viewer src/main.c)
target_compile_definitions(parquet_viewer
    PRIVATE PV_VERSION="${PROJECT_VERSION}")
target_link_libraries(parquet_viewer PRIVATE pv_core)
set_target_properties(parquet_viewer PROPERTIES
    BUILD_RPATH "${DUCKDB_LIBDIR}"
    INSTALL_RPATH "${DUCKDB_LIBDIR}")

install(TARGETS parquet_viewer RUNTIME DESTINATION bin)

# ---------------------------------------------------------------------------
# Tests (CTest). BUILD_TESTING defaults to ON; turn off with -DBUILD_TESTING=OFF.
# ---------------------------------------------------------------------------
include(CTest)

if(BUILD_TESTING)
    find_path(CMOCKA_INCLUDE_DIR cmocka.h
        HINTS /opt/homebrew/include /usr/local/include)
    find_library(CMOCKA_LIBRARY NAMES cmocka
        HINTS /opt/homebrew/lib /usr/local/lib)

    if(CMOCKA_INCLUDE_DIR AND CMOCKA_LIBRARY)
        # test_render takes a Parquet path; the others ignore the argument.
        foreach(unit buf sql util navigate app render)
            add_executable(test_${unit} tests/test_${unit}.c)
            target_link_libraries(test_${unit}
                PRIVATE pv_core "${CMOCKA_LIBRARY}")
            # Keep the project headers ahead of the cmocka include dir: a
            # system sql.h (e.g. unixODBC under /opt/homebrew/include) must not
            # shadow src/sql.h.
            target_include_directories(test_${unit} BEFORE
                PRIVATE "${CMAKE_SOURCE_DIR}/src" "${CMOCKA_INCLUDE_DIR}")
            set_target_properties(test_${unit} PROPERTIES
                BUILD_RPATH "${DUCKDB_LIBDIR}")
            add_test(NAME unit_${unit}
                COMMAND test_${unit}
                    "${CMAKE_SOURCE_DIR}/testdata/weather.parquet")
        endforeach()

        # test_i18n reads its fixture from $PV_TEST_I18N (argv is ignored).
        if(EXISTS "${CMAKE_SOURCE_DIR}/testdata/i18n.parquet")
            add_executable(test_i18n tests/test_i18n.c)
            target_link_libraries(test_i18n
                PRIVATE pv_core "${CMOCKA_LIBRARY}")
            target_include_directories(test_i18n BEFORE
                PRIVATE "${CMAKE_SOURCE_DIR}/src" "${CMOCKA_INCLUDE_DIR}")
            set_target_properties(test_i18n PROPERTIES
                BUILD_RPATH "${DUCKDB_LIBDIR}")
            add_test(NAME unit_i18n COMMAND test_i18n)
            set_tests_properties(unit_i18n PROPERTIES
                ENVIRONMENT
                "PV_TEST_I18N=${CMAKE_SOURCE_DIR}/testdata/i18n.parquet")
        endif()
    else()
        message(WARNING
            "cmocka not found; unit tests skipped. "
            "Install it (e.g. 'brew install cmocka').")
    endif()

    # Integration tests run the built binary against sample data.
    add_test(NAME cli_help COMMAND parquet_viewer --help)
    set_tests_properties(cli_help PROPERTIES
        PASS_REGULAR_EXPRESSION "usage:")

    add_test(NAME probe_weather
        COMMAND parquet_viewer --probe
            "${CMAKE_SOURCE_DIR}/testdata/weather.parquet")
    set_tests_properties(probe_weather PROPERTIES
        PASS_REGULAR_EXPRESSION "rows:")

    if(EXISTS "${CMAKE_SOURCE_DIR}/testdata/types.parquet")
        add_test(NAME probe_types
            COMMAND parquet_viewer --probe
                "${CMAKE_SOURCE_DIR}/testdata/types.parquet")
        set_tests_properties(probe_types PROPERTIES
            PASS_REGULAR_EXPRESSION "null counts:")
    endif()

    if(EXISTS "${CMAKE_SOURCE_DIR}/testdata/i18n.parquet")
        add_test(NAME probe_i18n
            COMMAND parquet_viewer --probe
                "${CMAKE_SOURCE_DIR}/testdata/i18n.parquet")
        set_tests_properties(probe_i18n PROPERTIES
            PASS_REGULAR_EXPRESSION "rows:")
    endif()
endif()
