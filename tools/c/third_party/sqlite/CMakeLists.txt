# SQLite 3.50.4 amalgamation (public domain; the version CPython 3.14 links), for the retail asm index
# (lib/shsm_asmindex). No extension loading, foreign keys off by default (as Python's connections), serialized
# threading (SQLITE_THREADSAFE=1, Python's build).
add_library(shsm_sqlite STATIC sqlite3.c)
target_include_directories(shsm_sqlite PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(shsm_sqlite PRIVATE SQLITE_OMIT_LOAD_EXTENSION SQLITE_DEFAULT_FOREIGN_KEYS=0
    SQLITE_THREADSAFE=1 _CRT_SECURE_NO_WARNINGS)
shsm_register_third_party(shsm_sqlite)
