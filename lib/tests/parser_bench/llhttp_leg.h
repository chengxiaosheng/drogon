#pragma once
#include <cstddef>
#include <string>
namespace drogon_bench
{
// Drive a minimal llhttp harness (counting callbacks, no drogon objects) over
// `data` fed in `fragSize`-byte fragments. Measures the parsing-phase ceiling.
// Returns true iff all `expectMessages` messages parse without error.
bool llhttpParseRequest(const std::string &data,
                        size_t fragSize,
                        size_t expectMessages);
bool llhttpParseResponse(const std::string &data,
                         size_t fragSize,
                         size_t expectMessages);
}  // namespace drogon_bench