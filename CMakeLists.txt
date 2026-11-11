cmake_minimum_required(VERSION 3.28)

# ---------------------------------------------------------------------------
# Release is the default build type. Set before project() so it applies to
# single-config generators; multi-config generators choose per build.
# A debug build is an explicit opt-in: -DCMAKE_BUILD_TYPE=Debug.
# ---------------------------------------------------------------------------
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type (Release by default)" FORCE)
    message(STATUS "zorlduh: CMAKE_BUILD_TYPE defaulted to Release.")
endif()

project(zorlduh
    VERSION 0.0.1
    DESCRIPTION "A top-down action-adventure on the Polyrhythm engine"
    LANGUAGES CXX)

# ---------------------------------------------------------------------------
# Compiler floor: the engine's C++20 minimums. An older compiler fails here,
# at configure time, with the version it found.
# ---------------------------------------------------------------------------
set(_zorlduh_id  "${CMAKE_CXX_COMPILER_ID}")
set(_zorlduh_ver "${CMAKE_CXX_COMPILER_VERSION}")
if(_zorlduh_id STREQUAL "GNU" AND _zorlduh_ver VERSION_LESS 13)
    message(FATAL_ERROR "zorlduh requires GCC 13+ (found ${_zorlduh_ver}).")
elseif(_zorlduh_id STREQUAL "Clang" AND _zorlduh_ver VERSION_LESS 16)
    message(FATAL_ERROR "zorlduh requires Clang 16+ (found ${_zorlduh_ver}).")
elseif(_zorlduh_id STREQUAL "AppleClang" AND _zorlduh_ver VERSION_LESS 15)
    message(FATAL_ERROR "zorlduh requires AppleClang 15+ / Xcode 15+ (found ${_zorlduh_ver}).")
elseif(_zorlduh_id STREQUAL "MSVC" AND _zorlduh_ver VERSION_LESS 19.38)
    message(FATAL_ERROR "zorlduh requires MSVC 19.38+ / VS 2022 17.8+ (found ${_zorlduh_ver}).")
endif()

enable_testing()

# ---------------------------------------------------------------------------
# The Polyrhythm engine, built from the engine/ submodule. As a subproject it
# builds none of its own tests or examples.
# ---------------------------------------------------------------------------
add_subdirectory(engine)

# Modules: systems that include nothing from this project, each with its own
# CMakeLists.txt. The first three depend on the engine alone; WorldKit composes
# LayerKit and CameraKit. Their example programs are built so the examples their
# guides walk through always compile.
set(LAYERKIT_BUILD_EXAMPLES ON CACHE BOOL "" FORCE)
add_subdirectory(modules/LayerKit)  # LayerKit::LayerKit — SpriteLayer, TileLayer, RasterLayer, ImageLayer
set(TOPDOWNMOVEMENT_BUILD_EXAMPLES ON CACHE BOOL "" FORCE)
add_subdirectory(modules/TopDownMovement)  # TopDownMovement::TopDownMovement — Walker, WalkerConfig, CollisionShape, WalkerSprite
set(CAMERAKIT_BUILD_EXAMPLES ON CACHE BOOL "" FORCE)
add_subdirectory(modules/CameraKit)  # CameraKit::CameraKit — View, Camera, follow
set(WORLDKIT_BUILD_EXAMPLES ON CACHE BOOL "" FORCE)
add_subdirectory(modules/WorldKit)  # WorldKit::WorldKit — Area, Opening, Layout, Camera, AreaSolids, WorldLayers, Fade

# Warnings apply to this project's targets only; the engine keeps its own flags.
if(MSVC)
    set(_zorlduh_warn /W4)
else()
    set(_zorlduh_warn -Wall -Wextra -Wpedantic)
endif()

# ---------------------------------------------------------------------------
# zorlduh-lib holds every source file except main.cpp, so the game and each
# test executable link the same code.
# ---------------------------------------------------------------------------
add_library(zorlduh-lib STATIC
    src/assets/assets.cpp
    src/config/config.cpp
    src/game/areas.cpp
    src/game/player.cpp
    src/game/sim.cpp
    src/input/input.cpp
    src/render/player.cpp)
target_include_directories(zorlduh-lib PUBLIC src)
target_compile_features(zorlduh-lib PUBLIC cxx_std_20)
target_compile_options(zorlduh-lib PRIVATE ${_zorlduh_warn})
target_link_libraries(zorlduh-lib PUBLIC retropp::engine LayerKit::LayerKit TopDownMovement::TopDownMovement
                                          CameraKit::CameraKit WorldKit::WorldKit)

# ---------------------------------------------------------------------------
# The game executable.
# ---------------------------------------------------------------------------
add_executable(zorlduh src/main.cpp)
target_compile_features(zorlduh PRIVATE cxx_std_20)
target_compile_options(zorlduh PRIVATE ${_zorlduh_warn})
target_link_libraries(zorlduh PRIVATE zorlduh-lib)

# The shipped binary is lean: the linker drops unreferenced code, and a
# Release build is stripped of local symbols after linking. The engine applies
# these to its own targets only, so the executable sets them here.
if(APPLE)
    target_link_options(zorlduh PRIVATE -Wl,-dead_strip)
elseif(_zorlduh_id MATCHES "GNU|Clang")
    target_compile_options(zorlduh PRIVATE -ffunction-sections -fdata-sections)
    target_link_options(zorlduh PRIVATE -Wl,--gc-sections)
elseif(MSVC)
    target_compile_options(zorlduh PRIVATE /Gy)
    target_link_options(zorlduh PRIVATE /OPT:REF /OPT:ICF)
endif()

if(NOT MSVC AND CMAKE_BUILD_TYPE STREQUAL "Release")
    add_custom_command(TARGET zorlduh POST_BUILD
        COMMAND "${CMAKE_STRIP}" -x "$<TARGET_FILE:zorlduh>"
        COMMENT "zorlduh: stripping symbols from the Release binary"
        VERBATIM)
endif()

# ---------------------------------------------------------------------------
# Tests: GoogleTest, fetched at configure time (the engine does not export it
# to a consuming project). Each test file is its own executable, registered
# with ctest through gtest_discover_tests.
# ---------------------------------------------------------------------------
include(FetchContent)
set(gtest_force_shared_crt ON  CACHE BOOL "" FORCE)  # match the MSVC runtime
set(INSTALL_GTEST          OFF CACHE BOOL "" FORCE)  # nothing is installed
FetchContent_Declare(googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.17.0)
FetchContent_MakeAvailable(googletest)
include(GoogleTest)

# zorlduh_add_test(<name>) builds tests/test_<name>.cpp as zorlduh-test-<name>.
function(zorlduh_add_test name)
    add_executable(zorlduh-test-${name} tests/test_${name}.cpp)
    target_compile_features(zorlduh-test-${name} PRIVATE cxx_std_20)
    target_compile_options(zorlduh-test-${name} PRIVATE ${_zorlduh_warn})
    target_link_libraries(zorlduh-test-${name} PRIVATE zorlduh-lib GTest::gtest_main)
    gtest_discover_tests(zorlduh-test-${name})
endfunction()

zorlduh_add_test(smoke)
zorlduh_add_test(layers)
zorlduh_add_test(raster_layers)
zorlduh_add_test(assets)
zorlduh_add_test(TopDownMovement)
zorlduh_add_test(player)
zorlduh_add_test(CameraKit)
zorlduh_add_test(WorldKit)
zorlduh_add_test(areas)
# The asset tests decode the sheets from the source tree; the maps come from the bytes baked into
# the test binary.
target_compile_definitions(zorlduh-test-assets PRIVATE
    ZORLDUH_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets")

# The committed files under assets/ are exactly what tools/placeholder_art.py writes.
find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(NAME zorlduh-placeholder-art-matches-script
    COMMAND Python3::Interpreter "${CMAKE_CURRENT_SOURCE_DIR}/tools/placeholder_art.py" --check)
