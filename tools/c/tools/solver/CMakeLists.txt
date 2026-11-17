# solver links wall (the classifier, the call lists, the compile helpers), levers (the definition finder and Score,
# lv_classify), sibling and witness (the read-only evidence lookups) and match (through wall: the rule checker for
# the candidate phases) and mwtrace (the inliner and register allocator traces the records state their conditions in).
shsm_add_tool(solver LIBS tool_levers tool_wall tool_match tool_sibling tool_witness tool_check_rules tool_mwtrace)
# The real-tree tests compile the same units into the solver's scratch folders (build/tmp/solver/<unit>): never at
# the same time.
set_tests_properties(solver_real_tree_test solver_phase2_real_test PROPERTIES RESOURCE_LOCK solver_scratch)
