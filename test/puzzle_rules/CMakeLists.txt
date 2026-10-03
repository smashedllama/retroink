add_executable(PuzzleRulesTest
  PuzzleRulesTest.cpp
)

target_link_libraries(PuzzleRulesTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

target_include_directories(PuzzleRulesTest PRIVATE
  ${REPO_ROOT}/src
)

gtest_discover_tests(PuzzleRulesTest)
