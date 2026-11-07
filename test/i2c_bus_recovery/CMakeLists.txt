add_executable(I2cBusRecoveryTest I2cBusRecoveryTest.cpp)

target_link_libraries(I2cBusRecoveryTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(I2cBusRecoveryTest)
