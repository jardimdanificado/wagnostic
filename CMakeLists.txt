cmake_minimum_required(VERSION 3.10)
project(Wagnostic C)

set(CMAKE_C_STANDARD 11)

# Find SDL2
find_package(SDL2 REQUIRED)

# Sources
file(GLOB WASM3_SRC "emulators/wasm3/wasm3/source/*.c")
set(SOURCES 
    emulators/wasm3/host.c
    ${WASM3_SRC}
)

add_executable(wagnostic ${SOURCES})

target_include_directories(wagnostic PRIVATE 
    emulators/wasm3/wasm3/source
    ${SDL2_INCLUDE_DIRS}
)

target_link_libraries(wagnostic PRIVATE 
    ${SDL2_LIBRARIES}
)

if(WIN32)
    find_package(GLEW REQUIRED)
    target_link_libraries(wagnostic PRIVATE GLEW::GLEW opengl32)
else()
    target_link_libraries(wagnostic PRIVATE GL m)
endif()
