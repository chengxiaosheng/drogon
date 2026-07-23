# HTTP parser microbenchmark (Phase 0).
#
# Compares the legacy hand-written parsers (driven through ParseCursor over a
# MsgBuffer accumulation buffer) against a minimal llhttp harness (counting
# callbacks, no drogon objects) across a shared corpus x fragmentation matrix.
#
# llhttp is provided by the root CMake's FetchContent_Declare (shared with the
# DROGON_USE_LLHTTP adapter), guarded by DROGON_BUILD_PARSER_BENCH. No vendored
# copy, no submodule, per the approved plan.
#
# IMPORTANT: the benchmark drives the private HttpRequestParser/HttpResponseParser
# headers directly. When drogon is built with DROGON_USE_LLHTTP, those headers
# carry llhttp state members behind #ifdef DROGON_USE_LLHTTP; the benchmark must
# be compiled with the SAME macro so its view of the class layout matches drogon's
# (otherwise ODR/layout mismatch corrupts the parser objects).

add_executable(parser_bench
    main.cc
    corpus.cc
    legacy_leg.cc
    llhttp_leg.cc)

target_include_directories(parser_bench PRIVATE ${PROJECT_SOURCE_DIR}/lib/src)

target_link_libraries(parser_bench PRIVATE ${PROJECT_NAME} llhttp::llhttp)

if (DROGON_USE_LLHTTP)
    target_compile_definitions(parser_bench PRIVATE DROGON_USE_LLHTTP)
    if (DROGON_LLHTTP_LENIENT)
        target_compile_definitions(parser_bench PRIVATE DROGON_LLHTTP_LENIENT)
    endif ()
endif ()

set_property(TARGET parser_bench PROPERTY CXX_STANDARD ${DROGON_CXX_STANDARD})
set_property(TARGET parser_bench PROPERTY CXX_STANDARD_REQUIRED ON)
set_property(TARGET parser_bench PROPERTY CXX_EXTENSIONS OFF)