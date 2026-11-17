# mwtrace links wdbg (wdbg_run: a compile under the debugger in a kill-on-close job).
shsm_add_tool(mwtrace LIBS tool_wdbg)
# The real-tree tests trace units into build/mwtrace-test/<case>: one run of them at a time.
set_tests_properties(mwtrace_real_tree_test PROPERTIES RESOURCE_LOCK mwtrace_scratch TIMEOUT 900)
set_tests_properties(mwtrace_regalloc_real_test PROPERTIES RESOURCE_LOCK mwtrace_scratch TIMEOUT 900)
set_tests_properties(mwtrace_sched_real_test PROPERTIES RESOURCE_LOCK mwtrace_scratch TIMEOUT 1200)
