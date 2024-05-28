cmake_minimum_required(VERSION 3.21)

project(njs LANGUAGES CXX)

# C++ Standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(SOURCES
    njs/vm/NjsVM.cpp
    njs/vm/NjsVM_setup.cpp
    njs/vm/Instruction.cpp
    njs/basic_types/JSFunction.cpp
    njs/basic_types/JSObject.cpp
    njs/basic_types/JSValue.cpp
    njs/main.cpp
    njs/gc/GCHeap.cpp
    njs/parser/ast.cpp
    njs/basic_types/JSArray.cpp
    njs/vm/NativeFunction.cpp
    njs/vm/basic_constructor.cpp
    njs/vm/JSRunLoop.cpp
    njs/basic_types/JSObjectPrototype.cpp
    njs/basic_types/JSArrayPrototype.cpp
    njs/basic_types/JSFunctionPrototype.cpp
    njs/basic_types/JSErrorPrototype.cpp
    njs/basic_types/conversion.cpp
)

# executable is named `njs`
add_executable(njs ${SOURCES})
target_include_directories(njs PRIVATE .)
target_compile_options(njs PRIVATE -Wno-deprecated-declarations)

# Conditionally add sanitizer options if not in Release mode
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(njs PRIVATE -fsanitize=address)
    target_link_options(njs PRIVATE -fsanitize=address)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(njs PRIVATE -flto)
    target_link_options(njs PRIVATE -flto)
endif()

if(DEBUG)
    target_compile_definitions(njs PRIVATE DEBUG)
endif()

if(DBG_SCOPE)
    target_compile_definitions(njs PRIVATE DBG_SCOPE)
endif()

# executable for debug print
add_executable(njs_dbgprint ${SOURCES})
target_include_directories(njs_dbgprint PRIVATE .)
target_compile_options(njs_dbgprint PRIVATE -Wno-deprecated-declarations)
target_compile_definitions(njs_dbgprint PRIVATE DBGPRINT)
