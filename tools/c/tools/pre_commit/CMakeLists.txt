# pre_commit links the rule checker's library: the warning runs check_rules <paths> in-process (pre_commit.py runs
# tools/rules/check_rules.py).
shsm_add_tool(pre_commit LIBS tool_check_rules)
