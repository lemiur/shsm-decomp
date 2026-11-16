# ledger links the rule checker's library: run_checker runs check_rules --json in-process (ledger.py runs
# tools/rules/check_rules.py).
shsm_add_tool(ledger LIBS tool_check_rules)
