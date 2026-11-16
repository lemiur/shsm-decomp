# CTest test entry_points (cmake -DROOT=<tools/c> -P entry_points.cmake): the enforcement half of the crash and hang
# policy (core/include/avp/base.h). Every executable must enter through AVP_MAIN -> avp_start, whose first statement is
# avp_crash_quiet_init(); so no .c/.h under tools/c (third_party and build output excluded) may define main, wmain,
# WinMain, wWinMain or DllMain itself, and avp_start must begin with that call.
if(NOT ROOT)
    message(FATAL_ERROR "entry_points: pass -DROOT=<tools/c>")
endif()

# A definition (or declaration) with an entry point's return type at the start of a line (so a Python snippet quoted
# in a corpus header does not count).
set(entry_re "(^|\n)[ \t]*(static[ \t]+)?(int|BOOL|INT)[ \t]+((WINAPI|APIENTRY|CALLBACK|__stdcall)[ \t]+)?(wmain|main|WinMain|wWinMain|DllMain)[ \t]*[(]")
# The one legitimate definition: the AVP_MAIN macro in base.h (its wmain line follows the #define's backslash).
set(macro_re "define AVP_MAIN[(]run_fn, tool_name[)] .\n[ 	]*int wmain[(]")

file(GLOB_RECURSE sources "${ROOT}/*.c" "${ROOT}/*.h")
set(problems "")
set(scanned 0)
foreach(f ${sources})
    file(RELATIVE_PATH rel "${ROOT}" "${f}")
    if(rel MATCHES "^(third_party|build)/" OR rel MATCHES "(^|/)(CMakeFiles|gen)/")
        continue()
    endif()
    math(EXPR scanned "${scanned}+1")
    file(READ "${f}" text)
    string(REPLACE "\r\n" "\n" text "${text}")
    if(rel STREQUAL "core/include/avp/base.h")
        string(REGEX REPLACE "${macro_re}" "AVP_MAIN_DEFINITION(" text "${text}")
    endif()
    if(text MATCHES "${entry_re}")
        list(APPEND problems "${rel}: defines ${CMAKE_MATCH_6} itself; use AVP_MAIN (avp/base.h)")
    endif()
endforeach()

file(READ "${ROOT}/core/src/base.c" base_c)
set(start_re "int avp_start[(][^{]*[{][ \t\r\n]*avp_crash_quiet_init[(][)];")
if(NOT base_c MATCHES "${start_re}")
    list(APPEND problems "core/src/base.c: the first statement of avp_start is not the avp_crash_quiet_init() call")
endif()
file(READ "${ROOT}/core/include/avp/base.h" base_h)
if(NOT base_h MATCHES [=[int wmain[(]int argc, wchar_t [*][*]wargv[)] [{] return avp_start[(]]=])
    list(APPEND problems "core/include/avp/base.h: AVP_MAIN no longer defines wmain through avp_start")
endif()

if(problems)
    string(REPLACE ";" "\n  " msg "${problems}")
    message(FATAL_ERROR "entry_points: FAILED\n  ${msg}")
endif()
message(STATUS "entry_points: OK (${scanned} files scanned)")
