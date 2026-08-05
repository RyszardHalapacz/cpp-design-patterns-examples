add_executable(dummy_gui_tests
    test_gui.cpp
)

target_link_libraries(dummy_gui_tests PRIVATE dummy_gui_lib GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(dummy_gui_tests)
