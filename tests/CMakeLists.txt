include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(patterns_tests
    test_services.cpp
    test_strategy.cpp
    test_engine.cpp
    test_session.cpp
    test_gui.cpp
)

target_link_libraries(patterns_tests PRIVATE patterns_lib GTest::gtest_main)

target_compile_definitions(patterns_tests PRIVATE
    WZORCE_BINARY_DIR="${CMAKE_BINARY_DIR}"
)

include(GoogleTest)
gtest_discover_tests(patterns_tests)
