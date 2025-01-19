cmake_minimum_required(VERSION 3.13)
project(shape_game VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "-Wall -Wextra -Wpedantic -Wconversion")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON) # for nvim LSP support

set(EXECUTABLE_NAME ${PROJECT_NAME})

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

file(CREATE_LINK "${PROJECT_SOURCE_DIR}/assets" "${CMAKE_BINARY_DIR}/assets" SYMBOLIC)

if (EMSCRIPTEN)
    set(CMAKE_FIND_ROOT_PATH /wasm)
	set(CMAKE_EXECUTABLE_SUFFIX ".html" CACHE INTERNAL "")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Os") # optimize for size

    target_link_directories(${EXECUTABLE_NAME} PRIVATE /wasm/lib)
    target_link_options(${EXECUTABLE_NAME} PRIVATE -sFULL_ES3 -sALLOW_MEMORY_GROWTH --embed-file assets)

    install(DIRECTORY /wasm/share/licenses DESTINATION .)
    install(FILES 
        README.md 
        LICENSE 
        index.html 
        ${CMAKE_BINARY_DIR}/${EXECUTABLE_NAME}.wasm
        ${CMAKE_BINARY_DIR}/${EXECUTABLE_NAME}.js
        DESTINATION .)

    set(CPACK_GENERATOR "ZIP")
    include(CPack)
elseif(WIN32)
    set(CPACK_SYSTEM_NAME Windows)
    target_link_directories(${EXECUTABLE_NAME} PRIVATE /win32/lib)
    target_link_libraries(${EXECUTABLE_NAME} PRIVATE GLESv2)
    target_link_options(${EXECUTABLE_NAME} PRIVATE -static-libgcc -static-libstdc++)

    install(TARGETS ${EXECUTABLE_NAME} RUNTIME DESTINATION .)
    install(DIRECTORY assets DESTINATION .)
    install(DIRECTORY /win32/share/licenses DESTINATION .)
    install(FILES 
        README.md 
        LICENSE 
        /win32/bin/SDL3.dll
        /win32/bin/libGLESv2.dll
        /win32/bin/libEGL.dll
        /usr/x86_64-w64-mingw32/lib/zlib1.dll
        /usr/x86_64-w64-mingw32/lib/libwinpthread-1.dll
        /msys2/mingw64/bin/libstdc++-6.dll # libGLESv2.dll needs this specific version
        /msys2/mingw64/bin/libgcc_s_seh-1.dll # libGLESv2.dll 
        DESTINATION .)

    set(CPACK_GENERATOR "ZIP")
    include(CPack)
else()
    # Linux
    install(TARGETS ${EXECUTABLE_NAME} RUNTIME DESTINATION .)
    install(DIRECTORY assets DESTINATION .)
    install(DIRECTORY /usr/local/share/licenses DESTINATION .)
    install(FILES 
        README.md 
        LICENSE 
        /usr/local/lib/libSDL3.so
        /usr/local/lib/libSDL3.so.0
        /usr/local/lib/libSDL3.so.0.1.7
        DESTINATION .)

    set(CPACK_GENERATOR "TGZ")
    include(CPack)
endif()

set(OpenGL_GL_PREFERENCE LEGACY)
find_package(SDL3 REQUIRED)
find_package(OpenGL REQUIRED)

target_link_libraries(${EXECUTABLE_NAME} PRIVATE SDL3::SDL3 ${OPENGL_LIBRARIES})
