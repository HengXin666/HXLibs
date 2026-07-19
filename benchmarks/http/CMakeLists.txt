cmake_minimum_required(VERSION 3.20)
project(HXLibsHttpBenchmark LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
find_package(Threads REQUIRED)

set(HXLIBS_ENABLE_SSL OFF CACHE BOOL "" FORCE)
set(HXLIBS_ENABLE_DB OFF CACHE BOOL "" FORCE)
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../.." hxlibs)

add_executable(bench-hxlibs servers/hxlibs_server.cpp)
target_link_libraries(bench-hxlibs PRIVATE HXLibs)

if(NOT ASIO_SOURCE_DIR)
    message(FATAL_ERROR "ASIO_SOURCE_DIR is required")
endif()
add_executable(bench-asio servers/asio_server.cpp)
target_include_directories(bench-asio PRIVATE "${ASIO_SOURCE_DIR}/asio/include")
target_link_libraries(bench-asio PRIVATE Threads::Threads)

if(NOT YLT_SOURCE_DIR)
    message(FATAL_ERROR "YLT_SOURCE_DIR is required")
endif()
add_executable(bench-yalantinglibs servers/yalantinglibs_server.cpp)
target_include_directories(bench-yalantinglibs PRIVATE
    "${YLT_SOURCE_DIR}/include"
    "${YLT_SOURCE_DIR}/include/ylt/thirdparty"
    "${YLT_SOURCE_DIR}/include/ylt/standalone"
    "${YLT_SOURCE_DIR}/src/include"
)
target_link_libraries(bench-yalantinglibs PRIVATE Threads::Threads)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(bench-yalantinglibs PRIVATE -fcoroutines -fno-tree-slp-vectorize)
endif()

foreach(target bench-hxlibs bench-asio bench-yalantinglibs)
    set_target_properties(${target} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )
endforeach()
