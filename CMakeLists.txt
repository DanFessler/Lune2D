cmake_minimum_required(VERSION 3.20)
project(lune2d)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── SDL3 ──────────────────────────────────────────────────────────────────────

find_package(SDL3 REQUIRED)

# ── Luau (built from source via FetchContent) ─────────────────────────────────

include(FetchContent)

set(LUAU_BUILD_CLI       OFF CACHE BOOL "" FORCE)
set(LUAU_BUILD_TESTS     OFF CACHE BOOL "" FORCE)
set(LUAU_BUILD_ANALYSIS  OFF CACHE BOOL "" FORCE)
set(LUAU_BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    luau
    GIT_REPOSITORY https://github.com/luau-lang/luau.git
    GIT_TAG        0.714
    GIT_SHALLOW    TRUE
)

FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
    GIT_SHALLOW    TRUE
)

# Single-header PNG/JPEG decoders (stb_image.h only).
FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG        master
    GIT_SHALLOW    TRUE
)

FetchContent_MakeAvailable(luau nlohmann_json stb)

# ── Executable ────────────────────────────────────────────────────────────────

add_executable(lune2d
    src/main.cpp
    src/scene.cpp
    src/editor_state.cpp
    src/editor_pick.cpp
    src/editor_bridge.cpp
    src/webview_viewport.cpp
    src/viewport_math.cpp
    src/engine/camera.cpp
    src/engine/camera_view_matrix.cpp
    src/engine/engine.cpp
    src/engine/audio_system.cpp
    src/engine/immediate_draw.cpp
    src/engine/texture_cache.cpp
    src/engine/platform_paths.cpp
    src/engine/lua/lua_draw.cpp
    src/engine/game_keyboard.cpp
    src/engine/lua/lua_input.cpp
    src/engine/lua/lua_audio.cpp
    src/engine/lua/lua_app.cpp
    src/engine/lua/lua_transform.cpp
    src/engine/lua/lua_runtime.cpp
    src/engine/lua/behavior_schema.cpp
    src/engine/lua/behavior_instance.cpp
    src/engine/lua/lua_vm_lifecycle.cpp
    src/engine/lua/lua_require.cpp
    src/engine/lua/lua_api_register.cpp
    src/engine/lua/lua_editor.cpp
    src/engine/lua/lua_editor_input.cpp
    src/engine/lua/script_host.cpp
    src/engine/scene_loader.cpp
)

enable_testing()
add_executable(viewport_math_tests tests/viewport_math_test.cpp src/viewport_math.cpp)
target_include_directories(viewport_math_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
add_test(NAME viewport_math COMMAND viewport_math_tests)

add_executable(behavior_model_tests
    tests/behavior_model_test.cpp
    src/scene.cpp
    src/engine/scene_loader.cpp
    src/engine/lua/behavior_schema.cpp)
target_include_directories(behavior_model_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/lua
    ${luau_SOURCE_DIR}/VM/include)
target_link_libraries(behavior_model_tests PRIVATE
    SDL3::SDL3
    Luau.VM
    nlohmann_json::nlohmann_json)
add_test(NAME behavior_model COMMAND behavior_model_tests)

add_executable(editor_input_tests tests/editor_input_test.cpp
    src/editor_state.cpp
    src/engine/lua/lua_editor_input.cpp)
target_include_directories(editor_input_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/lua)
add_test(NAME editor_input COMMAND editor_input_tests)

add_executable(affine2d_tests tests/affine2d_test.cpp)
target_include_directories(affine2d_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
add_test(NAME affine2d COMMAND affine2d_tests)

add_executable(camera_math_tests tests/camera_math_test.cpp src/engine/camera_view_matrix.cpp)
target_include_directories(camera_math_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
add_test(NAME camera_math COMMAND camera_math_tests)

add_executable(camera_sync_tests
    tests/camera_sync_test.cpp
    src/engine/camera.cpp
    src/engine/camera_view_matrix.cpp
    src/engine/engine.cpp
    src/editor_state.cpp
    src/scene.cpp
    src/engine/lua/behavior_schema.cpp)
target_include_directories(camera_sync_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/lua
    ${luau_SOURCE_DIR}/VM/include)
target_link_libraries(camera_sync_tests PRIVATE
    SDL3::SDL3
    Luau.VM
    nlohmann_json::nlohmann_json)
add_test(NAME camera_sync COMMAND camera_sync_tests)

add_test(NAME visual_camera COMMAND $<TARGET_FILE:lune2d>
    --project ${CMAKE_SOURCE_DIR}/test-project
    --run-visual-tests
    --visual-test-after-frames 30)
set_tests_properties(visual_camera PROPERTIES LABELS "visual;sdl")

if(APPLE)
    enable_language(OBJCXX)
    set(CMAKE_OBJCXX_STANDARD 17)
    set(CMAKE_OBJCXX_STANDARD_REQUIRED ON)
    target_sources(lune2d PRIVATE src/webview_host_mac.mm)
    target_link_libraries(lune2d PRIVATE "-framework WebKit" "-framework Cocoa"
        "-framework CoreGraphics" "-framework UniformTypeIdentifiers")
else()
    target_sources(lune2d PRIVATE src/webview_host_stub.cpp)
endif()

target_include_directories(lune2d PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/lua
    ${luau_SOURCE_DIR}/VM/include
    ${luau_SOURCE_DIR}/Compiler/include
    ${stb_SOURCE_DIR})

target_link_libraries(lune2d PRIVATE SDL3::SDL3 Luau.VM Luau.Compiler nlohmann_json::nlohmann_json)
