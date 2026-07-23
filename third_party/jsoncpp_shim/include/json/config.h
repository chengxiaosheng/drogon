// jsoncpp-compatible shim backed by yyjson.
// Provides the Json:: surface used by drogon while using yyjson as the
// parser/serializer engine. Intentionally a subset of jsoncpp.
#pragma once

#include <cstdint>
#include <string>

// JSON_API mirrors drogon's own DROGON_EXPORT (from the generated exports.h)
// so the Json:: symbols share drogon's export/import/static linkage. The
// drogon build defines DROGON_JSONCPP_SHIM; the standalone test does not.
#ifdef DROGON_JSONCPP_SHIM
#  include <drogon/exports.h>
#  define JSON_API DROGON_EXPORT
#else
#  define JSON_API
#endif

namespace Json
{
using Int = int;
using UInt = unsigned int;
using Int64 = std::int64_t;
using UInt64 = std::uint64_t;
using LargestInt = Int64;
using LargestUInt = UInt64;

using String = std::string;
}  // namespace Json

// jsoncpp exposes JSONCPP_STRING at GLOBAL scope (drogon uses it unqualified),
// so it must live outside namespace Json.
using JSONCPP_STRING = std::string;
