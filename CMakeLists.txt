cmake_minimum_required(VERSION 3.5)
project(shape_game)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "-g")
find_package(SDL3 REQUIRED)
find_package(OpenGL REQUIRED)

set(EXECUTABLE_NAME ${PROJECT_NAME})
set(CMAKE_INSTALL_PREFIX .)

if (CMAKE_SYSTEM_NAME MATCHES Emscripten)
	set(CMAKE_EXECUTABLE_SUFFIX ".html" CACHE INTERNAL "")
    set(CMAKE_CXX_FLAGS "-sFULL_ES3")
endif()

add_executable(${EXECUTABLE_NAME}
    src/main.cpp
    src/geometry.cpp
)

target_link_libraries(${EXECUTABLE_NAME} PRIVATE SDL3::SDL3 ${OPENGL_LIBRARIES})
install(TARGETS ${EXECUTABLE_NAME} DESTINATION AppDir)
