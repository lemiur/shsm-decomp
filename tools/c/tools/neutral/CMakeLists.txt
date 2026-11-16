# neutral links gate's library (parse_ninja_deps, as tools/neutral.py loads gate.py for it) and gatecheck's (its ELF
# reader and the layout check, which the Python loads or runs as gatecheck.py).
shsm_add_tool(neutral LIBS tool_gate tool_gatecheck)
