add_executable(AutoSyncPolicyTest AutoSyncPolicyTest.cpp)

target_include_directories(AutoSyncPolicyTest PRIVATE ../../src)

target_link_libraries(AutoSyncPolicyTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(AutoSyncPolicyTest)
