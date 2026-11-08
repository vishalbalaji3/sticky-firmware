add_executable(WriteBatcherTest WriteBatcherTest.cpp)

target_link_libraries(WriteBatcherTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(WriteBatcherTest)
