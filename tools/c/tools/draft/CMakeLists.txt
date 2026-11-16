# draft links the libraries of the tools draft.py imports: twins (World.tw), unit_plan (plan_for), hashes
# (var_names) and classes (stale_reason), and brief (packet's page, brief_page; brief links draft's libraries back
# only through its own, so no cycle). The grader runs build/tools/check_rules.exe (DEVIATIONS.md, D-draft-7).
shsm_add_tool(draft LIBS tool_twins tool_unit tool_unit_plan tool_hashes tool_classes tool_brief)

# tests/link_test.c also links every object of those libraries (/WHOLEARCHIVE), so a symbol two of them define
# fails the build here rather than in a later phase.
if(TARGET test_draft_link_test)
    foreach(lib tool_twins tool_unit_plan tool_hashes tool_classes tool_draft)
        target_link_options(test_draft_link_test PRIVATE "/WHOLEARCHIVE:$<TARGET_FILE:${lib}>")
    endforeach()
endif()
