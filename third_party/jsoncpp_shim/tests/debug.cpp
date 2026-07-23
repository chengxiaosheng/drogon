#include <json/json.h>
#include <iostream>
int main(){
    Json::Value v;
    v["a"] = 1;
    v["b"] = 2;
    std::cout << "built isObject=" << v.isObject() << " size=" << v.size() << "\n";
    std::cout << "v[a]=" << v["a"].asInt() << " v[b]=" << v["b"].asInt() << "\n";
    std::string err;
    Json::CharReaderBuilder rb;
    auto r = rb.newCharReader();
    Json::Value p;
    std::string s = R"({"a":1,"b":2})";
    bool ok = r->parse(s.data(), s.data()+s.size(), &p, &err);
    std::cout << "parse ok=" << ok << " isObject=" << p.isObject() << " size=" << p.size() << "\n";
    std::cout << "p[a] (non-const, triggers COW)=" << p["a"].asInt() << "\n";
    std::cout << "after COW isObject=" << p.isObject() << " size=" << p.size() << "\n";
    return 0;
}
