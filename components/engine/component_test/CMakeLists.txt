add_executable(engine_component_tests EngineComponentTest.cpp)

# Needed so that #include "scenario/..." resolves relative to this directory.
target_include_directories(engine_component_tests
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(engine_component_tests
    PRIVATE
        engine_lib
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(engine_component_tests DISCOVERY_MODE PRE_TEST)
