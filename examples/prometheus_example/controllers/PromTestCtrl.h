#pragma once

#include <drogon/HttpController.h>
#include <drogon/utils/coroutine.h>

using namespace drogon;

class PromTestCtrl : public drogon::HttpController<PromTestCtrl>
{
  public:
    METHOD_LIST_BEGIN
    registerMethod(&PromTestCtrl::fast, "/fast", {"PromStat"}, false, "PromTestCtrl::fast");
    ADD_METHOD_TO(PromTestCtrl::slow, "/slow", "PromStat");
    METHOD_LIST_END

    void fast(const HttpRequestPtr &req,
              std::function<void(const HttpResponsePtr &)> &&callback);
    drogon::AsyncTask slow(
        const HttpRequestPtr req,
        std::function<void(const HttpResponsePtr &)> callback);
};
