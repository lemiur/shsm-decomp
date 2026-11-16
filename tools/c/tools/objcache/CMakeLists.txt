# objcache: the tool (cmake/shsm_tool.cmake) and the fake compiler its wrapper tests run: objcache_fake_mwcc.exe
# (tests/fake/fake_mwcc.c), built into the tests output folder so it never sits beside the production tools.
shsm_add_tool(objcache)

add_executable(objcache_fake_mwcc "${CMAKE_CURRENT_SOURCE_DIR}/tests/fake/fake_mwcc.c")
set_target_properties(objcache_fake_mwcc PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${SHSM_TOOLS_OUT}/tests$<0:>")
shsm_target_flags(objcache_fake_mwcc)
target_link_libraries(objcache_fake_mwcc PRIVATE shsmtools)
add_dependencies(test_objcache_wrapper_test objcache_fake_mwcc objcache)
