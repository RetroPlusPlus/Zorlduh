# The WorldKit module: header-only, in two targets that share the include directory.
#
#   WorldKit::WorldKit  the world, its solids, its camera and its layers (headers "WorldKit/…"), on the
#                       targets retropp::engine, LayerKit::LayerKit and CameraKit::CameraKit.
#   ActorKit::ActorKit  the actors of a WorldKit world (headers "ActorKit/…"), on WorldKit::WorldKit and
#                       the targets LayerKit::LayerKit and ScriptKit::ScriptKit. Defined when the project
#                       has the target ScriptKit::ScriptKit as this directory is added. "WorldKit/scene.h",
#                       the scene, includes ActorKit's headers: a game that includes it links
#                       ActorKit::ActorKit.
#
# A project adds this directory and links the target for the level it uses.
add_library(WorldKit INTERFACE)
add_library(WorldKit::WorldKit ALIAS WorldKit)
target_include_directories(WorldKit INTERFACE include)
target_compile_features(WorldKit INTERFACE cxx_std_20)
target_link_libraries(WorldKit INTERFACE retropp::engine LayerKit::LayerKit CameraKit::CameraKit)

if(TARGET ScriptKit::ScriptKit)
    add_library(ActorKit INTERFACE)
    add_library(ActorKit::ActorKit ALIAS ActorKit)
    target_include_directories(ActorKit INTERFACE include)
    target_compile_features(ActorKit INTERFACE cxx_std_20)
    target_link_libraries(ActorKit INTERFACE WorldKit::WorldKit LayerKit::LayerKit ScriptKit::ScriptKit)
endif()

# Warnings, dead-code removal at link and a stripped Release build, for each example program.
function(worldkit_example _target)
    if(MSVC)
        target_compile_options(${_target} PRIVATE /W4)
    else()
        target_compile_options(${_target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    if(APPLE)
        target_link_options(${_target} PRIVATE -Wl,-dead_strip)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${_target} PRIVATE -ffunction-sections -fdata-sections)
        target_link_options(${_target} PRIVATE -Wl,--gc-sections)
    elseif(MSVC)
        target_compile_options(${_target} PRIVATE /Gy)
        target_link_options(${_target} PRIVATE /OPT:REF /OPT:ICF)
    endif()
    if(NOT MSVC AND CMAKE_BUILD_TYPE STREQUAL "Release")
        add_custom_command(TARGET ${_target} POST_BUILD
            COMMAND "${CMAKE_STRIP}" -x "$<TARGET_FILE:${_target}>"
            VERBATIM)
    endif()
endfunction()

# The example programs, on by default when this directory is the top-level project; a project that adds
# it turns them on with WORLDKIT_BUILD_EXAMPLES.
#
#   WorldKit-example-getting-started  the castle, from docs/getting-started.md. Built when the target
#                                     ActorKit::ActorKit exists: the castle is run by the scene, and its
#                                     inhabitants move with a mover of its own.
#   WorldKit-example-farm             the farmyard, from docs/actors/getting-started.md. Built when the
#                                     targets ActorKit::ActorKit and TopDownMovement::TopDownMovement
#                                     exist: its actors walk with TopDownMovement's walkers.
option(WORLDKIT_BUILD_EXAMPLES "Build the WorldKit module's example programs" ${PROJECT_IS_TOP_LEVEL})
if(WORLDKIT_BUILD_EXAMPLES)
    if(TARGET ActorKit::ActorKit)
        add_executable(WorldKit-example-getting-started examples/getting_started.cpp)
        target_link_libraries(WorldKit-example-getting-started PRIVATE ActorKit::ActorKit)
        worldkit_example(WorldKit-example-getting-started)
    endif()

    if(TARGET ActorKit::ActorKit AND TARGET TopDownMovement::TopDownMovement)
        add_executable(WorldKit-example-farm examples/farm/farm.cpp)
        target_link_libraries(WorldKit-example-farm PRIVATE ActorKit::ActorKit TopDownMovement::TopDownMovement)
        # The farm reads its sheets and palettes from examples/farm/assets/ in this directory.
        target_compile_definitions(WorldKit-example-farm PRIVATE
            WORLDKIT_FARM_ASSETS="${CMAKE_CURRENT_SOURCE_DIR}/examples/farm/assets")
        # The engine compiles and embeds every shader the farm registers by path (the pond's ripple,
        # examples/farm/shaders/ripple_ring.frag.hlsl); the paths resolve against this directory.
        retropp_autocompile_shaders(WorldKit-example-farm)
        worldkit_example(WorldKit-example-farm)
    endif()
endif()
