// jsoncpp-compatible shim (yyjson backed). Json::Value definition.
//
// A Value is ALWAYS a thin handle over a node in a shared yyjson document --
// all JSON data (including scalars) lives in yyjson, never in C++ fields.
// Parsed values keep yyjson's immutable doc (no second copy); mutation lazily
// COWs to a mutable doc. operator[] returns references to borrowed handles
// cached on the doc (no mutex: JSON access is not expected to be concurrent).
#pragma once
#include "config.h"
#include <memory>
#include <string>
#include <string_view>
#include <vector>

typedef struct yyjson_val yyjson_val;
typedef struct yyjson_doc yyjson_doc;
typedef struct yyjson_mut_val yyjson_mut_val;
typedef struct yyjson_mut_doc yyjson_mut_doc;

namespace Json
{
using ArrayIndex = unsigned int;

constexpr Int minInt = Int(~(UInt(-1) / 2));
constexpr Int maxInt = Int((UInt(-1) / 2));
constexpr UInt maxUInt = UInt(-1);
constexpr Int64 minInt64 = Int64(~(UInt64(-1) / 2));
constexpr Int64 maxInt64 = Int64((UInt64(-1) / 2));
constexpr UInt64 maxUInt64 = UInt64(-1);

enum ValueType
{
    nullValue = 0,
    intValue,
    uintValue,
    realValue,
    stringValue,
    booleanValue,
    arrayValue,
    objectValue
};

class JSON_API Exception : public std::exception
{
  public:
    explicit Exception(JSONCPP_STRING const &msg);
    ~Exception() noexcept override = default;
    char const *what() const noexcept override;

  protected:
    JSONCPP_STRING msg_;
};
class JSON_API RuntimeError : public Exception
{
  public:
    explicit RuntimeError(JSONCPP_STRING const &msg);
};
class JSON_API LogicError : public Exception
{
  public:
    explicit LogicError(JSONCPP_STRING const &msg);
};
[[noreturn]] JSON_API void throwRuntimeError(JSONCPP_STRING const &msg);
[[noreturn]] JSON_API void throwLogicError(JSONCPP_STRING const &msg);

struct Doc;
class YyjsonCharReader;

class Value
{
  public:
    using Members = std::vector<JSONCPP_STRING>;
    using iterator = class ValueIterator;
    using const_iterator = class ValueConstIterator;

    using Int = Json::Int;
    using UInt = Json::UInt;
    using Int64 = Json::Int64;
    using UInt64 = Json::UInt64;
    using LargestInt = Json::LargestInt;
    using LargestUInt = Json::LargestUInt;

    using String = std::string;

    Value();
    Value(Value const &other);
    Value(Value &&other) noexcept;
    explicit Value(ValueType type);
    Value(const char *value);
    Value(const char *begin, const char *end);
    Value(std::string const &value);
    Value(std::string &&value);
    Value(bool value);
    Value(Int value);
    Value(UInt value);
    Value(Int64 value);
    Value(UInt64 value);
    Value(double value);
    ~Value();

    Value &operator=(Value const &other);
    Value &operator=(Value &&other) noexcept;
    Value &operator=(const char *value);
    Value &operator=(bool value);
    Value &operator=(Int value);
    Value &operator=(UInt value);
    Value &operator=(Int64 value);
    Value &operator=(UInt64 value);
    Value &operator=(double value);
    Value &operator=(std::string const &value);

    void swap(Value &other);

    ValueType type() const;

    bool operator!() const;
    explicit operator bool() const;

    bool isNull() const;
    bool isBool() const;
    bool isInt() const;
    bool isInt64() const;
    bool isUInt() const;
    bool isUInt64() const;
    bool isIntegral() const;
    bool isDouble() const;
    bool isNumeric() const;
    bool isString() const;
    bool isArray() const;
    bool isObject() const;
    bool isConvertibleTo(ValueType other) const;

    ArrayIndex size() const;
    bool empty() const;
    void clear();
    void resize(ArrayIndex newSize);

    const char *asCString() const;
    JSONCPP_STRING asString() const;
    // Zero-copy view into yyjson string storage (invalidated on mutation).
    std::string_view asStringView() const;
    Int asInt() const;
    UInt asUInt() const;
    Int64 asInt64() const;
    UInt64 asUInt64() const;
    LargestInt asLargestInt() const;
    LargestUInt asLargestUInt() const;
    double asDouble() const;
    float asFloat() const;
    bool asBool() const;

    Value &operator[](const char *key);
    Value const &operator[](const char *key) const;
    Value &operator[](const JSONCPP_STRING &key);
    Value const &operator[](const JSONCPP_STRING &key) const;
    Value &operator[](ArrayIndex index);
    Value const &operator[](ArrayIndex index) const;
    Value &operator[](int index);
    Value const &operator[](int index) const;

    Value get(const char *key, Value const &defaultValue) const;
    Value get(const JSONCPP_STRING &key, Value const &defaultValue) const;
    Value get(ArrayIndex index, Value const &defaultValue) const;

    bool isMember(const char *key) const;
    bool isMember(const JSONCPP_STRING &key) const;

    Members getMemberNames() const;

    void removeMember(const char *key);
    void removeMember(const JSONCPP_STRING &key);

    Value &append(Value const &value);
    Value &append(Value &&value);

    iterator begin();
    iterator end();
    const_iterator begin() const;
    const_iterator end() const;

    JSONCPP_STRING toStyledString() const;

    bool operator==(Value const &other) const;
    bool operator!=(Value const &other) const;

  private:
    friend struct Doc;
    friend class ValueConstIterator;
    friend class ValueIterator;
    friend class YyjsonCharReader;
    friend yyjson_val *asNode(const Value &);
    friend Doc *asDoc(const Value &);

    Value(Doc *doc, void *node) : doc_(doc), node_(node) {}  // borrowed handle

    Value &cacheHandle(void *child) const;
    void initOwn();
    void makeOwnedCopyOf(Value const &other);
    void ensureMutable();  // COW immutable doc -> mutable on first mutation

    void *rawNode() const { return node_; }
    Doc *rawDoc() const { return doc_; }
    void setRawNode(void *n) { node_ = n; }

    std::shared_ptr<Doc> owner_;  // set only for owning values
    Doc *doc_{nullptr};           // owning doc or borrowed; nullptr => null
    void *node_{nullptr};         // yyjson_val*/yyjson_mut_val* (opaque)

    static Value const &nullSingleton();
};

class ValueConstIterator
{
  public:
    using iterator_category = std::bidirectional_iterator_tag;
    using value_type = Value;
    using difference_type = std::ptrdiff_t;
    using pointer = Value const *;
    using reference = Value const &;

    ValueConstIterator();
    ValueConstIterator(ValueConstIterator const &other);
    ValueConstIterator &operator=(ValueConstIterator const &other);
    reference operator*() const;
    pointer operator->() const;
    ValueConstIterator &operator++();
    ValueConstIterator operator++(int);
    ValueConstIterator &operator--();
    ValueConstIterator operator--(int);
    bool operator==(ValueConstIterator const &other) const;
    bool operator!=(ValueConstIterator const &other) const;

  private:
    friend class Value;
    explicit ValueConstIterator(Value const &container, bool atBegin);
    void sync() const;
    mutable Value current_;
    Doc *doc_{nullptr};
    void *arr_{nullptr};
    void *cur_{nullptr};
    std::size_t index_{0};
    std::size_t size_{0};
};

class ValueIterator
{
  public:
    using iterator_category = std::bidirectional_iterator_tag;
    using value_type = Value;
    using difference_type = std::ptrdiff_t;
    using pointer = Value *;
    using reference = Value &;

    ValueIterator();
    ValueIterator(ValueIterator const &other);
    ValueIterator &operator=(ValueIterator const &other);
    reference operator*() const;
    pointer operator->() const;
    ValueIterator &operator++();
    ValueIterator operator++(int);
    ValueIterator &operator--();
    ValueIterator operator--(int);
    bool operator==(ValueIterator const &other) const;
    bool operator!=(ValueIterator const &other) const;

  private:
    friend class Value;
    explicit ValueIterator(Value &container, bool atBegin);
    void sync() const;
    mutable Value current_;
    Doc *doc_{nullptr};
    void *arr_{nullptr};
    void *cur_{nullptr};
    std::size_t index_{0};
    std::size_t size_{0};
};

}  // namespace Json
