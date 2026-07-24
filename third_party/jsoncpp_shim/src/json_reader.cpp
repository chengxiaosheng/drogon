// jsoncpp-compatible shim (yyjson backed). Reader implementation.
// Parse keeps yyjson's immutable yyjson_doc directly (no second copy); the
// Value becomes immutable-backed and only COWs to mutable if later mutated.
#include "json_internal.h"
#include <json/reader.h>

#include <cstring>
#include <iterator>
#include <streambuf>

namespace Json
{
CharReaderBuilder::CharReaderBuilder()
{
    settings_["collectComments"] = false;
    settings_["allowComments"] = true;
    settings_["allowTrailingCommas"] = false;
    settings_["strictRoot"] = true;
    settings_["allowDroppedNullPlaceholders"] = false;
    settings_["allowNumericKeys"] = false;
    settings_["allowSingleQuotes"] = false;
    settings_["failIfExtra"] = false;
    settings_["rejectDupKeys"] = false;
    settings_["stackLimit"] = 1000;
}
CharReaderBuilder::~CharReaderBuilder() = default;

Value &CharReaderBuilder::operator[](const JSONCPP_STRING &key)
{
    return settings_[key];
}
Value const &CharReaderBuilder::operator[](const JSONCPP_STRING &key) const
{
    return settings_[key];
}

void CharReaderBuilder::getValidReaderKeys(std::vector<JSONCPP_STRING> *v) const
{
    if (v)
        *v = settings_.getMemberNames();
}
bool CharReaderBuilder::validate(Json::Value *) const
{
    return true;
}

class YyjsonCharReader : public CharReader
{
  public:
    yyjson_read_flag flags{YYJSON_READ_NOFLAG};
    bool allowComments{true};

    bool parse(char const *beginDoc,
               char const *endDoc,
               Value *root,
               JSONCPP_STRING *errs) override
    {
        if (!root)
            return false;
        size_t len = endDoc > beginDoc ? static_cast<size_t>(endDoc - beginDoc) : 0;
        yyjson_read_flag flg = flags;
        if (allowComments)
            flg = yyjson_read_flag(flg | YYJSON_READ_ALLOW_COMMENTS);
        yyjson_read_err err;
        yyjson_doc *idoc =
            yyjson_read_opts(const_cast<char *>(beginDoc), len, flg, nullptr, &err);
        if (!idoc)
        {
            if (errs)
                *errs = err.msg ? err.msg : "parse error";
            return false;
        }
        // Keep the immutable doc as the value's backing (no copy).
        root->owner_ = std::make_shared<Doc>(false);
        root->owner_->idoc = idoc;
        root->doc_ = root->owner_.get();
        root->node_ = yyjson_doc_get_root(idoc);
        if (errs)
            errs->clear();
        return true;
    }
};

CharReader * CharReaderBuilder::newCharReader() const
{
    auto r = new YyjsonCharReader();
    // operator[] returns a borrowed handle (no default-Value allocation);
    // "allowComments" is set by the constructor.
    r->allowComments = settings_["allowComments"].asBool();
    return r;
}

std::istream &operator>>(std::istream &sin, Value &root)
{
    std::string content((std::istreambuf_iterator<char>(sin)),
                        std::istreambuf_iterator<char>());
    JSONCPP_STRING err;
    CharReaderBuilder b;
    auto r = b.newCharReader();
    r->parse(content.data(), content.data() + content.size(), &root, &err);
    return sin;
}
}  // namespace Json
