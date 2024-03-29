cmake_minimum_required(VERSION 3.21)

project(njs LANGUAGES CXX)

# C++ Standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(SOURCES
    njs/vm/NjsVM.cpp
    njs/vm/NjsVM_setup.cpp
    njs/vm/Instructions.cpp
    njs/basic_types/GlobalObject.cpp
    njs/basic_types/JSFunction.cpp
    njs/basic_types/JSObject.cpp
    njs/basic_types/JSValue.cpp
    njs/main.cpp
    njs/gc/GCHeap.cpp
    njs/parser/ast.cpp
    njs/basic_types/JSArray.cpp
    njs/vm/NativeFunction.cpp
    njs/vm/BasicCtors.cpp
    njs/vm/JSRunLoop.cpp
    njs/basic_types/JSObjectPrototype.cpp
    njs/basic_types/JSArrayPrototype.cpp
    njs/basic_types/JSFunctionPrototype.cpp
)

# executable is named `njs`
add_executable(njs ${SOURCES})

target_compile_options(njs PRIVATE -Wno-deprecated-declarations -fsanitize=address)
target_link_options(njs PRIVATE -fsanitize=address)
target_include_directories(njs PRIVATE .)

# executable for debug print
add_executable(njs_dbgprint ${SOURCES})

target_compile_options(njs_dbgprint PRIVATE -Wno-deprecated-declarations)
target_include_directories(njs_dbgprint PRIVATE .)

target_compile_definitions(njs_dbgprint PRIVATE DBGPRINT)