// Internal (private) header for the jsoncpp-compatible yyjson shim.
// NOT installed. Pulls in yyjson so the public json/*.h headers never do.
//
// Design principles:
//  - A Value is a thin handle over a node inside a shared Doc.
//  - A parsed Value keeps yyjson's *immutable* yyjson_doc (no second copy on
//    parse). Reads work on both immutable (yyjson_val) and mutable
//    (yyjson_mut_val) nodes because yyjson guarantees identical layout, so we
//    cast to yyjson_val* and use the immutable read API for both.
//  - Mutation (operator[]=, append, ...) lazily COWs an immutable Doc into a
//    mutable one (yyjson_val_mut_copy of the subtree).
//  - operator[] returns references to borrowed handles cached on the Doc
//    (no mutex: JSON access is not expected to be concurrent).
//  - Borrowed handles are non-owning (raw Doc*), so there is no reference
//    cycle: a Doc is kept alive solely by owning Value instances.
#pragma once
#include <json/value.h>
#include <yyjson.h>

#include <list>
#include <string>
#include <unordered_map>

namespace Json
{
struct Doc
{
    bool isMut;
    yyjson_doc *idoc{nullptr};      // valid when !isMut
    yyjson_mut_doc *mdoc{nullptr};  // valid when isMut
    std::list<Value> cache;         // stable borrowed handles for operator[]
    std::unordered_map<void *, std::list<Value>::iterator> cacheIndex;
    std::list<std::string> strings; // anchors for in-place string assignment

    explicit Doc(bool mut) : isMut(mut)
    {
        if (mut)
            mdoc = yyjson_mut_doc_new(nullptr);
    }
    ~Doc()
    {
        if (isMut)
        {
            if (mdoc)
                yyjson_mut_doc_free(mdoc);
        }
        else
        {
            if (idoc)
                yyjson_doc_free(idoc);
        }
    }
    Doc(const Doc &) = delete;
    Doc &operator=(const Doc &) = delete;

    const char *anchor(std::string s)
    {
        strings.push_back(std::move(s));
        return strings.back().c_str();
    }
};

// Read-side accessors. node_ is yyjson_val* (immutable) or yyjson_mut_val*
// (mutable); both share yyjson's layout, so cast to yyjson_val* for reads.
inline yyjson_val *asNode(const Value &v)
{
    return static_cast<yyjson_val *>(v.rawNode());
}
inline Doc *asDoc(const Value &v)
{
    return v.rawDoc();
}
}  // namespace Json
