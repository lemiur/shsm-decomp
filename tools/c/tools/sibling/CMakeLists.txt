# sibling links the twins library for its fingerprints (twins_fingerprint: the exact and ref normalisation).
# It links the names library for `apply`: the rename planner that names.exe rename runs.
# It links the witness library for a foreign-architecture sibling (sibling_foreign.c): the PS2 model and the
# cross-architecture correspondence.
shsm_add_tool(sibling LIBS tool_twins tool_names tool_witness)
