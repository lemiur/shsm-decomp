# unit_plan links the classes tool's library: load_classes judges classes.json's input stamp with classes.h and reads
# member names with classes_prog.h (mangled_class), as tools/unit_plan.py imports tools/classes.py. It links the
# witness library for the PS2 link order of the mapped functions (witness_lookup.h wt_lookup_order).
shsm_add_tool(unit_plan LIBS tool_classes tool_witness)
