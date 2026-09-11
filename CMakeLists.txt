cmake_minimum_required(VERSION 3.10)
project(Wagnostic C)

set(CMAKE_C_STANDARD 11)

# Sources (shared wasm3 engine)
file(GLOB WASM3_SRC "runners/native/wasm3/source/*.c")

# ── wagnostic (100% Libc/POSIX Terminal + GIF Native Runner) ──────────────────
set(WAGNOSTIC_SOURCES
    runners/native/host.c
    runners/native/platform.c
    runners/native/ipc.c
    runners/native/worker.c
    runners/native/gif_encoder.c
    ${WASM3_SRC}
)

add_executable(wagnostic ${WAGNOSTIC_SOURCES})

target_include_directories(wagnostic PRIVATE
    include
    runners/native
    runners/native/wasm3/source
)

if(NOT WIN32)
    target_link_libraries(wagnostic PRIVATE m)
endif()

if(MSVC)
    target_compile_options(wagnostic PRIVATE /std:c11)
endif()
