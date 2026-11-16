# match links the rule checker's library (--rules runs check_rules in-process, as match.py runs tools/rules/check_rules.py)
# and gatecheck's (--data runs gatecheck layout in-process).
shsm_add_tool(match LIBS tool_check_rules tool_gatecheck)
