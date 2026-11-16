# levers links neutral's library (cli.first_error, as neutral reports compile errors) and match's (the state inputs'
# flags key, for tools/wall.py load_walls' staleness check).
shsm_add_tool(levers LIBS tool_neutral tool_match)
