# fragments links the rule checker's library for its lexer and parser (rules_lex, rules_parse), as
# tools/fragments.py imports tools/rules/check_rules for `source`.
shsm_add_tool(fragments LIBS tool_check_rules)
