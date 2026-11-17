# The ActorKit module: header-only, depending on the Polyrhythm engine, LayerKit, WorldKit and ScriptKit.
# A project that has those as the targets retropp::engine, LayerKit::LayerKit, WorldKit::WorldKit and
# ScriptKit::ScriptKit adds this directory and links ActorKit::ActorKit.
add_library(ActorKit INTERFACE)
add_library(ActorKit::ActorKit ALIAS ActorKit)
target_include_directories(ActorKit INTERFACE include)
target_compile_features(ActorKit INTERFACE cxx_std_20)
target_link_libraries(ActorKit INTERFACE retropp::engine LayerKit::LayerKit WorldKit::WorldKit ScriptKit::ScriptKit)

# The example program, as the target ActorKit-example-getting-started. On by default when this directory
# is the top-level project; a project that adds it turns it on with ACTORKIT_BUILD_EXAMPLES. The example
# moves its actors with TopDownMovement's walkers, so it needs the target TopDownMovement::TopDownMovement.
option(ACTORKIT_BUILD_EXAMPLES "Build the ActorKit module's example programs" ${PROJECT_IS_TOP_LEVEL})
if(ACTORKIT_BUILD_EXAMPLES)
    add_executable(ActorKit-example-getting-started examples/getting_started.cpp)
    target_link_libraries(ActorKit-example-getting-started PRIVATE ActorKit::ActorKit TopDownMovement::TopDownMovement)
    # The example reads its sheets and palettes from examples/assets/ in this directory.
    target_compile_definitions(ActorKit-example-getting-started PRIVATE
        ACTORKIT_EXAMPLE_ASSETS="${CMAKE_CURRENT_SOURCE_DIR}/examples/assets")
    if(MSVC)
        target_compile_options(ActorKit-example-getting-started PRIVATE /W4)
    else()
        target_compile_options(ActorKit-example-getting-started PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    # Lean like any program that runs: unreferenced code dropped at link, local symbols stripped from
    # a Release build.
    if(APPLE)
        target_link_options(ActorKit-example-getting-started PRIVATE -Wl,-dead_strip)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(ActorKit-example-getting-started PRIVATE -ffunction-sections -fdata-sections)
        target_link_options(ActorKit-example-getting-started PRIVATE -Wl,--gc-sections)
    elseif(MSVC)
        target_compile_options(ActorKit-example-getting-started PRIVATE /Gy)
        target_link_options(ActorKit-example-getting-started PRIVATE /OPT:REF /OPT:ICF)
    endif()
    if(NOT MSVC AND CMAKE_BUILD_TYPE STREQUAL "Release")
        add_custom_command(TARGET ActorKit-example-getting-started POST_BUILD
            COMMAND "${CMAKE_STRIP}" -x "$<TARGET_FILE:ActorKit-example-getting-started>"
            VERBATIM)
    endif()
endif()
