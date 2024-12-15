cmake_minimum_required(VERSION 3.13)
project(shape_game)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "-g")
set(CMAKE_CXX_FLAGS "-Wall")

set(EXECUTABLE_NAME ${PROJECT_NAME})
set(OpenGL_GL_PREFERENCE LEGACY)

find_package(SDL3 REQUIRED)
find_package(OpenGL REQUIRED)

add_executable(${EXECUTABLE_NAME}
    src/main.cpp
    src/geometry.cpp
    src/geometry.hpp
    src/stb_vorbis.cpp
    src/stb_vorbis.hpp
    src/audio.cpp
    src/audio.hpp
    src/font.cpp
    src/font.hpp
    src/gl_helper.cpp
    src/gl_helper.hpp
    src/log.hpp
)

target_link_libraries(${EXECUTABLE_NAME} PRIVATE SDL3::SDL3 ${OPENGL_LIBRARIES})

if (CMAKE_SYSTEM_NAME MATCHES Emscripten)
    file(CREATE_LINK "${PROJECT_SOURCE_DIR}/assets" "${CMAKE_BINARY_DIR}/assets" SYMBOLIC)
	set(CMAKE_EXECUTABLE_SUFFIX ".html" CACHE INTERNAL "")
    target_link_options(${EXECUTABLE_NAME} PRIVATE -sFULL_ES3 -sALLOW_MEMORY_GROWTH --embed-file assets)
endif()
