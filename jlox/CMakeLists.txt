cmake_minimum_required(VERSION 3.20)

project(JLox LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(jlox
    src/main.cpp

    src/ast_printer.cpp
    src/interpreter.cpp
    src/lox.cpp
    src/parser.cpp
    src/scanner.cpp
    src/token.cpp
    src/token_type.cpp
)

add_executable(generate_ast
    tools/generate_ast.cpp
)

target_compile_features(generate_ast PRIVATE cxx_std_20)

if(MSVC)
    target_compile_options(jlox PRIVATE /W4 /permissive-)
else()
    target_compile_options(jlox PRIVATE -Wall -Wextra -Wpedantic)
endif()
