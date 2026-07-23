// jsoncpp-compatible shim (yyjson backed). Writer implementation.
// Serialization writes the Value's document directly (immutable or mutable);
// only a borrowed subtree handle needs a temporary copy.
#include "json_internal.h"
#include <json/writer.h>

#include <cstdlib>
#include <ostream>
#include <utility>

namespace Json
{
namespace
{
// Serialize a Value (which may be a subtree) to a string using yyjson.
std::string serialize(const Value &v, yyjson_write_flag flags)
{
    Doc *d = asDoc(v);
    yyjson_val *node = asNode(v);
    if (!node) return "null";
    char *buf = nullptr;
    size_t len = 0;
    yyjson_write_err err;
    if (d && !d->isMut && node == yyjson_doc_get_root(d->idoc))
        buf = yyjson_write_opts(d->idoc, flags, nullptr, &len, &err);
    else if (d && d->isMut && reinterpret_cast<yyjson_mut_val *>(node) == yyjson_mut_doc_get_root(d->mdoc))
        buf = yyjson_mut_write_opts(d->mdoc, flags, nullptr, &len, &err);
    else
    {
        yyjson_mut_doc *td = yyjson_mut_doc_new(nullptr);
        yyjson_mut_val *cp = (d && d->isMut) ? yyjson_mut_val_mut_copy(td, reinterpret_cast<yyjson_mut_val *>(node)) : yyjson_val_mut_copy(td, node);
        yyjson_mut_doc_set_root(td, cp);
        buf = yyjson_mut_write_opts(td, flags, nullptr, &len, &err);
        yyjson_mut_doc_free(td);
    }
    if (!buf) return std::string();
    std::string out(buf, len);
    free(buf);
    return out;
}

yyjson_write_flag flagsFromSettings(const Value &settings)
{
    yyjson_write_flag flg = YYJSON_WRITE_NOFLAG;
    // operator[] returns a borrowed handle (no default-Value allocation).
    if (!settings["indentation"].asStringView().empty())
        flg = yyjson_write_flag(flg | YYJSON_WRITE_PRETTY);
    if (!settings["emitUTF8"].asBool())
        flg = yyjson_write_flag(flg | YYJSON_WRITE_ESCAPE_UNICODE);
    return flg;
}
}  // namespace

JSONCPP_STRING Value::toStyledString() const
{
    return serialize(*this, YYJSON_WRITE_PRETTY) + "\n";
}

StreamWriterBuilder::StreamWriterBuilder()
    : cachedFlags_(YYJSON_WRITE_NOFLAG), flagsCached_(false)
{
    settings_["commentStyle"] = "All";
    settings_["indentation"] = "\t";
    settings_["enableYAMLCompatibility"] = false;
    settings_["dropNullPlaceholders"] = false;
    settings_["useSpecialFloats"] = false;
    settings_["precision"] = 17;
    settings_["precisionType"] = "significant";
}
StreamWriterBuilder::~StreamWriterBuilder() = default;

Value &StreamWriterBuilder::operator[](const JSONCPP_STRING &key)
{
    flagsCached_ = false;  // settings changed; recompute flags lazily
    return settings_[key];
}
Value const &StreamWriterBuilder::operator[](const JSONCPP_STRING &key) const
{
    return settings_[key];
}
void StreamWriterBuilder::getValidWriterKeys(std::vector<JSONCPP_STRING> *v) const
{
    if (v)
        *v = settings_.getMemberNames();
}
bool StreamWriterBuilder::validate(Json::Value *) const
{
    return true;
}

yyjson_write_flag StreamWriterBuilder::flags() const
{
    if (!flagsCached_)
    {
        cachedFlags_ = flagsFromSettings(settings_);
        flagsCached_ = true;
    }
    return cachedFlags_;
}

JSONCPP_STRING writeString(StreamWriterBuilder const &builder, Value const &root)
{
    return serialize(root, builder.flags());
}

namespace
{
class YyjsonStreamWriter : public StreamWriter
{
  public:
    yyjson_write_flag flags{YYJSON_WRITE_NOFLAG};
    int write(Value const &root, std::ostream *sout) override
    {
        std::string s = serialize(root, flags);
        if (sout)
            sout->write(s.data(), static_cast<std::streamsize>(s.size()));
        return static_cast<int>(s.size());
    }
};
}  // namespace

std::unique_ptr<StreamWriter> StreamWriterBuilder::newStreamWriter() const
{
    auto w = std::make_unique<YyjsonStreamWriter>();
    w->flags = flags();
    return w;
}

JSONCPP_STRING FastWriter::write(Value const &root)
{
    yyjson_write_flag flg = YYJSON_WRITE_NOFLAG;
    if (yamlCompatibility_)
        flg = yyjson_write_flag(flg | YYJSON_WRITE_ESCAPE_UNICODE);
    return serialize(root, flg) + "\n";
}

JSONCPP_STRING StyledWriter::write(Value const &root)
{
    return serialize(root, YYJSON_WRITE_PRETTY) + "\n";
}
}  // namespace Json
