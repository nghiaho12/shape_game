cmake_minimum_required(VERSION 3.5)
project(shape_game)

set(CMAKE_CXX_STANDARD 20)
find_package(SDL3 REQUIRED)
find_package(OpenGL REQUIRED)

if (CMAKE_SYSTEM_NAME MATCHES Emscripten)
	set(CMAKE_EXECUTABLE_SUFFIX ".html" CACHE INTERNAL "")
    set(CMAKE_CXX_FLAGS "-sFULL_ES3")
endif()

add_executable(main 
    src/main.cpp
    src/geometry.cpp
)

target_link_libraries(main PRIVATE SDL3::SDL3 ${OPENGL_LIBRARIES})
