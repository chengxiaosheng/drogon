#include "MethodTest.h"

static void makeGetRespose(
    const std::function<void(const HttpResponsePtr &)> &callback)
{
    callback(drogon::HttpResponse::newHttpJsonResponse("GET"));
}

static void makePostRespose(
    const std::function<void(const HttpResponsePtr &)> &callback)
{
    callback(drogon::HttpResponse::newHttpJsonResponse("POST"));
}

void MethodTest::get(const HttpRequestPtr &req,
                     std::function<void(const HttpResponsePtr &)> &&callback)
{
    DebugL;
    makeGetRespose(callback);
}

void MethodTest::post(const HttpRequestPtr &req,
                      std::function<void(const HttpResponsePtr &)> &&callback,
                      std::string str)
{
    DebugL << str;
    makePostRespose(callback);
}

void MethodTest::getReg(const HttpRequestPtr &req,
                        std::function<void(const HttpResponsePtr &)> &&callback,
                        std::string regStr)
{
    DebugL << regStr;
    makeGetRespose(callback);
}

void MethodTest::postReg(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback,
    std::string regStr,
    std::string str)
{
    DebugL << regStr;
    DebugL << str;
    makePostRespose(callback);
}

void MethodTest::postRegex(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback,
    std::string regStr)
{
    DebugL << regStr;
    makePostRespose(callback);
}
