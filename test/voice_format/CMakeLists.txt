add_executable(VoiceFormatTest VoiceFormatTest.cpp)

target_include_directories(VoiceFormatTest PRIVATE ../../src)

target_link_libraries(VoiceFormatTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(VoiceFormatTest)
