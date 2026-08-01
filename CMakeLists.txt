cmake_minimum_required(VERSION 3.10)
project(Wagnostic C)

set(CMAKE_C_STANDARD 11)

# Find SDL2
find_package(SDL2 REQUIRED)

# Sources (shared wasm3 engine)
file(GLOB WASM3_SRC "emulators/wasm3/wasm3/source/*.c")

# ── wagnostic (SDL2 + OpenGL emulator) ──────────────────────────────────────
set(WAGNOSTIC_SOURCES
    emulators/wasm3/host.c
    ${WASM3_SRC}
)

add_executable(wagnostic ${WAGNOSTIC_SOURCES})

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

# ── gifnostic (headless WASM3 + GIF exporter, no SDL2/OpenGL) ───────────────
set(GIFNOSTIC_SOURCES
    emulators/gifnostic/main.c
    emulators/gifnostic/gifnostic.c
    emulators/gifnostic/gif_encoder.c
    ${WASM3_SRC}
)

add_executable(gifnostic ${GIFNOSTIC_SOURCES})

target_include_directories(gifnostic PRIVATE
    emulators/wasm3/wasm3/source
    emulators/gifnostic
)

if(NOT WIN32)
    target_link_libraries(gifnostic PRIVATE m)
endif()

# MSVC: enable C11 for static_assert with message string
if(MSVC)
    target_compile_options(gifnostic PRIVATE /std:c11)
endif()
