cmake_minimum_required(VERSION 3.20)
project(Asteroids)

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
FetchContent_MakeAvailable(luau)

# ── Executable ────────────────────────────────────────────────────────────────

add_executable(asteroids src/main.cpp)

target_include_directories(asteroids PRIVATE
    ${luau_SOURCE_DIR}/VM/include
    ${luau_SOURCE_DIR}/Compiler/include)

target_link_libraries(asteroids PRIVATE SDL3::SDL3 Luau.VM Luau.Compiler)
