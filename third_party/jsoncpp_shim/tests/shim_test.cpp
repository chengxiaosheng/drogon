// Standalone tests for the jsoncpp-compatible yyjson shim.
#include <json/json.h>
#include <cassert>
#include <iostream>
#include <string>

static int g_fail = 0;
#define CHECK(cond)                                                       \
    do                                                                    \
    {                                                                     \
        if (!(cond))                                                      \
        {                                                                 \
            std::cerr << "FAIL " << __LINE__ << ": " #cond << std::endl;  \
            ++g_fail;                                                     \
        }                                                                 \
    } while (0)

static Json::Value parse(const std::string &s, std::string &err)
{
    Json::CharReaderBuilder b;
    auto r = b.newCharReader();
    Json::Value v;
    r->parse(s.data(), s.data() + s.size(), &v, &err);
    return v;
}

int main()
{
    std::cerr << "build" << std::flush;
    // ---- build + read back ----
    {
        Json::Value v;
        v["path"] = "json";
        v["name"] = "json test";
        v["count"] = 5;
        v["ratio"] = 1.5;
        v["flag"] = true;
        CHECK(v["path"].asString() == "json");
        CHECK(v["count"].asInt() == 5);
        CHECK(v["count"].asInt64() == 5);
        CHECK(v["ratio"].asDouble() == 1.5);
        CHECK(v["flag"].asBool() == true);
        CHECK(v.isMember("path"));
        CHECK(!v.isMember("missing"));
        CHECK(v.get("missing", Json::Value("d")).asString() == "d");
        CHECK(v.get("count", 0).asInt() == 5);
        CHECK(v.type() == Json::objectValue);
        CHECK(v.size() == 5);
    }

    std::cerr << "array" << std::flush;
    // ---- array append + iteration ----
    {
        Json::Value arr(Json::arrayValue);
        for (int i = 0; i < 5; ++i)
        {
            Json::Value user;
            user["id"] = i;
            user["name"] = "none";
            arr.append(user);
        }
        CHECK(arr.size() == 5);
        CHECK(arr.isArray());
        int sum = 0;
        for (auto const &u : arr)
            sum += u["id"].asInt();
        CHECK(sum == 0 + 1 + 2 + 3 + 4);
        CHECK(arr[2]["id"].asInt() == 2);
    }

    std::cerr << "parse" << std::flush;
    // ---- parse + access ----
    {
        std::string err;
        Json::Value v = parse(R"({"a":1,"b":"hi","c":[10,20,30],"d":{"e":true}})", err);
        CHECK(err.empty());
        CHECK(v["a"].asInt() == 1);
        CHECK(v["b"].asString() == "hi");
        CHECK(v["c"].size() == 3);
        CHECK(v["c"][0].asInt() == 10);
        CHECK(v["c"][2].asInt() == 30);
        CHECK(v["d"]["e"].asBool() == true);
        CHECK(v.get("a", 0).asInt() == 1);
    }

    std::cerr << "serialize" << std::flush;
    // ---- serialize round-trip ----
    {
        Json::Value v;
        v["name"] = "test";
        v["n"] = 42;
        v["arr"].append(1);
        v["arr"].append(2);
        Json::StreamWriterBuilder wb;
        wb["indentation"] = "";
        std::string s = Json::writeString(wb, v);
        std::string err;
        Json::Value v2 = parse(s, err);
        CHECK(err.empty());
        CHECK(v2["name"].asString() == "test");
        CHECK(v2["n"].asInt() == 42);
        CHECK(v2["arr"][1].asInt() == 2);
        // compact (no pretty whitespace inside objects)
        CHECK(s.find("\n") == std::string::npos);
    }

    // ---- toStyledString (pretty) ----
    {
        Json::Value v;
        v["x"] = 1;
        std::string s = v.toStyledString();
        CHECK(s.find("\n") != std::string::npos);
    }

    std::cerr << "unicode" << std::flush;
    // ---- unicode round-trip ----
    {
        Json::Value v;
        v["name"] = "\xe5\xbc\xa0\xe4\xb8\x89";  // 张三 in UTF-8
        Json::StreamWriterBuilder wb;
        wb["indentation"] = "";
        std::string s = Json::writeString(wb, v);
        std::string err;
        Json::Value v2 = parse(s, err);
        CHECK(v2["name"].asString() == "\xe5\xbc\xa0\xe4\xb8\x89");
    }

    std::cerr << "unicode" << std::flush;
    // ---- unicode escaping ----
    {
        Json::Value v;
        v["name"] = "\xe5\xbc\xa0\xe4\xb8\x89";
        Json::StreamWriterBuilder wb;
        wb["indentation"] = "";
        wb["emitUTF8"] = false;  // escape non-ascii
        std::string s = Json::writeString(wb, v);
        CHECK(s.find("\\u") != std::string::npos);
    }

    std::cerr << "copy" << std::flush;
    // ---- copy independence ----
    {
        Json::Value v;
        v["a"] = 1;
        Json::Value c = v;  // deep copy
        c["a"] = 2;
        CHECK(v["a"].asInt() == 1);
        CHECK(c["a"].asInt() == 2);
    }

    // ---- reference mutation through operator[] ----
    {
        Json::Value v;
        v["a"] = 1;
        auto &ref = v["a"];
        ref = 99;
        CHECK(v["a"].asInt() == 99);
    }

    // ---- operator== with string ----
    {
        std::string err;
        Json::Value v = parse(R"({"P1":"upload","P2":"test"})", err);
        CHECK(v["P1"] == "upload");
        CHECK(v["P2"] == "test");
        CHECK(v["P1"] != "other");
    }

    // ---- getMemberNames ----
    {
        std::string err;
        Json::Value v = parse(R"({"connect":{"host":"127.0.0.1","port":5432}})", err);
        auto conn = v.get("connect", Json::Value());
        CHECK(conn.isObject());
        CHECK(!conn.empty());
        auto names = conn.getMemberNames();
        CHECK(names.size() == 2);
        CHECK(conn["host"].asString() == "127.0.0.1");
        CHECK(conn["port"].asInt() == 5432);
    }

    // ---- FastWriter ----
    {
        Json::Value v;
        v["x"] = 1;
        Json::FastWriter fw;
        std::string s = fw.write(v);
        CHECK(!s.empty() && s.back() == '\n');
    }

    std::cerr << "null" << std::flush;
    // ---- null semantics ----
    {
        Json::Value v;
        CHECK(v.isNull());
        CHECK(!v);
        CHECK(v.asString() == "");
        CHECK(v.asInt() == 0);
        std::string err;
        Json::Value p = parse("null", err);
        CHECK(p.isNull());
    }

    // ---- large int64 ----
    {
        Json::Value v;
        v["id"] = (Json::Int64)9223372036854775800LL;
        CHECK(v["id"].asInt64() == 9223372036854775800LL);
        std::string err;
        Json::Value p = parse(R"({"id":9223372036854775807})", err);
        CHECK(p["id"].asInt64() == 9223372036854775807LL);
        CHECK(p["id"].isInt64());
    }

    std::cerr << "chain" << std::flush;
    // ---- chained build ----
    {
        Json::Value root;
        root["plugins"] = Json::Value(Json::arrayValue);
        Json::Value plugin;
        plugin["name"] = "p1";
        plugin["deps"].append("a");
        plugin["deps"].append("b");
        root["plugins"].append(plugin);
        CHECK(root["plugins"][0]["name"].asString() == "p1");
        CHECK(root["plugins"][0]["deps"].size() == 2);
    }

    // ---- asStringView (zero-copy) + parse-then-mutate (COW) ----
    {
        std::string err;
        Json::Value p = parse(R"({"name":"hello","n":7})", err);
        CHECK(p["name"].asStringView() == std::string_view("hello"));
        // mutate a parsed (immutable) value: triggers COW to mutable
        p["n"] = 99;
        CHECK(p["n"].asInt() == 99);
        CHECK(p["name"].asString() == "hello");
        CHECK(p.size() == 2);
        p["added"] = "x";
        CHECK(p.isMember("added"));
    }

    // ---- inline scalars (no Doc allocation) ----
    {
        Json::Value n(5);
        Json::Value b(true);
        Json::Value d(1.5);
        Json::Value null;
        CHECK(n.asInt() == 5);
        CHECK(b.asBool() == true);
        CHECK(d.asDouble() == 1.5);
        CHECK(null.isNull());
        CHECK(n.isInt());
        Json::StreamWriterBuilder wb; wb["indentation"] = "";
        CHECK(Json::writeString(wb, n) == "5");
        CHECK(Json::writeString(wb, b) == "true");
        CHECK(Json::writeString(wb, null) == "null");
        // promote inline scalar to object
        Json::Value v;
        v["x"] = 1;
        CHECK(v.isObject() && v["x"].asInt() == 1);
        // copy of inline scalar stays inline and independent
        Json::Value c = n;
        CHECK(c.asInt() == 5);
    }

    if (g_fail)
    {
        std::cerr << g_fail << " test(s) failed" << std::endl;
        return 1;
    }
    std::cout << "ALL SHIM TESTS PASSED" << std::endl;
    return 0;
}
