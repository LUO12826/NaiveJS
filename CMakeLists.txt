cmake_minimum_required(VERSION 3.21)

project(njs LANGUAGES CXX)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(SOURCES
    njs/basic_types/JSObject.cpp
    njs/basic_types/JSValue.cpp
    njs/main.cpp
)

# 添加可执行文件
add_executable(njs ${SOURCES})

target_compile_options(njs PRIVATE -Wno-deprecated-declarations)
target_include_directories(njs PRIVATE .)
# target_include_directories(njs PRIVATE include/)