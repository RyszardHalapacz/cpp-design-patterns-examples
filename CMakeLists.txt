cmake_minimum_required(VERSION 3.20)
project(patterns LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(-Wall -Wextra -Wpedantic)

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

# ─── Component version numbers ───────────────────────────────────────────────
set(APP_VERSION       "1.0.0")
set(ENGINE_VERSION    "1.0.0")
set(DUMMY_GUI_VERSION "1.0.0")

configure_file(src/engine/engine.yml.in     ${CMAKE_BINARY_DIR}/src/engine/engine.yml    @ONLY)
configure_file(src/gui/dummy_gui.yml.in     ${CMAKE_BINARY_DIR}/src/gui/dummy_gui.yml    @ONLY)

# ─── Static library ──────────────────────────────────────────────────────────
add_library(patterns_lib STATIC
    src/services/Logger.cpp
    src/services/FileLogger.cpp
    src/services/DoSomething.cpp

    src/strategy/AscendingSortStrategy.cpp
    src/strategy/DescendingSortStrategy.cpp
    src/strategy/BubbleSortStrategy.cpp
    src/strategy/SortStrategyFactory.cpp

    src/session/SessionManagement.cpp
    src/session/SessionEstablisher.cpp
    src/session/SessionAuditObserver.cpp

    src/gui/CommandBatchBuilder.cpp

    src/config/Configurator.cpp

    src/manifest/ManifestWriter.cpp
)

target_include_directories(patterns_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(patterns_lib PRIVATE yaml-cpp::yaml-cpp)

target_compile_definitions(patterns_lib PUBLIC
    APP_VERSION_STR="${APP_VERSION}"
    ENGINE_VERSION_STR="${ENGINE_VERSION}"
    DUMMY_GUI_VERSION_STR="${DUMMY_GUI_VERSION}"
)

# ─── Main executable ─────────────────────────────────────────────────────────
add_executable(patterns src/main.cpp)
target_link_libraries(patterns PRIVATE patterns_lib)


# ─── Tests ───────────────────────────────────────────────────────────────────
enable_testing()
add_subdirectory(tests)
