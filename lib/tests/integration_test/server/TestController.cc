#include "TestController.h"
using namespace example;

void TestController::asyncHandleHttpRequest(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback)
{
    // write your application logic here
    counter_->increment();
    WarnL << req->matchedPathPatternData();
    DebugL << "index=" << threadIndex_.getThreadData();
    ++(threadIndex_.getThreadData());
    auto resp = HttpResponse::newHttpResponse();
    resp->setContentTypeCodeAndCustomString(CT_TEXT_PLAIN,
                                            "content-type: plaintext\r\n");
    resp->setBody("<p>Hello, world!</p>");
    resp->setExpiredTime(20);
    callback(resp);
}
