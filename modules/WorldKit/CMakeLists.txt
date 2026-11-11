# The WorldKit module: header-only, depending on the Polyrhythm engine, LayerKit and CameraKit. A
# project that has those as the targets retropp::engine, LayerKit::LayerKit and CameraKit::CameraKit
# adds this directory and links WorldKit::WorldKit.
add_library(WorldKit INTERFACE)
add_library(WorldKit::WorldKit ALIAS WorldKit)
target_include_directories(WorldKit INTERFACE include)
target_compile_features(WorldKit INTERFACE cxx_std_20)
target_link_libraries(WorldKit INTERFACE retropp::engine LayerKit::LayerKit CameraKit::CameraKit)

# The example program from docs/getting-started.md, as the target WorldKit-example-getting-started.
# On by default when this directory is the top-level project; a project that adds it turns it on with
# WORLDKIT_BUILD_EXAMPLES.
option(WORLDKIT_BUILD_EXAMPLES "Build the WorldKit module's example programs" ${PROJECT_IS_TOP_LEVEL})
if(WORLDKIT_BUILD_EXAMPLES)
    add_executable(WorldKit-example-getting-started examples/getting_started.cpp)
    target_link_libraries(WorldKit-example-getting-started PRIVATE WorldKit::WorldKit)
    if(MSVC)
        target_compile_options(WorldKit-example-getting-started PRIVATE /W4)
    else()
        target_compile_options(WorldKit-example-getting-started PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    # Lean like any program that runs: unreferenced code dropped at link, local symbols stripped from
    # a Release build.
    if(APPLE)
        target_link_options(WorldKit-example-getting-started PRIVATE -Wl,-dead_strip)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(WorldKit-example-getting-started PRIVATE -ffunction-sections -fdata-sections)
        target_link_options(WorldKit-example-getting-started PRIVATE -Wl,--gc-sections)
    elseif(MSVC)
        target_compile_options(WorldKit-example-getting-started PRIVATE /Gy)
        target_link_options(WorldKit-example-getting-started PRIVATE /OPT:REF /OPT:ICF)
    endif()
    if(NOT MSVC AND CMAKE_BUILD_TYPE STREQUAL "Release")
        add_custom_command(TARGET WorldKit-example-getting-started POST_BUILD
            COMMAND "${CMAKE_STRIP}" -x "$<TARGET_FILE:WorldKit-example-getting-started>"
            VERBATIM)
    endif()
endif()
