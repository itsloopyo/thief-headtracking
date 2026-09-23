cmake_minimum_required(VERSION 3.20)
project(ThiefHeadTracking VERSION 0.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "ThiefHeadTracking must be built as x64. Configure with -A x64.")
endif()

if(WIN32)
    add_definitions(-DWIN32_LEAN_AND_MEAN -DNOMINMAX -D_CRT_SECURE_NO_WARNINGS)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

set(CAMERAUNLOCK_BUILD_TESTS OFF CACHE BOOL "" FORCE)
# MinHook comes from cameraunlock-core's vendored copy, which this switches on.
set(CAMERAUNLOCK_BUILD_HOOKS ON CACHE BOOL "" FORCE)
add_subdirectory(cameraunlock-core/cpp ${CMAKE_BINARY_DIR}/cameraunlock-core)

set(MOD_SOURCES
    src/dllmain.cpp
    src/config.cpp
    src/path_utils.cpp
    src/hotkeys.cpp
    src/tracking_runtime.cpp
    src/build_profile.cpp
    src/aim_marker.cpp
    src/hook_install.cpp
    src/camera_hook.cpp
    src/camera_diagnostics.cpp
    src/world_trace.cpp
    src/camera_collision.cpp
    src/aim_trace.cpp
    src/crosshair_hook.cpp
    src/game_state.cpp
    src/struct_probe.cpp
    src/hud_basis.cpp
    src/window_centering.cpp
)

add_library(ThiefHeadTracking SHARED
    ${MOD_SOURCES}
)

# Opt-in native unit tests. Declared here rather than beside the test targets because the
# mod's own output directory depends on it.
option(THIEF_BUILD_TESTS "Build ThiefHeadTracking unit tests" OFF)

# bin/ is written by the SHIPPING tree only. The test tree configures this same target, and
# both scripts/deploy.ps1 and scripts/package-release.ps1 read bin/<config> - so a test run
# at -Configuration Release would otherwise overwrite the exact file the packager ships
# with one built out of a tree configured for tests.
if(THIEF_BUILD_TESTS)
    set(MOD_OUTPUT_DIR "${CMAKE_BINARY_DIR}/mod")
else()
    set(MOD_OUTPUT_DIR "${CMAKE_SOURCE_DIR}/bin")
endif()

set_target_properties(ThiefHeadTracking PROPERTIES
    OUTPUT_NAME "ThiefHeadTracking"
    SUFFIX ".asi"
    RUNTIME_OUTPUT_DIRECTORY "${MOD_OUTPUT_DIR}"
    LIBRARY_OUTPUT_DIRECTORY "${MOD_OUTPUT_DIR}"
)

target_include_directories(ThiefHeadTracking PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# minhook carries its own include directory, so MinHook.h resolves through the link.
target_link_libraries(ThiefHeadTracking PRIVATE cameraunlock minhook)

if(MSVC)
    target_compile_options(ThiefHeadTracking PRIVATE /W4 /WX /permissive- /wd4324)
endif()

if(WIN32)
    target_link_libraries(ThiefHeadTracking PRIVATE ws2_32)
endif()

# Opt-in native unit tests for the pure logic the hooks inject through: the INI
# sanitizers, the UE3 rotator maths, the aim projection and the lean clamp geometry.
# Off by default so a normal `pixi run build` never builds a test binary into bin/.
if(THIEF_BUILD_TESTS)
    enable_testing()

    # One shape for every suite: the test source, whatever mod sources it needs compiled
    # in, the src/ include path, the core library, and the same warning level the mod is
    # built at. Keeping it in one place is what stops a new suite quietly building with
    # weaker warnings than the code it covers.
    function(thief_add_test name)
        add_executable(${name} tests/${name}.cpp ${ARGN})
        target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
        target_link_libraries(${name} PRIVATE cameraunlock)
        if(MSVC)
            target_compile_options(${name} PRIVATE /W4 /WX /permissive-)
        endif()
        add_test(NAME ${name} COMMAND ${name})
    endfunction()

    # src/config.cpp is compiled in, not just its headers: the sanitizers were already
    # covered but the CALL SITES were not, so a test drives an INI through
    # Config::LoadOrCreate and asserts what comes out. The rest of what this suite covers
    # - the UE3 rotator maths, the aim projection, the zoom factor and the lean trace
    # geometry - is header-only and needs no game.
    thief_add_test(thief_tests src/config.cpp src/build_profile.cpp)

    # The lean easing while the bow is drawn. src/ads.h is header-only, so it needs no game
    # or build profile.
    thief_add_test(ads_lean_tests)
    thief_add_test(reticle_projection_tests)
    thief_add_test(hud_accessor_tests)
    thief_add_test(game_state_tests src/game_state.cpp src/camera_diagnostics.cpp
                   src/aim_marker.cpp src/struct_probe.cpp)
endif()
