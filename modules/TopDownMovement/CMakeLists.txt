# The TopDownMovement module: header-only, depending on the Polyrhythm engine alone. A project that has the
# engine as the target retropp::engine adds this directory and links TopDownMovement::TopDownMovement.
add_library(TopDownMovement INTERFACE)
add_library(TopDownMovement::TopDownMovement ALIAS TopDownMovement)
target_include_directories(TopDownMovement INTERFACE include)
target_compile_features(TopDownMovement INTERFACE cxx_std_20)
target_link_libraries(TopDownMovement INTERFACE retropp::engine)

# The example program from docs/getting-started.md, as the target TopDownMovement-example-getting-started.
# On by default when this directory is the top-level project; a project that adds it turns it on with
# TOPDOWNMOVEMENT_BUILD_EXAMPLES.
option(TOPDOWNMOVEMENT_BUILD_EXAMPLES "Build the TopDownMovement module's example programs" ${PROJECT_IS_TOP_LEVEL})
if(TOPDOWNMOVEMENT_BUILD_EXAMPLES)
    add_executable(TopDownMovement-example-getting-started examples/getting_started.cpp)
    target_link_libraries(TopDownMovement-example-getting-started PRIVATE TopDownMovement::TopDownMovement)
    if(MSVC)
        target_compile_options(TopDownMovement-example-getting-started PRIVATE /W4)
    else()
        target_compile_options(TopDownMovement-example-getting-started PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    # Lean like any program that runs: unreferenced code dropped at link, local symbols stripped from
    # a Release build.
    if(APPLE)
        target_link_options(TopDownMovement-example-getting-started PRIVATE -Wl,-dead_strip)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(TopDownMovement-example-getting-started PRIVATE -ffunction-sections -fdata-sections)
        target_link_options(TopDownMovement-example-getting-started PRIVATE -Wl,--gc-sections)
    elseif(MSVC)
        target_compile_options(TopDownMovement-example-getting-started PRIVATE /Gy)
        target_link_options(TopDownMovement-example-getting-started PRIVATE /OPT:REF /OPT:ICF)
    endif()
    if(NOT MSVC AND CMAKE_BUILD_TYPE STREQUAL "Release")
        add_custom_command(TARGET TopDownMovement-example-getting-started POST_BUILD
            COMMAND "${CMAKE_STRIP}" -x "$<TARGET_FILE:TopDownMovement-example-getting-started>"
            VERBATIM)
    endif()
endif()
