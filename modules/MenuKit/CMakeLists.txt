# The MenuKit module: header-only, depending on the Polyrhythm engine, LayerKit, TextKit and WindowKit.
# A project that has those as the targets retropp::engine, LayerKit::LayerKit, TextKit::TextKit and
# WindowKit::WindowKit adds this directory and links MenuKit::MenuKit.
add_library(MenuKit INTERFACE)
add_library(MenuKit::MenuKit ALIAS MenuKit)
target_include_directories(MenuKit INTERFACE include)
target_compile_features(MenuKit INTERFACE cxx_std_20)
target_link_libraries(MenuKit INTERFACE retropp::engine LayerKit::LayerKit TextKit::TextKit WindowKit::WindowKit)

# The example program from docs/getting-started.md, as the target MenuKit-example-getting-started. On by
# default when this directory is the top-level project; a project that adds it turns it on with
# MENUKIT_BUILD_EXAMPLES.
option(MENUKIT_BUILD_EXAMPLES "Build the MenuKit module's example programs" ${PROJECT_IS_TOP_LEVEL})
if(MENUKIT_BUILD_EXAMPLES)
    add_executable(MenuKit-example-getting-started examples/getting_started.cpp)
    target_link_libraries(MenuKit-example-getting-started PRIVATE MenuKit::MenuKit)
    # The example reads its border, font and palettes from examples/assets/ in this directory.
    target_compile_definitions(MenuKit-example-getting-started PRIVATE
        MENUKIT_EXAMPLE_ASSETS="${CMAKE_CURRENT_SOURCE_DIR}/examples/assets")
    if(MSVC)
        target_compile_options(MenuKit-example-getting-started PRIVATE /W4)
    else()
        target_compile_options(MenuKit-example-getting-started PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    # Lean like any program that runs: unreferenced code dropped at link, local symbols stripped from
    # a Release build.
    if(APPLE)
        target_link_options(MenuKit-example-getting-started PRIVATE -Wl,-dead_strip)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(MenuKit-example-getting-started PRIVATE -ffunction-sections -fdata-sections)
        target_link_options(MenuKit-example-getting-started PRIVATE -Wl,--gc-sections)
    elseif(MSVC)
        target_compile_options(MenuKit-example-getting-started PRIVATE /Gy)
        target_link_options(MenuKit-example-getting-started PRIVATE /OPT:REF /OPT:ICF)
    endif()
    if(NOT MSVC AND CMAKE_BUILD_TYPE STREQUAL "Release")
        add_custom_command(TARGET MenuKit-example-getting-started POST_BUILD
            COMMAND "${CMAKE_STRIP}" -x "$<TARGET_FILE:MenuKit-example-getting-started>"
            VERBATIM)
    endif()
endif()
