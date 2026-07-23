// jsoncpp-compatible shim (yyjson backed). Writer API.
#pragma once
#include "value.h"
#include <cstdint>
#include <ostream>
#include <string>

namespace Json
{
class StreamWriter
{
  public:
    virtual ~StreamWriter() = default;
    virtual int write(Value const &root, std::ostream *sout) = 0;
};

class JSON_API StreamWriterBuilder
{
  public:
    StreamWriterBuilder();
    ~StreamWriterBuilder();

    Value &operator[](const JSONCPP_STRING &key);
    Value const &operator[](const JSONCPP_STRING &key) const;

    std::unique_ptr<StreamWriter> newStreamWriter() const;

    void getValidWriterKeys(std::vector<JSONCPP_STRING> *validKeys) const;
    bool validate(Json::Value *invalid) const;

    Value settings_;

  private:
    friend JSONCPP_STRING writeString(StreamWriterBuilder const &, Value const &);
    // Cached yyjson write flags (uint32_t to keep yyjson out of this header).
    std::uint32_t flags() const;
    mutable std::uint32_t cachedFlags_{0};
    mutable bool flagsCached_{false};
};

JSON_API JSONCPP_STRING writeString(StreamWriterBuilder const &builder,
                                    Value const &root);

// Minimal FastWriter/StyledWriter compatibility (used by drogon_ctl).
class JSON_API FastWriter
{
  public:
    FastWriter() = default;
    JSONCPP_STRING write(Value const &root);

  private:
    bool yamlCompatibility_{false};
};

class JSON_API StyledWriter
{
  public:
    StyledWriter() = default;
    JSONCPP_STRING write(Value const &root);
};
}  // namespace Json
