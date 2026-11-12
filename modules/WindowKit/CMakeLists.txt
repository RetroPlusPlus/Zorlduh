# The WindowKit module: header-only, depending on the Polyrhythm engine, LayerKit and TextKit. A
# project that has those as the targets retropp::engine, LayerKit::LayerKit and TextKit::TextKit adds
# this directory and links WindowKit::WindowKit.
add_library(WindowKit INTERFACE)
add_library(WindowKit::WindowKit ALIAS WindowKit)
target_include_directories(WindowKit INTERFACE include)
target_compile_features(WindowKit INTERFACE cxx_std_20)
target_link_libraries(WindowKit INTERFACE retropp::engine LayerKit::LayerKit TextKit::TextKit)

# The example program from docs/getting-started.md, as the target WindowKit-example-getting-started.
# On by default when this directory is the top-level project; a project that adds it turns it on with
# WINDOWKIT_BUILD_EXAMPLES.
option(WINDOWKIT_BUILD_EXAMPLES "Build the WindowKit module's example programs" ${PROJECT_IS_TOP_LEVEL})
if(WINDOWKIT_BUILD_EXAMPLES)
    add_executable(WindowKit-example-getting-started examples/getting_started.cpp)
    target_link_libraries(WindowKit-example-getting-started PRIVATE WindowKit::WindowKit)
    # The example reads its border, font and palettes from examples/assets/ in this directory.
    target_compile_definitions(WindowKit-example-getting-started PRIVATE
        WINDOWKIT_EXAMPLE_ASSETS="${CMAKE_CURRENT_SOURCE_DIR}/examples/assets")
    if(MSVC)
        target_compile_options(WindowKit-example-getting-started PRIVATE /W4)
    else()
        target_compile_options(WindowKit-example-getting-started PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    # Lean like any program that runs: unreferenced code dropped at link, local symbols stripped from
    # a Release build.
    if(APPLE)
        target_link_options(WindowKit-example-getting-started PRIVATE -Wl,-dead_strip)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(WindowKit-example-getting-started PRIVATE -ffunction-sections -fdata-sections)
        target_link_options(WindowKit-example-getting-started PRIVATE -Wl,--gc-sections)
    elseif(MSVC)
        target_compile_options(WindowKit-example-getting-started PRIVATE /Gy)
        target_link_options(WindowKit-example-getting-started PRIVATE /OPT:REF /OPT:ICF)
    endif()
    if(NOT MSVC AND CMAKE_BUILD_TYPE STREQUAL "Release")
        add_custom_command(TARGET WindowKit-example-getting-started POST_BUILD
            COMMAND "${CMAKE_STRIP}" -x "$<TARGET_FILE:WindowKit-example-getting-started>"
            VERBATIM)
    endif()
endif()
