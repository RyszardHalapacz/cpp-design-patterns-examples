add_executable(patterns_tests
    test_services.cpp
    test_strategy.cpp
    test_session.cpp
    test_gui_owning.cpp
    test_establish_signals.cpp
)

target_link_libraries(patterns_tests PRIVATE patterns_lib GTest::gtest_main GTest::gmock)

target_compile_definitions(patterns_tests PRIVATE
    WZORCE_BINARY_DIR="${CMAKE_BINARY_DIR}"
)

include(GoogleTest)
gtest_discover_tests(patterns_tests)
