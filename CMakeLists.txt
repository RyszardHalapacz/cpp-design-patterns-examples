cmake_minimum_required(VERSION 3.20)
project(wzorce LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(-Wall -Wextra -Wpedantic)

# ─── Static library ──────────────────────────────────────────────────────────
add_library(wzorce_lib STATIC
    src/services/Logger.cpp
    src/services/FileLogger.cpp
    src/services/DoSomething.cpp

    src/strategy/AscendingSortStrategy.cpp
    src/strategy/DescendingSortStrategy.cpp
    src/strategy/BubbleSortStrategy.cpp
    src/strategy/SortStrategyFactory.cpp

    src/engine/Engine.cpp

    src/session/SessionManagement.cpp
    src/session/SessionEstablisher.cpp
    src/session/SessionAuditObserver.cpp

    src/gui/CommandBatchBuilder.cpp
    src/gui/DummyGui.cpp

    src/config/Configurator.cpp
)

target_include_directories(wzorce_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# ─── Main executable ─────────────────────────────────────────────────────────
add_executable(wzorce src/main.cpp)
target_link_libraries(wzorce PRIVATE wzorce_lib)

# ─── Tests ───────────────────────────────────────────────────────────────────
enable_testing()
add_subdirectory(tests)
