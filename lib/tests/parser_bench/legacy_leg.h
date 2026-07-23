#pragma once
#include <cstddef>
#include <string>
namespace drogon_bench
{
// Drive the real legacy parser over `data` fed in `fragSize`-byte fragments.
// Returns true iff all `expectMessages` messages parse without error.
bool legacyParseRequest(const std::string &data,
                        size_t fragSize,
                        size_t expectMessages);
bool legacyParseResponse(const std::string &data,
                         size_t fragSize,
                         size_t expectMessages);
}  // namespace drogon_bench