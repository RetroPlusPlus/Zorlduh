# The CameraKit module: header-only, depending on the Polyrhythm engine alone. A project that has the
# engine as the target retropp::engine adds this directory and links CameraKit::CameraKit.
add_library(CameraKit INTERFACE)
add_library(CameraKit::CameraKit ALIAS CameraKit)
target_include_directories(CameraKit INTERFACE include)
target_compile_features(CameraKit INTERFACE cxx_std_20)
target_link_libraries(CameraKit INTERFACE retropp::engine)

# The example program from docs/getting-started.md, as the target CameraKit-example-getting-started.
# On by default when this directory is the top-level project; a project that adds it turns it on with
# CAMERAKIT_BUILD_EXAMPLES.
option(CAMERAKIT_BUILD_EXAMPLES "Build the CameraKit module's example programs" ${PROJECT_IS_TOP_LEVEL})
if(CAMERAKIT_BUILD_EXAMPLES)
    add_executable(CameraKit-example-getting-started examples/getting_started.cpp)
    target_link_libraries(CameraKit-example-getting-started PRIVATE CameraKit::CameraKit)
    if(MSVC)
        target_compile_options(CameraKit-example-getting-started PRIVATE /W4)
    else()
        target_compile_options(CameraKit-example-getting-started PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    # Lean like any program that runs: unreferenced code dropped at link, local symbols stripped from
    # a Release build.
    if(APPLE)
        target_link_options(CameraKit-example-getting-started PRIVATE -Wl,-dead_strip)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(CameraKit-example-getting-started PRIVATE -ffunction-sections -fdata-sections)
        target_link_options(CameraKit-example-getting-started PRIVATE -Wl,--gc-sections)
    elseif(MSVC)
        target_compile_options(CameraKit-example-getting-started PRIVATE /Gy)
        target_link_options(CameraKit-example-getting-started PRIVATE /OPT:REF /OPT:ICF)
    endif()
    if(NOT MSVC AND CMAKE_BUILD_TYPE STREQUAL "Release")
        add_custom_command(TARGET CameraKit-example-getting-started POST_BUILD
            COMMAND "${CMAKE_STRIP}" -x "$<TARGET_FILE:CameraKit-example-getting-started>"
            VERBATIM)
    endif()
endif()
