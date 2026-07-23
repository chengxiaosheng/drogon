// jsoncpp-compatible shim (yyjson backed). Reader API.
#pragma once
#include "value.h"
#include <istream>
#include <memory>
#include <string>

namespace Json
{
class CharReader
{
  public:
    virtual ~CharReader() = default;
    virtual bool parse(char const *beginDoc,
                       char const *endDoc,
                       Value *root,
                       JSONCPP_STRING *errs) = 0;
};

class JSON_API CharReaderBuilder
{
  public:
    CharReaderBuilder();
    ~CharReaderBuilder();

    Value &operator[](const JSONCPP_STRING &key);
    Value const &operator[](const JSONCPP_STRING &key) const;

    std::unique_ptr<CharReader> newCharReader() const;

    // jsoncpp exposes these; kept for compatibility.
    void getValidReaderKeys(std::vector<JSONCPP_STRING> *validKeys) const;
    bool validate(Json::Value *invalid) const;

    Value settings_;
};

// Parse a stream into a Value (jsoncpp-compatible operator>>).
JSON_API std::istream &operator>>(std::istream &sin, Value &root);
}  // namespace Json
