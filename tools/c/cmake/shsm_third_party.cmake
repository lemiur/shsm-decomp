# Vendored libraries (tools/c/third_party/<name>/), included by the top-level CMakeLists.txt before it adds every
# third_party/*/CMakeLists.txt (glob, CONFIGURE_DEPENDS). A vendored library's own CMakeLists.txt defines its static
# target, publishes its include directories and registers itself with one line:
#     add_library(<target> STATIC <sources>...)
#     target_include_directories(<target> PUBLIC <include dirs>)
#     shsm_register_third_party(<target>)
# Registration applies the third-party compile flags (warnings off, no /WX: not our code) and appends the target to
# the global property SHSM_THIRD_PARTY_LIBS; the top-level CMakeLists.txt links every registered target into shsmcore
# (PUBLIC), so shsmtools, every tool and every test link it and see its include directories.
# Adding a vendored library edits no shared file.

# The compile flags of vendored code: warnings off and not errors, UTF-8 sources.
function(shsm_third_party_flags target)
    target_compile_options(${target} PRIVATE /W0 /WX- /utf-8)
endfunction()

function(shsm_register_third_party target)
    if(NOT TARGET ${target})
        message(FATAL_ERROR "shsm_register_third_party: no target ${target} (define it with add_library first)")
    endif()
    get_target_property(type ${target} TYPE)
    if(NOT type STREQUAL "INTERFACE_LIBRARY")
        shsm_third_party_flags(${target})
    endif()
    set_property(GLOBAL APPEND PROPERTY SHSM_THIRD_PARTY_LIBS ${target})
endfunction()
