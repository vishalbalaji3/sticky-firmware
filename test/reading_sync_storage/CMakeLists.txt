add_executable(ReadingSyncStorageTest ReadingSyncStorageTest.cpp)
target_include_directories(ReadingSyncStorageTest PRIVATE stubs ../../src ../../lib/Epub)
target_link_libraries(ReadingSyncStorageTest PRIVATE crosspoint_test_common GTest::gtest_main)
gtest_discover_tests(ReadingSyncStorageTest)
