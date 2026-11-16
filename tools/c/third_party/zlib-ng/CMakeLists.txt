# zlib-ng 2.2.4 in zlib-compatible mode (the same compressed bytes as CPython 3.14's zlib), under lib/shsm_zlib and
# lib/shsm_archive. The upstream tree is in src/ with its own CMakeLists.txt; the options below are set as normal
# variables in this directory's scope only (policy CMP0077), so nothing leaks into the rest of the build.
# zlib-ng 2.2.4 accepts only C 89/99/11 in CMAKE_C_STANDARD (the top level sets 17 for our code).
set(CMAKE_C_STANDARD 11)
set(ZLIB_COMPAT ON)
set(ZLIB_ENABLE_TESTS OFF)
set(ZLIBNG_ENABLE_TESTS OFF)
set(WITH_GTEST OFF)
set(WITH_GZFILEOP OFF)
set(SKIP_INSTALL_ALL ON)
set(BUILD_SHARED_LIBS OFF)
add_subdirectory(src EXCLUDE_FROM_ALL)
shsm_register_third_party(zlib)
