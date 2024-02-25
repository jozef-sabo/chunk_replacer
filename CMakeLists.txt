cmake_minimum_required(VERSION 3.0)

# Project configuration
project(chunk_replacer)
set(SOURCES "main.c")
set(EXECUTABLE chunk_replacer)


# Executable
add_executable(${EXECUTABLE} ${SOURCES})

# Configure compiler warnings
if (CMAKE_C_COMPILER_ID MATCHES Clang OR ${CMAKE_C_COMPILER_ID} STREQUAL GNU)
    # using regular Clang, AppleClang or GCC
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99 -Wall -Wextra -pedantic -g -D_POSIX_C_SOURCE=200809L")
elseif (${CMAKE_C_COMPILER_ID} STREQUAL MSVC)
    # using Visual Studio C++
    target_compile_definitions(${EXECUTABLE} PRIVATE _CRT_SECURE_NO_DEPRECATE)
    target_compile_definitions(${EXECUTABLE_TESTS} PRIVATE _CRT_SECURE_NO_DEPRECATE)
    set(CMAKE_CXX_FLAGS "/permissive- /W4 /EHsc")
endif()