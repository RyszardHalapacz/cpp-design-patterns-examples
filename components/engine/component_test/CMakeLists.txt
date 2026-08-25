add_executable(engine_component_tests EngineComponentTest.cpp)

target_link_libraries(engine_component_tests
    PRIVATE
        engine_lib
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(engine_component_tests DISCOVERY_MODE PRE_TEST)
