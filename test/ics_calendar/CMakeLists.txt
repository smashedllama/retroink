add_executable(IcsCalendarTest
  IcsCalendarTest.cpp
  ${REPO_ROOT}/lib/IcsCalendar/IcsCalendar.cpp
)

target_link_libraries(IcsCalendarTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(IcsCalendarTest)
