cmake_minimum_required(VERSION 3.20)
project(patterns LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(-Wall -Wextra -Wpedantic)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ─── yaml-cpp via FetchContent ────────────────────────────────────────────────
include(FetchContent)

set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_INSTALL    OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    yaml-cpp
    GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
    GIT_TAG        yaml-cpp-0.9.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(yaml-cpp)

# ─── GoogleTest via FetchContent ─────────────────────────────────────────────
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# enable_testing() must be called before any add_subdirectory that contains tests
enable_testing()

# ─── Components ──────────────────────────────────────────────────────────────
add_subdirectory(components/core)
add_subdirectory(components/engine)
add_subdirectory(components/dummy_gui)

# ─── Static library (session + app) ──────────────────────────────────────────
set(APP_VERSION "1.1.0")

add_library(patterns_lib STATIC
    src/session/SessionManagement.cpp
    src/session/SessionCoordinator.cpp
    src/session/SessionAuditObserver.cpp
    src/config/Configurator.cpp
    src/app/Application.cpp
)

target_include_directories(patterns_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(patterns_lib PUBLIC core_lib engine_lib dummy_gui_lib)

target_compile_definitions(patterns_lib PUBLIC
    APP_VERSION_STR="${APP_VERSION}"
)

# ─── Main executable ─────────────────────────────────────────────────────────
add_executable(patterns src/main.cpp
        src/app/Application.cpp)
target_link_libraries(patterns PRIVATE patterns_lib)

# ─── Tests ───────────────────────────────────────────────────────────────────
add_subdirectory(tests)
