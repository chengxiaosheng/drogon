/**
 *
 *  HttpRequestParserCommon.cc
 *
 *  Non-parse members of HttpRequestParser shared by the legacy and the llhttp
 *  implementations (request-object pool + pipelining queue). Always compiled.
 *  Extracted behavior-neutral from the legacy parser (plan §4.2).
 *
 *  Copyright 2018, An Tao.  All rights reserved.
 *  https://github.com/an-tao/drogon
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Drogon
 *
 */

#include "HttpRequestParser.h"
#include "HttpRequestImpl.h"

using namespace trantor;
using namespace drogon;

HttpRequestImplPtr HttpRequestParser::makeRequestForPool(HttpRequestImpl *ptr)
{
    return std::shared_ptr<HttpRequestImpl>(
        ptr, [weakPtr = weak_from_this()](HttpRequestImpl *p) {
            auto thisPtr = weakPtr.lock();
            if (thisPtr && thisPtr->loop_)
            {
                if (thisPtr->loop_->isCurrentThread())
                {
                    p->reset();
                    thisPtr->requestsPool_.emplace_back(
                        thisPtr->makeRequestForPool(p));
                }
                else
                {
                    auto &loop = thisPtr->loop_;
                    loop->async([thisPtr = std::move(thisPtr), p]() {
                        p->reset();
                        thisPtr->requestsPool_.emplace_back(
                            thisPtr->makeRequestForPool(p));
                    }, false);
                }
            }
            else
            {
                delete p;
            }
        });
}

void HttpRequestParser::pushRequestToPipelining(const HttpRequestPtr &req,
                                                 bool isHeadMethod)
{
    assert(loop_->isCurrentThread());
    requestPipelining_.push_back({req, {nullptr, isHeadMethod}});
}

bool HttpRequestParser::pushResponseToPipelining(const HttpRequestPtr &req,
                                                  HttpResponsePtr resp)
{
    assert(loop_->isCurrentThread());
    for (size_t i = 0; i != requestPipelining_.size(); ++i)
    {
        if (requestPipelining_[i].first == req)
        {
            requestPipelining_[i].second.first = std::move(resp);
            return i == 0;
        }
    }
    assert(false);  // Should always find a match
    return false;
}

void HttpRequestParser::popReadyResponses(
    std::vector<std::pair<HttpResponsePtr, bool>> &buffer)
{
    while (!requestPipelining_.empty() &&
           requestPipelining_.front().second.first)
    {
        buffer.push_back(std::move(requestPipelining_.front().second));
        requestPipelining_.pop_front();
    }
}