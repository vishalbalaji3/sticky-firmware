add_executable(WsFrameTest WsFrameTest.cpp)

target_include_directories(WsFrameTest PRIVATE ../../src)

target_link_libraries(WsFrameTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(WsFrameTest)
