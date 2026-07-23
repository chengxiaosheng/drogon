# Temporary standalone build for the jsoncpp-shim (compile/test only).
cmake_minimum_required(VERSION 3.14)
project(jsoncpp_shim_test CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(yyjson URL https://github.com/ibireme/yyjson/archive/refs/tags/0.10.0.tar.gz)
set(YYJSON_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(YYJSON_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(yyjson)
set_target_properties(yyjson PROPERTIES EXCLUDE_FROM_ALL TRUE)

add_library(jsoncpp_shim STATIC
    src/json_value.cpp
    src/json_reader.cpp
    src/json_writer.cpp)
target_include_directories(jsoncpp_shim PUBLIC include PRIVATE src ${yyjson_SOURCE_DIR}/src)
target_sources(jsoncpp_shim PRIVATE ${yyjson_SOURCE_DIR}/src/yyjson.c)
set_source_files_properties(${yyjson_SOURCE_DIR}/src/yyjson.c PROPERTIES LANGUAGE C)
target_compile_definitions(jsoncpp_shim PUBLIC DROGON_STATIC_DEFINE)

add_executable(shim_test tests/shim_test.cpp)
target_link_libraries(shim_test PRIVATE jsoncpp_shim)
