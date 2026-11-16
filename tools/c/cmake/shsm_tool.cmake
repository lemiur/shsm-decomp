# Helpers for one tool per directory (tools/c/tools/<name>/), included by the top-level CMakeLists.txt.
#
# The default rule (a tool directory without its own CMakeLists.txt) is one call:
#     shsm_add_tool(<name> DIR <tools/c/tools/name>)
# A tool directory WITH its own CMakeLists.txt is add_subdirectory'd instead and calls the same function itself,
# usually with explicit lists (DIR defaults to that directory, CMAKE_CURRENT_SOURCE_DIR):
#     shsm_add_tool(<name> [DIR <dir>] [SOURCES <c>...] [MAIN <main.c>] [TESTS <c>...] [LIBS <target>...]
#                   [INCLUDES <dir>...] [NO_EXE])
#   SOURCES   the logic compiled into the static library tool_<name> (default: every <dir>/*.c except main.c;
#             no sources gives an INTERFACE library that still carries <dir> as include directory)
#   MAIN      the front end, built as <name>.exe into SHSM_TOOLS_OUT (default <dir>/main.c; absent = no exe)
#   TESTS     unit tests, each <file>.c -> test_<name>_<file>.exe in SHSM_TOOLS_OUT/tests, CTest test
#             <name>_<file> with label <name>, run from the repository root (default <dir>/tests/*.c)
#   LIBS      extra libraries for tool_<name> (shsmtools, and through it shsmcore, are always linked)
#   INCLUDES  extra include directories for tool_<name> (<dir> itself is always one)
#   NO_EXE    build the library and tests only
# Other things a tool's own CMakeLists.txt may use:
#   shsm_target_flags(<target>)              the project's compile flags (/W4 /WX ..., SHSM_C_FLAGS) on a target
#   shsm_add_test(<label> <src> [LIBS ...])  one CTest test as above (exe test_<label>_<file>, test <label>_<file>)
#   SHSM_REPO, SHSM_TOOLS_OUT, SHSM_C_FLAGS, SHSM_C_DEFS, SHSM_TEST_TIMEOUT   (set by the top-level CMakeLists.txt)
# A tool never edits a shared file: the top-level CMakeLists.txt finds every tools/c/tools/*/ by glob.

function(shsm_target_flags target)
    target_compile_options(${target} PRIVATE ${SHSM_C_FLAGS})
endfunction()

function(shsm_add_test label src)
    cmake_parse_arguments(PARSE_ARGV 2 T "" "" "LIBS")
    get_filename_component(file "${src}" NAME_WE)
    set(exe test_${label}_${file})
    add_executable(${exe} ${src})
    set_target_properties(${exe} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${SHSM_TOOLS_OUT}/tests$<0:>")
    shsm_target_flags(${exe})
    target_link_libraries(${exe} PRIVATE ${T_LIBS} shsmtools)
    add_test(NAME ${label}_${file} COMMAND ${exe} WORKING_DIRECTORY "${SHSM_REPO}")
    # A test that runs past this was waiting for something: CTest kills it (crash and hang policy, base.h).
    set_tests_properties(${label}_${file} PROPERTIES TIMEOUT ${SHSM_TEST_TIMEOUT} LABELS ${label})
endfunction()

function(shsm_add_tool name)
    cmake_parse_arguments(PARSE_ARGV 1 T "NO_EXE" "DIR;MAIN" "SOURCES;TESTS;LIBS;INCLUDES")
    if(NOT T_DIR)
        set(T_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    endif()
    if(NOT DEFINED T_SOURCES)
        file(GLOB T_SOURCES CONFIGURE_DEPENDS "${T_DIR}/*.c")
        list(FILTER T_SOURCES EXCLUDE REGEX "/main\\.c$")
    endif()
    if(NOT DEFINED T_MAIN AND EXISTS "${T_DIR}/main.c")
        set(T_MAIN "${T_DIR}/main.c")
    endif()
    if(NOT DEFINED T_TESTS)
        file(GLOB T_TESTS CONFIGURE_DEPENDS "${T_DIR}/tests/*.c")
    endif()

    set(lib tool_${name})
    if(T_SOURCES)
        add_library(${lib} STATIC ${T_SOURCES})
        target_include_directories(${lib} PUBLIC "${T_DIR}" ${T_INCLUDES})
        shsm_target_flags(${lib})
        target_link_libraries(${lib} PUBLIC ${T_LIBS} shsmtools)
    else()
        add_library(${lib} INTERFACE)
        target_include_directories(${lib} INTERFACE "${T_DIR}" ${T_INCLUDES})
        target_link_libraries(${lib} INTERFACE ${T_LIBS} shsmtools)
    endif()

    if(T_MAIN AND NOT T_NO_EXE)
        add_executable(${name} ${T_MAIN})
        shsm_target_flags(${name})
        target_link_libraries(${name} PRIVATE ${lib})
    endif()

    foreach(src ${T_TESTS})
        shsm_add_test(${name} ${src} LIBS ${lib})
    endforeach()
endfunction()
