add_executable(engine_tests
    test_engine.cpp
)

target_link_libraries(engine_tests PRIVATE engine_lib GTest::gtest_main GTest::gmock)

include(GoogleTest)
gtest_discover_tests(engine_tests)
