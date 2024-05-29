cmake_minimum_required(VERSION 3.21)

project(libregexp LANGUAGES C)

set(SOURCES
    libregexp.c
    libunicode.c
    cutils.c
)

add_library(regexp STATIC ${SOURCES})

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(regexp PRIVATE -fsanitize=address)
    target_link_options(regexp PRIVATE -fsanitize=address)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(regexp PRIVATE -flto)
    target_link_options(regexp PRIVATE -flto)
endif()
