# wall links match for its state helpers (as wall.py imports tools/match.py) and eqcheck for the prover (as wall.py
# imports tools/eqcheck.py; eqcheck links wall back: a cycle of static libraries, which CMake repeats on the link line).
shsm_add_tool(wall LIBS tool_match tool_eqcheck)
