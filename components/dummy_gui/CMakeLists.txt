# ─── DummyGui component ───────────────────────────────────────────────────────

set(DUMMY_GUI_VERSION "1.2.0")

configure_file(dummy_gui.yml.in
    ${CMAKE_BINARY_DIR}/components/dummy_gui/dummy_gui.yml @ONLY)

add_library(dummy_gui_lib STATIC
    src/CommandBatchBuilder.cpp
    src/DummyGuiAdapter.cpp
)

target_include_directories(dummy_gui_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(dummy_gui_lib PUBLIC core_lib)

target_compile_definitions(dummy_gui_lib PUBLIC
    DUMMY_GUI_VERSION_STR="${DUMMY_GUI_VERSION}"
    DUMMY_GUI_MANIFEST_PATH="${CMAKE_BINARY_DIR}/components/dummy_gui/dummy_gui.yml"
)

# ─── Tests ────────────────────────────────────────────────────────────────────
add_subdirectory(tests)
