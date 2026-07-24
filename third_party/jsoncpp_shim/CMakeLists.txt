# jsoncpp_shim -- a jsoncpp-compatible API backed by yyjson.
#
# This is an independent, self-contained library.  It has its own project,
# install rules, export set, and package config file.  It does *not* depend
# on any parent-project variables and can be built standalone or consumed via
# add_subdirectory() from any project (including drogon).

cmake_minimum_required(VERSION 3.14...3.31)

project(jsoncpp_shim
        VERSION 0.1.0
        DESCRIPTION "jsoncpp-compatible API backed by yyjson"
        LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(GenerateExportHeader)

# ---------------------------------------------------------------------------
# yyjson -- single-file C JSON library (private implementation detail)
# ---------------------------------------------------------------------------
# The shim's public headers forward-declare yyjson types as opaque pointers
# and never include <yyjson.h>.
if(NOT TARGET yyjson)
    include(FetchContent)
    set(YYJSON_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(YYJSON_INSTALL  OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        yyjson
        URL https://github.com/ibireme/yyjson/archive/refs/tags/0.12.0.tar.gz
        CMAKE_ARGS -DYYJSON_BUILD_TESTS=OFF -DYYJSON_INSTALL=OFF -DBUILD_SHARED_LIBS=OFF)
    FetchContent_MakeAvailable(yyjson)
endif()

# ---------------------------------------------------------------------------
# jsoncpp_shim library
# ---------------------------------------------------------------------------
add_library(jsoncpp_shim STATIC
    src/json_value.cpp
    src/json_reader.cpp
    src/json_writer.cpp)

# Alias that mirrors the jsoncpp "jsoncpp_static" target name.
add_library(jsoncpp_static ALIAS jsoncpp_shim)

set_target_properties(jsoncpp_shim PROPERTIES EXPORT_NAME jsoncpp_shim)
set_target_properties(yyjson      PROPERTIES EXPORT_NAME yyjson)

# Generate the shim's own export header (empty macros for STATIC builds).
generate_export_header(jsoncpp_shim
    BASE_NAME JSONCPP_SHIM
    EXPORT_FILE_NAME ${CMAKE_CURRENT_BINARY_DIR}/generated/jsoncpp_shim_export.h)

set(JSONCPP_SHIM_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/json/config.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/json/forwards.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/json/json.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/json/reader.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/json/value.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/json/writer.h)

target_include_directories(jsoncpp_shim
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/generated>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        $<BUILD_INTERFACE:${yyjson_SOURCE_DIR}/src>)

target_link_libraries(jsoncpp_shim PRIVATE yyjson)

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if(BUILD_TESTING)
    add_executable(shim_test tests/shim_test.cpp)
    target_link_libraries(shim_test PRIVATE jsoncpp_shim)
endif()

# ---------------------------------------------------------------------------
# Install
# ---------------------------------------------------------------------------
install(TARGETS jsoncpp_shim yyjson
    EXPORT jsoncpp_shim-targets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

install(FILES
    ${JSONCPP_SHIM_HEADERS}
    ${CMAKE_CURRENT_BINARY_DIR}/generated/jsoncpp_shim_export.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/json)

# ---------------------------------------------------------------------------
# Export / package config
# ---------------------------------------------------------------------------
install(EXPORT jsoncpp_shim-targets
    NAMESPACE jsoncpp_shim::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/jsoncpp_shim)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/jsoncpp_shim-config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/jsoncpp_shim-config.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/jsoncpp_shim)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/jsoncpp_shim-config-version.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/jsoncpp_shim-config.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/jsoncpp_shim-config-version.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/jsoncpp_shim)
