// jsoncpp-compatible shim (yyjson backed). Json::Value implementation.
// A Value is always a handle over a node in a shared yyjson doc; all JSON data
// (scalars included) lives in yyjson. Parsed values keep the immutable doc;
// mutation COWs to mutable.
#include "json_internal.h"

#include <cmath>
#include <cstring>
#include <utility>

namespace Json
{
Exception::Exception(JSONCPP_STRING const &msg) : msg_(msg) {}
char const *Exception::what() const noexcept { return msg_.c_str(); }
RuntimeError::RuntimeError(JSONCPP_STRING const &msg) : Exception(msg) {}
LogicError::LogicError(JSONCPP_STRING const &msg) : Exception(msg) {}
[[noreturn]] void throwRuntimeError(JSONCPP_STRING const &msg) { throw RuntimeError(msg); }
[[noreturn]] void throwLogicError(JSONCPP_STRING const &msg) { throw LogicError(msg); }

static yyjson_mut_val *copyNodeInto(yyjson_mut_doc *d, Doc *srcDoc, void *src)
{
    if (!src) return yyjson_mut_null(d);
    if (srcDoc && srcDoc->isMut) return yyjson_mut_val_mut_copy(d, static_cast<yyjson_mut_val *>(src));
    return yyjson_val_mut_copy(d, static_cast<yyjson_val *>(src));
}
static ValueType yyTypeToValueType(yyjson_val *n)
{
    if (!n) return nullValue;
    switch (yyjson_get_type(n))
    {
    case YYJSON_TYPE_NULL: return nullValue;
    case YYJSON_TYPE_BOOL: return booleanValue;
    case YYJSON_TYPE_NUM:
        if (yyjson_get_subtype(n) == YYJSON_SUBTYPE_REAL) return realValue;
        return yyjson_get_subtype(n) == YYJSON_SUBTYPE_UINT ? uintValue : intValue;
    case YYJSON_TYPE_STR: return stringValue;
    case YYJSON_TYPE_ARR: return arrayValue;
    case YYJSON_TYPE_OBJ: return objectValue;
    default: return nullValue;
    }
}
static void *vObjGet(Doc *d, void *n, const char *key)
{
    if (!n) return nullptr;
    return d && d->isMut ? (void *)yyjson_mut_obj_get((yyjson_mut_val *)n, key)
                         : (void *)yyjson_obj_get((yyjson_val *)n, key);
}
static void *vArrGet(Doc *d, void *n, size_t i)
{
    if (!n) return nullptr;
    return d && d->isMut ? (void *)yyjson_mut_arr_get((yyjson_mut_val *)n, i)
                         : (void *)yyjson_arr_get((yyjson_val *)n, i);
}
static size_t vArrSize(Doc *d, void *n)
{
    if (!n) return 0;
    return d && d->isMut ? yyjson_mut_arr_size((yyjson_mut_val *)n) : yyjson_arr_size((yyjson_val *)n);
}
static size_t vObjSize(Doc *d, void *n)
{
    if (!n) return 0;
    return d && d->isMut ? yyjson_mut_obj_size((yyjson_mut_val *)n) : yyjson_obj_size((yyjson_val *)n);
}
static void *vArrFirst(Doc *d, void *arr)
{
    if (!arr) return nullptr;
    return d && d->isMut ? (void *)yyjson_mut_arr_get_first((yyjson_mut_val *)arr)
                         : (void *)unsafe_yyjson_get_first((yyjson_val *)arr);
}
static void *vArrNext(Doc *d, void *val)
{
    if (!val) return nullptr;
    return d && d->isMut ? (void *)((yyjson_mut_val *)val)->next
                         : (void *)unsafe_yyjson_get_next((yyjson_val *)val);
}

Value const &Value::nullSingleton()
{
    static Value const null;
    return null;
}
void Value::initOwn()
{
    owner_ = std::make_shared<Doc>(true);
    doc_ = owner_.get();
    yyjson_mut_val *n = yyjson_mut_null(doc_->mdoc);
    yyjson_mut_doc_set_root(doc_->mdoc, n);
    node_ = n;
}
void Value::makeOwnedCopyOf(Value const &other)
{
    owner_ = std::make_shared<Doc>(true);
    doc_ = owner_.get();
    yyjson_mut_val *n = copyNodeInto(doc_->mdoc, other.doc_, other.node_);
    yyjson_mut_doc_set_root(doc_->mdoc, n);
    node_ = n;
}
void Value::ensureMutable()
{
    if (!doc_ || doc_->isMut) return;
    yyjson_mut_doc *m = yyjson_mut_doc_new(nullptr);
    yyjson_mut_val *cp = yyjson_val_mut_copy(m, static_cast<yyjson_val *>(node_));
    yyjson_mut_doc_set_root(m, cp);
    owner_ = std::make_shared<Doc>(true);
    owner_->mdoc = m;
    doc_ = owner_.get();
    node_ = cp;
}

Value::Value() { initOwn(); }
Value::Value(ValueType type)
{
    initOwn();
    yyjson_mut_doc *d = doc_->mdoc;
    yyjson_mut_val *n = (type == arrayValue) ? yyjson_mut_arr(d) : (type == objectValue) ? yyjson_mut_obj(d) : yyjson_mut_null(d);
    node_ = n;
    yyjson_mut_doc_set_root(d, n);
}
Value::Value(bool value) { initOwn(); yyjson_mut_val *n = yyjson_mut_bool(doc_->mdoc, value); node_ = n; yyjson_mut_doc_set_root(doc_->mdoc, n); }
Value::Value(Int value) { initOwn(); yyjson_mut_val *n = yyjson_mut_sint(doc_->mdoc, value); node_ = n; yyjson_mut_doc_set_root(doc_->mdoc, n); }
Value::Value(UInt value) { initOwn(); yyjson_mut_val *n = yyjson_mut_uint(doc_->mdoc, value); node_ = n; yyjson_mut_doc_set_root(doc_->mdoc, n); }
Value::Value(Int64 value) { initOwn(); yyjson_mut_val *n = yyjson_mut_sint(doc_->mdoc, value); node_ = n; yyjson_mut_doc_set_root(doc_->mdoc, n); }
Value::Value(UInt64 value) { initOwn(); yyjson_mut_val *n = yyjson_mut_uint(doc_->mdoc, value); node_ = n; yyjson_mut_doc_set_root(doc_->mdoc, n); }
Value::Value(double value) { initOwn(); yyjson_mut_val *n = yyjson_mut_real(doc_->mdoc, value); node_ = n; yyjson_mut_doc_set_root(doc_->mdoc, n); }
Value::Value(const char *value) { initOwn(); yyjson_mut_val *n = yyjson_mut_strcpy(doc_->mdoc, value ? value : ""); node_ = n; yyjson_mut_doc_set_root(doc_->mdoc, n); }
Value::Value(const char *begin, const char *end) { initOwn(); size_t len = (begin && end > begin) ? static_cast<size_t>(end - begin) : 0; yyjson_mut_val *n = yyjson_mut_strncpy(doc_->mdoc, begin ? begin : "", len); node_ = n; yyjson_mut_doc_set_root(doc_->mdoc, n); }
Value::Value(std::string const &value) { initOwn(); yyjson_mut_val *n = yyjson_mut_strncpy(doc_->mdoc, value.data(), value.size()); node_ = n; yyjson_mut_doc_set_root(doc_->mdoc, n); }
Value::Value(std::string &&value) { initOwn(); yyjson_mut_val *n = yyjson_mut_strncpy(doc_->mdoc, value.data(), value.size()); node_ = n; yyjson_mut_doc_set_root(doc_->mdoc, n); }

Value::Value(Value const &other) { initOwn(); makeOwnedCopyOf(other); }
Value::Value(Value &&other) noexcept : owner_(std::move(other.owner_)), doc_(other.doc_), node_(other.node_) { other.doc_ = nullptr; other.node_ = nullptr; }
Value::~Value() = default;

Value &Value::operator=(Value const &other)
{
    if (this == &other) return *this;
    if (owner_) { makeOwnedCopyOf(other); return *this; }
    if (!node_ || !doc_) return *this;
    ensureMutable();
    yyjson_mut_doc *d = doc_->mdoc;
    yyjson_mut_val *n = static_cast<yyjson_mut_val *>(node_);
    yyjson_val *src = static_cast<yyjson_val *>(other.node_);
    if (!src || yyjson_is_null(src)) yyjson_mut_set_null(n);
    else if (yyjson_is_bool(src)) yyjson_mut_set_bool(n, yyjson_get_bool(src));
    else if (yyjson_is_uint(src)) yyjson_mut_set_uint(n, yyjson_get_uint(src));
    else if (yyjson_is_int(src)) yyjson_mut_set_sint(n, yyjson_get_sint(src));
    else if (yyjson_is_real(src)) yyjson_mut_set_real(n, yyjson_get_real(src));
    else if (yyjson_is_str(src))
    {
        const char *a = doc_->anchor(JSONCPP_STRING(yyjson_get_str(src), yyjson_get_len(src)));
        yyjson_mut_set_str(n, a);
    }
    else if (yyjson_is_arr(src))
    {
        yyjson_mut_set_arr(n);
        if (other.doc_ && other.doc_->isMut)
        { size_t idx, max; yyjson_mut_val *e; yyjson_mut_arr_foreach((yyjson_mut_val *)src, idx, max, e) yyjson_mut_arr_append(n, yyjson_mut_val_mut_copy(d, e)); }
        else
        { size_t idx, max; yyjson_val *e; yyjson_arr_foreach(src, idx, max, e) yyjson_mut_arr_append(n, yyjson_val_mut_copy(d, e)); }
    }
    else if (yyjson_is_obj(src))
    {
        yyjson_mut_set_obj(n);
        if (other.doc_ && other.doc_->isMut)
        {
            yyjson_mut_obj_iter iter;
            if (yyjson_mut_obj_iter_init((yyjson_mut_val *)src, &iter))
            { yyjson_mut_val *key; while ((key = yyjson_mut_obj_iter_next(&iter))) { yyjson_mut_val *val = yyjson_mut_obj_iter_get_val(key); yyjson_mut_obj_add(n, yyjson_mut_strncpy(d, yyjson_mut_get_str(key), yyjson_mut_get_len(key)), yyjson_mut_val_mut_copy(d, val)); } }
        }
        else
        {
            yyjson_obj_iter iter;
            if (yyjson_obj_iter_init(src, &iter))
            { yyjson_val *key; while ((key = yyjson_obj_iter_next(&iter))) { yyjson_val *val = yyjson_obj_iter_get_val(key); yyjson_mut_obj_add(n, yyjson_mut_strncpy(d, yyjson_get_str(key), yyjson_get_len(key)), yyjson_val_mut_copy(d, val)); } }
        }
    }
    return *this;
}
Value &Value::operator=(Value &&other) noexcept
{
    if (this == &other) return *this;
    if (owner_) { owner_ = std::move(other.owner_); doc_ = other.doc_; node_ = other.node_; other.doc_ = nullptr; other.node_ = nullptr; }
    else return operator=(static_cast<Value const &>(other));
    return *this;
}
Value &Value::operator=(const char *value) { if (node_ && doc_) { ensureMutable(); const char *a = doc_->anchor(JSONCPP_STRING(value ? value : "")); yyjson_mut_set_str(static_cast<yyjson_mut_val *>(node_), a); } return *this; }
Value &Value::operator=(bool value) { if (node_) { ensureMutable(); yyjson_mut_set_bool(static_cast<yyjson_mut_val *>(node_), value); } return *this; }
Value &Value::operator=(Int value) { if (node_) { ensureMutable(); yyjson_mut_set_sint(static_cast<yyjson_mut_val *>(node_), value); } return *this; }
Value &Value::operator=(UInt value) { if (node_) { ensureMutable(); yyjson_mut_set_uint(static_cast<yyjson_mut_val *>(node_), value); } return *this; }
Value &Value::operator=(Int64 value) { if (node_) { ensureMutable(); yyjson_mut_set_sint(static_cast<yyjson_mut_val *>(node_), value); } return *this; }
Value &Value::operator=(UInt64 value) { if (node_) { ensureMutable(); yyjson_mut_set_uint(static_cast<yyjson_mut_val *>(node_), value); } return *this; }
Value &Value::operator=(double value) { if (node_) { ensureMutable(); yyjson_mut_set_real(static_cast<yyjson_mut_val *>(node_), value); } return *this; }
Value &Value::operator=(std::string const &value) { if (node_ && doc_) { ensureMutable(); const char *a = doc_->anchor(value); yyjson_mut_set_str(static_cast<yyjson_mut_val *>(node_), a); } return *this; }
void Value::swap(Value &other) { owner_.swap(other.owner_); std::swap(doc_, other.doc_); std::swap(node_, other.node_); }

ValueType Value::type() const { return yyTypeToValueType(static_cast<yyjson_val *>(node_)); }
bool Value::operator!() const { return isNull(); }
Value::operator bool() const { return !isNull(); }
bool Value::isNull() const { return !node_ || yyjson_is_null(static_cast<yyjson_val *>(node_)); }
bool Value::isBool() const { return node_ && yyjson_is_bool(static_cast<yyjson_val *>(node_)); }
bool Value::isInt() const
{
    if (!node_) return true;
    yyjson_val *n = static_cast<yyjson_val *>(node_);
    if (yyjson_is_int(n)) return yyjson_get_sint(n) >= minInt && yyjson_get_sint(n) <= maxInt;
    if (yyjson_is_uint(n)) return yyjson_get_uint(n) <= UInt(maxInt);
    if (yyjson_is_real(n)) { double r = yyjson_get_real(n); return std::isfinite(r) && r >= minInt && r < maxInt; }
    return isNull() || isBool();
}
bool Value::isInt64() const
{
    if (!node_) return true;
    yyjson_val *n = static_cast<yyjson_val *>(node_);
    if (yyjson_is_int(n)) return true;
    if (yyjson_is_uint(n)) return yyjson_get_uint(n) <= UInt64(maxInt64);
    if (yyjson_is_real(n)) { double r = yyjson_get_real(n); return std::isfinite(r) && r >= minInt64 && r < maxInt64; }
    return isNull() || isBool();
}
bool Value::isUInt() const
{
    if (!node_) return true;
    yyjson_val *n = static_cast<yyjson_val *>(node_);
    if (yyjson_is_int(n)) return yyjson_get_sint(n) >= 0;
    if (yyjson_is_uint(n)) return yyjson_get_uint(n) <= maxUInt;
    if (yyjson_is_real(n)) { double r = yyjson_get_real(n); return std::isfinite(r) && r >= 0 && r < maxUInt; }
    return isNull() || isBool();
}
bool Value::isUInt64() const
{
    if (!node_) return true;
    yyjson_val *n = static_cast<yyjson_val *>(node_);
    if (yyjson_is_int(n)) return yyjson_get_sint(n) >= 0;
    if (yyjson_is_uint(n)) return true;
    if (yyjson_is_real(n)) { double r = yyjson_get_real(n); return std::isfinite(r) && r >= 0 && r < maxUInt64; }
    return isNull() || isBool();
}
bool Value::isIntegral() const
{
    if (!node_) return true;
    yyjson_val *n = static_cast<yyjson_val *>(node_);
    if (yyjson_is_int(n) || yyjson_is_uint(n) || isBool()) return true;
    if (yyjson_is_real(n)) { double r = yyjson_get_real(n); return std::isfinite(r) && r == std::floor(r) && r >= double(minInt64) && r < double(maxUInt64); }
    return false;
}
bool Value::isDouble() const { if (!node_) return false; yyjson_val *n = static_cast<yyjson_val *>(node_); return yyjson_is_real(n) || yyjson_is_int(n) || yyjson_is_uint(n); }
bool Value::isNumeric() const { return node_ && yyjson_is_num(static_cast<yyjson_val *>(node_)); }
bool Value::isString() const { return node_ && yyjson_is_str(static_cast<yyjson_val *>(node_)); }
bool Value::isArray() const { return node_ && yyjson_is_arr(static_cast<yyjson_val *>(node_)); }
bool Value::isObject() const { return node_ && yyjson_is_obj(static_cast<yyjson_val *>(node_)); }
bool Value::isConvertibleTo(ValueType) const { return true; }
ArrayIndex Value::size() const
{
    if (!node_) return 0;
    yyjson_val *n = static_cast<yyjson_val *>(node_);
    if (yyjson_is_arr(n)) return static_cast<ArrayIndex>(vArrSize(doc_, node_));
    if (yyjson_is_obj(n)) return static_cast<ArrayIndex>(vObjSize(doc_, node_));
    return 0;
}
bool Value::empty() const { if (isNull()) return true; return size() == 0; }
void Value::clear()
{
    if (!node_) return;
    ensureMutable();
    yyjson_mut_val *n = static_cast<yyjson_mut_val *>(node_);
    if (yyjson_mut_is_arr(n)) yyjson_mut_set_arr(n);
    else if (yyjson_mut_is_obj(n)) yyjson_mut_set_obj(n);
    else yyjson_mut_set_null(n);
}
void Value::resize(ArrayIndex newSize)
{
    if (!node_) return;
    ensureMutable();
    yyjson_mut_val *n = static_cast<yyjson_mut_val *>(node_);
    if (!yyjson_mut_is_arr(n)) { if (newSize == 0) return; yyjson_mut_set_arr(n); }
    yyjson_mut_doc *d = doc_->mdoc;
    size_t cur = yyjson_mut_arr_size(n);
    while (cur < newSize) { yyjson_mut_arr_add_null(d, n); ++cur; }
    while (cur > newSize) { yyjson_mut_arr_remove(n, cur - 1); --cur; }
}

const char *Value::asCString() const { if (!isString()) throwLogicError("in Json::Value::asCString(): requires stringValue"); return yyjson_get_str(static_cast<yyjson_val *>(node_)); }
std::string_view Value::asStringView() const
{
    if (!node_ || !yyjson_is_str(static_cast<yyjson_val *>(node_))) return {};
    yyjson_val *n = static_cast<yyjson_val *>(node_);
    return std::string_view(yyjson_get_str(n), yyjson_get_len(n));
}
JSONCPP_STRING Value::asString() const
{
    if (!node_ || yyjson_is_null(static_cast<yyjson_val *>(node_))) return "";
    yyjson_val *n = static_cast<yyjson_val *>(node_);
    if (yyjson_is_str(n)) return JSONCPP_STRING(yyjson_get_str(n), yyjson_get_len(n));
    if (yyjson_is_bool(n)) return yyjson_get_bool(n) ? "true" : "false";
    if (yyjson_is_int(n)) return std::to_string(yyjson_get_sint(n));
    if (yyjson_is_uint(n)) return std::to_string(yyjson_get_uint(n));
    if (yyjson_is_real(n)) { char buf[64]; std::snprintf(buf, sizeof(buf), "%.17g", yyjson_get_real(n)); return JSONCPP_STRING(buf); }
    throwLogicError("Type is not convertible to string");
}
Int Value::asInt() const { return static_cast<Int>(asInt64()); }
UInt Value::asUInt() const { Int64 v = asInt64(); if (v < 0) throwLogicError("Value is not convertible to UInt."); return static_cast<UInt>(v); }
Int64 Value::asInt64() const
{
    if (!node_ || yyjson_is_null(static_cast<yyjson_val *>(node_))) return 0;
    yyjson_val *n = static_cast<yyjson_val *>(node_);
    if (yyjson_is_bool(n)) return yyjson_get_bool(n) ? 1 : 0;
    if (yyjson_is_int(n)) return yyjson_get_sint(n);
    if (yyjson_is_uint(n)) { UInt64 u = yyjson_get_uint(n); if (u <= UInt64(maxInt64)) return static_cast<Int64>(u); throwLogicError("LargestUInt out of Int64 range"); }
    if (yyjson_is_real(n)) return static_cast<Int64>(yyjson_get_real(n));
    throwLogicError("Value is not convertible to Int64.");
}
UInt64 Value::asUInt64() const
{
    if (!node_ || yyjson_is_null(static_cast<yyjson_val *>(node_))) return 0;
    yyjson_val *n = static_cast<yyjson_val *>(node_);
    if (yyjson_is_bool(n)) return yyjson_get_bool(n) ? 1 : 0;
    if (yyjson_is_uint(n)) return yyjson_get_uint(n);
    if (yyjson_is_int(n)) { Int64 s = yyjson_get_sint(n); if (s >= 0) return static_cast<UInt64>(s); throwLogicError("LargestInt out of UInt64 range"); }
    if (yyjson_is_real(n)) return static_cast<UInt64>(yyjson_get_real(n));
    throwLogicError("Value is not convertible to UInt64.");
}
LargestInt Value::asLargestInt() const { return asInt64(); }
LargestUInt Value::asLargestUInt() const { return asUInt64(); }
double Value::asDouble() const
{
    if (!node_ || yyjson_is_null(static_cast<yyjson_val *>(node_))) return 0.0;
    yyjson_val *n = static_cast<yyjson_val *>(node_);
    if (yyjson_is_bool(n)) return yyjson_get_bool(n) ? 1.0 : 0.0;
    if (yyjson_is_int(n)) return static_cast<double>(yyjson_get_sint(n));
    if (yyjson_is_uint(n)) return static_cast<double>(yyjson_get_uint(n));
    if (yyjson_is_real(n)) return yyjson_get_real(n);
    throwLogicError("Value is not convertible to double.");
}
float Value::asFloat() const { return static_cast<float>(asDouble()); }
bool Value::asBool() const
{
    if (!node_ || yyjson_is_null(static_cast<yyjson_val *>(node_))) return false;
    yyjson_val *n = static_cast<yyjson_val *>(node_);
    if (yyjson_is_bool(n)) return yyjson_get_bool(n);
    if (yyjson_is_int(n)) return yyjson_get_sint(n) != 0;
    if (yyjson_is_uint(n)) return yyjson_get_uint(n) != 0;
    if (yyjson_is_real(n)) return yyjson_get_real(n) != 0.0;
    throwLogicError("Value is not convertible to bool.");
}

Value &Value::cacheHandle(void *child) const
{
    Doc *h = doc_;
    auto it = h->cacheIndex.find(child);
    if (it != h->cacheIndex.end()) return *it->second;
    h->cache.emplace_back(Value(h, child));
    auto listIt = std::prev(h->cache.end());
    h->cacheIndex[child] = listIt;
    return *listIt;
}
Value &Value::operator[](const char *key)
{
    if (!node_ || !doc_) return const_cast<Value &>(nullSingleton());
    ensureMutable();
    yyjson_mut_doc *d = doc_->mdoc;
    yyjson_mut_val *n = static_cast<yyjson_mut_val *>(node_);
    if (!yyjson_mut_is_obj(n)) yyjson_mut_set_obj(n);
    yyjson_mut_val *child = yyjson_mut_obj_get(n, key);
    if (!child) { yyjson_mut_val *k = yyjson_mut_strcpy(d, key ? key : ""); yyjson_mut_val *v = yyjson_mut_null(d); yyjson_mut_obj_add(n, k, v); child = v; }
    return cacheHandle(child);
}
Value const &Value::operator[](const char *key) const
{
    if (!node_) return nullSingleton();
    yyjson_val *n = static_cast<yyjson_val *>(node_);
    if (yyjson_is_obj(n)) { void *child = vObjGet(doc_, node_, key); if (child) return cacheHandle(child); }
    return nullSingleton();
}
Value &Value::operator[](const JSONCPP_STRING &key) { return operator[](key.c_str()); }
Value const &Value::operator[](const JSONCPP_STRING &key) const { return operator[](key.c_str()); }
Value &Value::operator[](ArrayIndex index)
{
    if (!node_ || !doc_) return const_cast<Value &>(nullSingleton());
    ensureMutable();
    yyjson_mut_doc *d = doc_->mdoc;
    yyjson_mut_val *n = static_cast<yyjson_mut_val *>(node_);
    if (!yyjson_mut_is_arr(n)) yyjson_mut_set_arr(n);
    size_t sz = yyjson_mut_arr_size(n);
    while (sz <= index) { yyjson_mut_arr_add_null(d, n); ++sz; }
    yyjson_mut_val *child = yyjson_mut_arr_get(n, index);
    return cacheHandle(child);
}
Value const &Value::operator[](ArrayIndex index) const
{
    if (node_)
    {
        yyjson_val *n = static_cast<yyjson_val *>(node_);
        if (yyjson_is_arr(n) && index < vArrSize(doc_, node_)) { void *child = vArrGet(doc_, node_, index); if (child) return cacheHandle(child); }
    }
    return nullSingleton();
}
Value &Value::operator[](int index) { return operator[](static_cast<ArrayIndex>(index)); }
Value const &Value::operator[](int index) const { return operator[](static_cast<ArrayIndex>(index)); }

Value Value::get(const char *key, Value const &defaultValue) const
{
    if (node_)
    {
        yyjson_val *n = static_cast<yyjson_val *>(node_);
        if (yyjson_is_obj(n)) { void *child = vObjGet(doc_, node_, key); if (child) return Value(doc_, child); }
    }
    return defaultValue;
}
Value Value::get(const JSONCPP_STRING &key, Value const &defaultValue) const { return get(key.c_str(), defaultValue); }
Value Value::get(ArrayIndex index, Value const &defaultValue) const
{
    if (node_)
    {
        yyjson_val *n = static_cast<yyjson_val *>(node_);
        if (yyjson_is_arr(n) && index < vArrSize(doc_, node_)) { void *child = vArrGet(doc_, node_, index); if (child) return Value(doc_, child); }
    }
    return defaultValue;
}
bool Value::isMember(const char *key) const
{
    if (!node_) return false;
    yyjson_val *n = static_cast<yyjson_val *>(node_);
    return yyjson_is_obj(n) && vObjGet(doc_, node_, key) != nullptr;
}
bool Value::isMember(const JSONCPP_STRING &key) const { return isMember(key.c_str()); }
Value::Members Value::getMemberNames() const
{
    Members names;
    if (!node_) return names;
    yyjson_val *n = static_cast<yyjson_val *>(node_);
    if (!yyjson_is_obj(n)) return names;
    names.reserve(vObjSize(doc_, node_));
    if (doc_ && doc_->isMut)
    {
        yyjson_mut_obj_iter iter;
        if (yyjson_mut_obj_iter_init((yyjson_mut_val *)n, &iter)) { yyjson_mut_val *key; while ((key = yyjson_mut_obj_iter_next(&iter))) names.emplace_back(yyjson_mut_get_str(key), yyjson_mut_get_len(key)); }
    }
    else
    {
        yyjson_obj_iter iter;
        if (yyjson_obj_iter_init(n, &iter)) { yyjson_val *key; while ((key = yyjson_obj_iter_next(&iter))) names.emplace_back(yyjson_get_str(key), yyjson_get_len(key)); }
    }
    return names;
}
void Value::removeMember(const char *key) { if (!node_) return; ensureMutable(); yyjson_mut_obj_remove_key(static_cast<yyjson_mut_val *>(node_), key); }
void Value::removeMember(const JSONCPP_STRING &key) { removeMember(key.c_str()); }
Value &Value::append(Value const &value)
{
    if (!node_ || !doc_) return const_cast<Value &>(nullSingleton());
    ensureMutable();
    yyjson_mut_doc *d = doc_->mdoc;
    yyjson_mut_val *n = static_cast<yyjson_mut_val *>(node_);
    if (!yyjson_mut_is_arr(n)) yyjson_mut_set_arr(n);
    yyjson_mut_val *child = copyNodeInto(d, value.doc_, value.node_);
    yyjson_mut_arr_append(n, child);
    return cacheHandle(child);
}
Value &Value::append(Value &&value) { return append(static_cast<Value const &>(value)); }

Value::iterator Value::begin() { return iterator(*this, true); }
Value::iterator Value::end() { return iterator(*this, false); }
Value::const_iterator Value::begin() const { return const_iterator(*this, true); }
Value::const_iterator Value::end() const { return const_iterator(*this, false); }

bool Value::operator==(Value const &other) const
{
    bool aNull = isNull(), bNull = other.isNull();
    if (aNull && bNull) return true;
    if (aNull != bNull) return false;
    return unsafe_yyjson_equals(static_cast<yyjson_val *>(node_), static_cast<yyjson_val *>(other.node_));
}
bool Value::operator!=(Value const &other) const { return !(*this == other); }

// ---- iterators (array element traversal, bidirectional via index) ----
ValueConstIterator::ValueConstIterator() = default;
ValueConstIterator::ValueConstIterator(ValueConstIterator const &other)
    : current_(Value(other.doc_, nullptr)),
      doc_(other.doc_),
      arr_(other.arr_),
      cur_(other.cur_),
      index_(other.index_),
      size_(other.size_)
{
}
ValueConstIterator &ValueConstIterator::operator=(ValueConstIterator const &other)
{
    current_.owner_.reset();
    current_.doc_ = nullptr;
    current_.node_ = nullptr;
    doc_ = other.doc_;
    arr_ = other.arr_;
    cur_ = other.cur_;
    index_ = other.index_;
    size_ = other.size_;
    return *this;
}
ValueConstIterator::ValueConstIterator(Value const &container, bool atBegin)
    : current_(Value(container.doc_, nullptr))
{
    doc_ = container.doc_;
    yyjson_val *n = static_cast<yyjson_val *>(container.node_);
    arr_ = (n && yyjson_is_arr(n)) ? container.node_ : nullptr;
    size_ = arr_ ? vArrSize(doc_, arr_) : 0;
    index_ = atBegin ? 0 : size_;
    cur_ = (atBegin && size_ > 0) ? vArrFirst(doc_, arr_) : nullptr;
}
void ValueConstIterator::sync() const
{
    current_.owner_.reset();
    current_.doc_ = doc_;
    current_.setRawNode(cur_);
}
ValueConstIterator::reference ValueConstIterator::operator*() const
{
    sync();
    return current_;
}
ValueConstIterator::pointer ValueConstIterator::operator->() const
{
    sync();
    return &current_;
}
ValueConstIterator &ValueConstIterator::operator++()
{
    if (index_ < size_)
    {
        cur_ = vArrNext(doc_, cur_);
        ++index_;
    }
    return *this;
}
ValueConstIterator ValueConstIterator::operator++(int)
{
    ValueConstIterator t = *this;
    ++(*this);
    return t;
}
ValueConstIterator &ValueConstIterator::operator--()
{
    if (index_ > 0)
    {
        --index_;
        cur_ = vArrGet(doc_, arr_, index_);
    }
    return *this;
}
ValueConstIterator ValueConstIterator::operator--(int)
{
    ValueConstIterator t = *this;
    --(*this);
    return t;
}
bool ValueConstIterator::operator==(ValueConstIterator const &other) const
{
    return arr_ == other.arr_ && index_ == other.index_;
}
bool ValueConstIterator::operator!=(ValueConstIterator const &other) const
{
    return !(*this == other);
}

ValueIterator::ValueIterator() = default;
ValueIterator::ValueIterator(ValueIterator const &other)
    : current_(Value(other.doc_, nullptr)),
      doc_(other.doc_),
      arr_(other.arr_),
      cur_(other.cur_),
      index_(other.index_),
      size_(other.size_)
{
}
ValueIterator &ValueIterator::operator=(ValueIterator const &other)
{
    current_.owner_.reset();
    current_.doc_ = nullptr;
    current_.node_ = nullptr;
    doc_ = other.doc_;
    arr_ = other.arr_;
    cur_ = other.cur_;
    index_ = other.index_;
    size_ = other.size_;
    return *this;
}
ValueIterator::ValueIterator(Value &container, bool atBegin)
    : current_(Value(container.doc_, nullptr))
{
    doc_ = container.doc_;
    yyjson_val *n = static_cast<yyjson_val *>(container.node_);
    arr_ = (n && yyjson_is_arr(n)) ? container.node_ : nullptr;
    size_ = arr_ ? vArrSize(doc_, arr_) : 0;
    index_ = atBegin ? 0 : size_;
    cur_ = (atBegin && size_ > 0) ? vArrFirst(doc_, arr_) : nullptr;
}
void ValueIterator::sync() const
{
    current_.owner_.reset();
    current_.doc_ = doc_;
    current_.setRawNode(cur_);
}
ValueIterator::reference ValueIterator::operator*() const
{
    sync();
    return current_;
}
ValueIterator::pointer ValueIterator::operator->() const
{
    sync();
    return &current_;
}
ValueIterator &ValueIterator::operator++()
{
    if (index_ < size_)
    {
        cur_ = vArrNext(doc_, cur_);
        ++index_;
    }
    return *this;
}
ValueIterator ValueIterator::operator++(int)
{
    ValueIterator t = *this;
    ++(*this);
    return t;
}
ValueIterator &ValueIterator::operator--()
{
    if (index_ > 0)
    {
        --index_;
        cur_ = vArrGet(doc_, arr_, index_);
    }
    return *this;
}
ValueIterator ValueIterator::operator--(int)
{
    ValueIterator t = *this;
    --(*this);
    return t;
}
bool ValueIterator::operator==(ValueIterator const &other) const
{
    return arr_ == other.arr_ && index_ == other.index_;
}
bool ValueIterator::operator!=(ValueIterator const &other) const
{
    return !(*this == other);
}

}  // namespace Json


