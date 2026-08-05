# ─── Core shared library ──────────────────────────────────────────────────────

add_library(core_lib STATIC
    src/services/Logger.cpp
    src/services/FileLogger.cpp
    src/services/DoSomething.cpp

    src/strategy/AscendingSortStrategy.cpp
    src/strategy/DescendingSortStrategy.cpp
    src/strategy/BubbleSortStrategy.cpp
    src/strategy/SortStrategyFactory.cpp

    src/manifest/ManifestWriter.cpp
)

target_include_directories(core_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(core_lib PRIVATE yaml-cpp::yaml-cpp)
