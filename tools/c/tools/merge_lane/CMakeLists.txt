# merge_lane links the gate's library (the changes-step helpers in gate_regress), the rule checker and the class
# scanner (the rules step), and score_history's source index (index_source).
# Its tests are added below (not by shsm_add_tool): they link the test-only stubs library (tests/ml_stubs.c),
# so it never sits beside the production tools.
shsm_add_tool(merge_lane LIBS tool_gate tool_check_rules tool_classes tool_score_history TESTS "")

add_library(merge_lane_stubs STATIC "${CMAKE_CURRENT_SOURCE_DIR}/tests/ml_stubs.c")
shsm_target_flags(merge_lane_stubs)
target_include_directories(merge_lane_stubs PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/tests")
target_link_libraries(merge_lane_stubs PUBLIC tool_merge_lane tool_changes_fmt)

file(GLOB ML_TESTS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tests/*_test.c")
foreach(src ${ML_TESTS})
    shsm_add_test(merge_lane ${src} LIBS merge_lane_stubs)
endforeach()
