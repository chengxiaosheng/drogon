# Find jsoncpp
#
# Find the jsoncpp includes and library
#
# if you nee to add a custom library search path, do it via via
# CMAKE_PREFIX_PATH
#
# This module defines JSONCPP_INCLUDE_DIRS, where to find header, etc.
# JSONCPP_LIBRARIES, the libraries needed to use jsoncpp. JSONCPP_FOUND, If
# false, do not try to use jsoncpp. 
# Jsoncpp_lib - The imported target library.

# On Apple Silicon Macs, Homebrew installs to /opt/homebrew instead of
# /usr/local (Intel Macs). Detect the prefix dynamically so cmake finds
# dependencies regardless of Mac architecture.
if(APPLE)
    execute_process(
        COMMAND brew --prefix jsoncpp
        OUTPUT_VARIABLE HOMEBREW_JSONCPP_PREFIX
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    if(HOMEBREW_JSONCPP_PREFIX)
        list(APPEND CMAKE_PREFIX_PATH ${HOMEBREW_JSONCPP_PREFIX})
    endif()
endif()


# only look in default directories
find_path(JSONCPP_INCLUDE_DIRS
          NAMES json/json.h
          DOC "jsoncpp include dir"
          PATH_SUFFIXES jsoncpp)

find_library(JSONCPP_LIBRARIES NAMES jsoncpp DOC "jsoncpp library")

# If a system jsoncpp could not be located, fall back to fetching the sources
# from upstream (https://github.com/open-source-parsers/jsoncpp, tag 1.9.8) and
# building the static library in-tree so the build stays self-contained on hosts
# without jsoncpp installed.
if(NOT JSONCPP_INCLUDE_DIRS OR NOT JSONCPP_LIBRARIES)
    if(NOT Jsoncpp_FIND_QUIETLY)
        message(STATUS "jsoncpp not found locally; fetching jsoncpp 1.9.8 from GitHub")
    endif()
    if(CMAKE_VERSION VERSION_LESS 3.11)
        message(FATAL_ERROR
            "jsoncpp was not found on the system and CMake ${CMAKE_VERSION} is too "
            "old to fetch it (requires >= 3.11). Install jsoncpp >= 1.7 or upgrade CMake.")
    endif()
    include(FetchContent)
    FetchContent_Declare(
        jsoncpp_src
        GIT_REPOSITORY https://github.com/open-source-parsers/jsoncpp.git
        GIT_TAG 1.9.8)
    # Keep the fetched jsoncpp lean: no tests, no packaging and no install rules
    # that could clash with drogon's own install step. Build the static target
    # (jsoncpp_static) that Jsoncpp_lib links against below.
    set(JSONCPP_WITH_TESTS OFF CACHE BOOL "" FORCE)
    set(JSONCPP_WITH_POST_BUILD_UNITTEST OFF CACHE BOOL "" FORCE)
    set(JSONCPP_WITH_PKGCONFIG_SUPPORT OFF CACHE BOOL "" FORCE)
    set(JSONCPP_WITH_CMAKE_PACKAGE OFF CACHE BOOL "" FORCE)
    set(JSONCPP_WITH_EXAMPLE OFF CACHE BOOL "" FORCE)
    set(JSONCPP_WITH_INSTALL OFF CACHE BOOL "" FORCE)
    set(BUILD_STATIC_LIBS ON CACHE BOOL "" FORCE)
    set(BUILD_OBJECT_LIBS OFF CACHE BOOL "" FORCE)
    set(CMAKE_CXX_STANDARD ${DROGON_CXX_STANDARD})
    if(CMAKE_VERSION VERSION_LESS 3.14)
        FetchContent_GetProperties(jsoncpp_src)
        if(NOT jsoncpp_src_POPULATED)
            FetchContent_Populate(jsoncpp_src)
        endif()
        add_subdirectory(${jsoncpp_src_SOURCE_DIR} ${jsoncpp_src_BINARY_DIR}
                         EXCLUDE_FROM_ALL)
    else()
        FetchContent_MakeAvailable(jsoncpp_src)
    endif()
    FetchContent_GetProperties(jsoncpp_src)
    set(JSONCPP_INCLUDE_DIRS "${jsoncpp_src_SOURCE_DIR}/include")
    set(JSONCPP_LIBRARIES jsoncpp_static)
    set(JSONCPP_FETCHED TRUE)
endif()

# debug library on windows same naming convention as in qt (appending debug
# library with d) boost is using the same "hack" as us with "optimized" and
# "debug" if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
# find_library(JSONCPP_LIBRARIES_DEBUG NAMES jsoncppd DOC "jsoncpp debug
# library") if("${JSONCPP_LIBRARIES_DEBUG}" STREQUAL "JSONCPP_LIBRARIES_DEBUG-
# NOTFOUND") set(JSONCPP_LIBRARIES_DEBUG ${JSONCPP_LIBRARIES}) endif()

# set(JSONCPP_LIBRARIES optimized ${JSONCPP_LIBRARIES} debug
# ${JSONCPP_LIBRARIES_DEBUG})

# endif()

# handle the QUIETLY and REQUIRED arguments and set JSONCPP_FOUND to TRUE if all
# listed variables are TRUE, hide their existence from configuration view
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Jsoncpp
                                  DEFAULT_MSG
                                  JSONCPP_INCLUDE_DIRS
                                  JSONCPP_LIBRARIES)
mark_as_advanced(JSONCPP_INCLUDE_DIRS JSONCPP_LIBRARIES)

if(Jsoncpp_FOUND)
  if(NOT EXISTS ${JSONCPP_INCLUDE_DIRS}/json/version.h)
    message(FATAL_ERROR "Error: jsoncpp lib is too old.....stop")
  endif()
  if(NOT WIN32)
    execute_process(
      COMMAND cat ${JSONCPP_INCLUDE_DIRS}/json/version.h
      COMMAND grep JSONCPP_VERSION_STRING
      COMMAND sed -e "s/.*define/define/"
      COMMAND awk "{ printf \$3 }"
      COMMAND sed -e "s/\"//g"
      OUTPUT_VARIABLE jsoncpp_ver)
    if(NOT Jsoncpp_FIND_QUIETLY)
      message(STATUS "jsoncpp version:" ${jsoncpp_ver})
    endif()
    if(jsoncpp_ver LESS 1.7)
      message(
        FATAL_ERROR
          "jsoncpp lib is too old, please get new version from https://github.com/open-source-parsers/jsoncpp"
        )
    endif(jsoncpp_ver LESS 1.7)
  endif()
  if (NOT TARGET Jsoncpp_lib)
          add_library(Jsoncpp_lib INTERFACE IMPORTED)
  endif()
  if(JSONCPP_FETCHED)
          # Link the in-tree fetched jsoncpp_static target directly so its
          # build interface (include dirs, compile features) propagates.
          set_target_properties(Jsoncpp_lib
                                PROPERTIES INTERFACE_INCLUDE_DIRECTORIES
                                           "${JSONCPP_INCLUDE_DIRS}")
          target_link_libraries(Jsoncpp_lib INTERFACE ${JSONCPP_LIBRARIES})
  else()
          set_target_properties(Jsoncpp_lib
                                PROPERTIES INTERFACE_INCLUDE_DIRECTORIES
                                           "${JSONCPP_INCLUDE_DIRS}"
                                           INTERFACE_LINK_LIBRARIES
                                           "${JSONCPP_LIBRARIES}")
  endif()

endif(Jsoncpp_FOUND)
