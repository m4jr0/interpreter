cmake_minimum_required(VERSION 3.20)

project(clox LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_executable(clox
    src/chunk.c
    src/compiler.c
    src/debug.c
    src/main.c
    src/memory.c
    src/object.c
    src/scanner.c
    src/value.c
    src/vm.c
)

target_include_directories(clox PRIVATE src)

if(MSVC)
    target_compile_options(clox PRIVATE /W4)
else()
    target_compile_options(clox PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
endif()