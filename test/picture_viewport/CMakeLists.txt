add_executable(PictureViewportTest PictureViewportTest.cpp)

target_include_directories(PictureViewportTest PRIVATE ../../src)

target_link_libraries(PictureViewportTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(PictureViewportTest)
