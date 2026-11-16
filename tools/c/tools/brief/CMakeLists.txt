# brief links the tools whose answers it stitches together, in process, as tools/brief.py imports them: query (the
# context, resolve, summarize, row_note, the notes search), twins (the fingerprint database), pick (reference_flag,
# stale_note), match (load_state), wall (find_notes, inputs_staleness; the classification itself runs
# build/tools/wall.exe as brief.py runs wall.py), unit_plan (plan_for), decomp (the stored listing), witness
# (lookup), hashes (var_names), fragments (in_function) and sibling (the Gormiti counterpart and object spans, read
# from its database when it exists).
shsm_add_tool(brief LIBS tool_query tool_twins tool_pick tool_match tool_wall tool_unit_plan tool_decomp tool_witness
              tool_hashes tool_fragments tool_sibling)
