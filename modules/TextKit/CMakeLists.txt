# The TextKit module: header-only, depending on the Polyrhythm engine alone. A project that has the
# engine as the target retropp::engine adds this directory and links TextKit::TextKit.
add_library(TextKit INTERFACE)
add_library(TextKit::TextKit ALIAS TextKit)
target_include_directories(TextKit INTERFACE include)
target_compile_features(TextKit INTERFACE cxx_std_20)
target_link_libraries(TextKit INTERFACE retropp::engine)

# The example program from docs/getting-started.md, as the target TextKit-example-getting-started.
# On by default when this directory is the top-level project; a project that adds it turns it on with
# TEXTKIT_BUILD_EXAMPLES.
option(TEXTKIT_BUILD_EXAMPLES "Build the TextKit module's example programs" ${PROJECT_IS_TOP_LEVEL})
if(TEXTKIT_BUILD_EXAMPLES)
    add_executable(TextKit-example-getting-started examples/getting_started.cpp)
    target_link_libraries(TextKit-example-getting-started PRIVATE TextKit::TextKit)
    # The example reads its font and palettes from examples/assets/ in this directory.
    target_compile_definitions(TextKit-example-getting-started PRIVATE
        TEXTKIT_EXAMPLE_ASSETS="${CMAKE_CURRENT_SOURCE_DIR}/examples/assets")
    if(MSVC)
        target_compile_options(TextKit-example-getting-started PRIVATE /W4)
    else()
        target_compile_options(TextKit-example-getting-started PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    # Lean like any program that runs: unreferenced code dropped at link, local symbols stripped from
    # a Release build.
    if(APPLE)
        target_link_options(TextKit-example-getting-started PRIVATE -Wl,-dead_strip)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(TextKit-example-getting-started PRIVATE -ffunction-sections -fdata-sections)
        target_link_options(TextKit-example-getting-started PRIVATE -Wl,--gc-sections)
    elseif(MSVC)
        target_compile_options(TextKit-example-getting-started PRIVATE /Gy)
        target_link_options(TextKit-example-getting-started PRIVATE /OPT:REF /OPT:ICF)
    endif()
    if(NOT MSVC AND CMAKE_BUILD_TYPE STREQUAL "Release")
        add_custom_command(TARGET TextKit-example-getting-started POST_BUILD
            COMMAND "${CMAKE_STRIP}" -x "$<TARGET_FILE:TextKit-example-getting-started>"
            VERBATIM)
    endif()
endif()
