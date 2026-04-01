cmake_minimum_required(VERSION 3.20)
project(Asteroids)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SDL3 REQUIRED)

add_executable(asteroids src/main.cpp)

target_link_libraries(asteroids PRIVATE SDL3::SDL3)
