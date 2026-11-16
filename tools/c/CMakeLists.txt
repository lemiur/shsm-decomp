# The project's tools in C (tools/c): the runtime library shsmcore (core/, taken from another project of the author's, avp_ API names kept), the
# shared tool logic shsmtools (lib/), one directory per tool (tools/<name>/), the unit tests. Build every tool, in tools/c (Release, no tests):
#     cmake --workflow --preset tools-build
# or configure, build and run the unit tests:  cmake --workflow --preset tools
# (tools/build_c.cmd is a convenience wrapper around the same presets.)
# The executables land in <repo>/build/tools/, the tests in <repo>/build/tools/tests/.
cmake_minimum_required(VERSION 3.25)
project(shsmtools LANGUAGES C)

if(NOT MSVC)
    message(FATAL_ERROR "shsmtools: built with MSVC (Visual Studio 2022, x64) only")
endif()
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "shsmtools: build the tools as x64 (preset tools)")
endif()

get_filename_component(SHSM_REPO "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
set(SHSM_TOOLS_OUT "${SHSM_REPO}/build/tools" CACHE PATH "where the tool executables go")
# $<0:> keeps multi-config generators from adding a per-configuration folder.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${SHSM_TOOLS_OUT}$<0:>")
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
# A user-wide vcpkg MSBuild integration adds an app-local DLL copy step that runs pwsh.exe after every link (and fails
# noisily where PowerShell 7 is absent); the tools use no vcpkg packages.
set(CMAKE_VS_GLOBALS "VcpkgEnabled=false")

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Our code: /W4 clean, warnings are errors.
set(SHSM_C_FLAGS /W4 /WX /utf-8 /Zc:preprocessor /Oi /Gy $<$<CONFIG:Release>:/GL>)
set(SHSM_C_DEFS WIN32_LEAN_AND_MEAN NOMINMAX _CRT_SECURE_NO_WARNINGS UNICODE _UNICODE)
add_link_options($<$<CONFIG:Release>:/LTCG> $<$<CONFIG:Release>:/OPT:REF> $<$<CONFIG:Release>:/OPT:ICF>)
# /d2:-scheduler-: another project of the author's found the LTCG instruction scheduler of MSVC 14.51 moving a store past a load of the same
# stack slot when a struct-valued ?: has an inlined arm (core/tests/struct_cond_test.c); kept for the same code.
add_link_options($<$<CONFIG:Release>:/d2:-scheduler->)

# avp_tools_stamp(): the hash of the tools' C sources (cmake/tools_stamp.cmake), regenerated when any of them changes.
file(GLOB SHSMCORE_SOURCES CONFIGURE_DEPENDS core/src/*.c)
file(GLOB_RECURSE SHSM_STAMPED_SOURCES CONFIGURE_DEPENDS core/src/* core/include/* lib/*.c lib/*.h tools/*.c tools/*.h)
set(SHSM_TOOLS_STAMP_C ${CMAKE_CURRENT_BINARY_DIR}/gen/tools_stamp.c)
add_custom_command(OUTPUT ${SHSM_TOOLS_STAMP_C}
    COMMAND ${CMAKE_COMMAND} -DROOT=${CMAKE_CURRENT_SOURCE_DIR} -DOUT=${SHSM_TOOLS_STAMP_C}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/tools_stamp.cmake
    DEPENDS ${SHSM_STAMPED_SOURCES} ${CMAKE_CURRENT_SOURCE_DIR}/cmake/tools_stamp.cmake
    COMMENT "tools stamp" VERBATIM)
add_library(shsmcore STATIC ${SHSMCORE_SOURCES} ${SHSM_TOOLS_STAMP_C})
target_include_directories(shsmcore PUBLIC core/include)
target_compile_options(shsmcore PRIVATE ${SHSM_C_FLAGS})
target_compile_definitions(shsmcore PUBLIC ${SHSM_C_DEFS})
target_link_libraries(shsmcore PUBLIC bcrypt shlwapi advapi32 dbghelp)

# Vendored libraries: third_party/<name>/ with its own CMakeLists.txt, which defines its static target and registers
# it with shsm_register_third_party(<target>) (cmake/shsm_third_party.cmake). Every registered target is linked into
# shsmcore below, once all the directories have been added. Adding a vendored library edits no shared file.
include(cmake/shsm_third_party.cmake)
file(GLOB SHSM_THIRD_PARTY_CMAKELISTS CONFIGURE_DEPENDS third_party/*/CMakeLists.txt)
list(SORT SHSM_THIRD_PARTY_CMAKELISTS)
foreach(lists ${SHSM_THIRD_PARTY_CMAKELISTS})
    get_filename_component(dir "${lists}" DIRECTORY)
    add_subdirectory("${dir}")
endforeach()

# The tool logic: lib/<area>_*.c with one header each.
file(GLOB SHSMTOOLS_SOURCES CONFIGURE_DEPENDS lib/*.c)
if(SHSMTOOLS_SOURCES)
    add_library(shsmtools STATIC ${SHSMTOOLS_SOURCES})
    target_include_directories(shsmtools PUBLIC lib)
    target_compile_options(shsmtools PRIVATE ${SHSM_C_FLAGS})
    target_link_libraries(shsmtools PUBLIC shsmcore)
else()
    add_library(shsmtools INTERFACE)
    target_include_directories(shsmtools INTERFACE lib)
    target_link_libraries(shsmtools INTERFACE shsmcore)
endif()

# Unit tests of the shared pieces: core/tests/<name>.c and tests/<name>.c -> tests/test_<name>.exe, each one a CTest
# test (labels core and lib) run from the repository root.
enable_testing()
set(SHSM_TEST_TIMEOUT 600 CACHE STRING "seconds a CTest test may run before CTest kills it")
foreach(group core lib)
    if(group STREQUAL "core")
        file(GLOB SHSM_TESTS CONFIGURE_DEPENDS core/tests/*.c)
    else()
        file(GLOB SHSM_TESTS CONFIGURE_DEPENDS tests/*.c)
    endif()
    foreach(src ${SHSM_TESTS})
        get_filename_component(name "${src}" NAME_WE)
        add_executable(test_${name} ${src})
        set_target_properties(test_${name} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${SHSM_TOOLS_OUT}/tests$<0:>")
        target_compile_options(test_${name} PRIVATE ${SHSM_C_FLAGS})
        target_link_libraries(test_${name} PRIVATE shsmtools)
        add_test(NAME ${name} COMMAND test_${name} WORKING_DIRECTORY "${SHSM_REPO}")
        # A test that runs past this was waiting for something: CTest kills it (crash and hang policy, base.h).
        set_tests_properties(${name} PROPERTIES TIMEOUT ${SHSM_TEST_TIMEOUT} LABELS ${group})
    endforeach()
endforeach()

# The child process of shsm_cli_test (tests/helpers/shsm_child.c).
add_executable(shsm_child tests/helpers/shsm_child.c)
set_target_properties(shsm_child PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${SHSM_TOOLS_OUT}/tests$<0:>")
target_compile_options(shsm_child PRIVATE ${SHSM_C_FLAGS})
target_link_libraries(shsm_child PRIVATE shsmtools)
add_dependencies(test_shsm_cli_test shsm_child)

# One directory per tool: tools/<name>/ -> library tool_<name> (its *.c but main.c), <name>.exe (main.c), tests
# test_<name>_<file> (tools/<name>/tests/*.c, CTest label <name>). A directory with its own CMakeLists.txt is
# add_subdirectory'd instead; it calls the same shsm_add_tool() (cmake/shsm_tool.cmake documents the interface).
# Adding a tool edits no shared file.
include(cmake/shsm_tool.cmake)
file(GLOB SHSM_TOOL_DIRS LIST_DIRECTORIES true CONFIGURE_DEPENDS tools/*)
# Also re-run the configure step when a tool directory gains or loses its own CMakeLists.txt.
file(GLOB SHSM_TOOL_CMAKELISTS CONFIGURE_DEPENDS tools/*/CMakeLists.txt)
list(SORT SHSM_TOOL_DIRS)
foreach(dir ${SHSM_TOOL_DIRS})
    if(NOT IS_DIRECTORY "${dir}")
        continue()
    endif()
    get_filename_component(name "${dir}" NAME)
    if(EXISTS "${dir}/CMakeLists.txt")
        add_subdirectory("${dir}")
    else()
        shsm_add_tool(${name} DIR "${dir}")
    endif()
endforeach()

# Every executable enters through AVP_MAIN -> avp_start -> avp_crash_quiet_init (cmake/entry_points.cmake).
add_test(NAME entry_points COMMAND ${CMAKE_COMMAND} -DROOT=${CMAKE_CURRENT_SOURCE_DIR}
         -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/entry_points.cmake)
set_tests_properties(entry_points PROPERTIES TIMEOUT ${SHSM_TEST_TIMEOUT})

# Every registered vendored library (shsm_register_third_party), resolved after every subdirectory has been added:
# linked into shsmcore, so shsmtools, the tools and the tests all link it and see its PUBLIC include directories.
get_property(SHSM_THIRD_PARTY_LIBS GLOBAL PROPERTY SHSM_THIRD_PARTY_LIBS)
if(SHSM_THIRD_PARTY_LIBS)
    target_link_libraries(shsmcore PUBLIC ${SHSM_THIRD_PARTY_LIBS})
endif()
