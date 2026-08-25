# ─── Engine component ─────────────────────────────────────────────────────────

set(ENGINE_VERSION "1.0.2")

configure_file(engine.yml.in
    ${CMAKE_BINARY_DIR}/components/engine/engine.yml @ONLY)

add_library(engine_lib INTERFACE)

target_include_directories(engine_lib INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(engine_lib INTERFACE core_lib)

target_compile_definitions(engine_lib INTERFACE
    ENGINE_VERSION_STR="${ENGINE_VERSION}"
    ENGINE_MANIFEST_PATH="${CMAKE_BINARY_DIR}/components/engine/engine.yml"
)

# ─── Tests ────────────────────────────────────────────────────────────────────
add_subdirectory(tests)
