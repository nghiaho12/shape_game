cmake_minimum_required(VERSION 3.5)
project(shape_game)

find_package(SDL3 REQUIRED CONFIG REQUIRED COMPONENTS SDL3-shared)
find_package(OpenGL REQUIRED)

add_executable(main main.cpp)

target_link_libraries(main PRIVATE SDL3::SDL3 GL)
