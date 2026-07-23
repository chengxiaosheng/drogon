/**
 *
 *  HttpRequestParser.h
 *  An Tao
 *
 *  Copyright 2018, An Tao.  All rights reserved.
 *  https://github.com/an-tao/drogon
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Drogon
 *
 */

#pragma once

#include <drogon/HttpTypes.h>
#include <trantor/net/TcpConnection.h>
#include <trantor/utils/MsgBuffer.h>
#include <Util/util.h>
#include <deque>
#include <memory>
#include <mutex>
#include "impl_forwards.h"
#ifdef DROGON_USE_LLHTTP
#include <llhttp.h>
#include <string>
#endif

namespace drogon
{
class HttpRequestParser : public toolkit::noncopyable,
                          public std::enable_shared_from_this<HttpRequestParser>
{
  public:
    enum class HttpRequestParseStatus
    {
        kExpectMethod,
        kExpectRequestLine,
        kExpectHeaders,
        kExpectBody,
        kExpectChunkLen,
        kExpectChunkBody,
        kExpectLastEmptyChunk,
        kGotAll,
    };

    explicit HttpRequestParser(const trantor::TcpConnectionPtr &connPtr);

    int parseRequest(trantor::ParseCursor *buf);

    bool gotAll() const
    {
        return status_ == HttpRequestParseStatus::kGotAll;
    }

    void reset();

    const HttpRequestImplPtr &requestImpl() const
    {
        return request_;
    }

    bool firstReq()
    {
        if (firstRequest_)
        {
            firstRequest_ = false;
            return true;
        }
        return false;
    }

    const WebSocketConnectionImplPtr &webSocketConn() const
    {
        return websockConnPtr_;
    }

    void setWebsockConnection(const WebSocketConnectionImplPtr &conn)
    {
        websockConnPtr_ = conn;
    }

    // to support request pipelining(rfc2616-8.1.2.2)
    void pushRequestToPipelining(const HttpRequestPtr &, bool isHeadMethod);
    bool pushResponseToPipelining(const HttpRequestPtr &, HttpResponsePtr);
    void popReadyResponses(std::vector<std::pair<HttpResponsePtr, bool>> &);

    size_t numberOfRequestsInPipelining() const
    {
        return requestPipelining_.size();
    }

    bool emptyPipelining()
    {
        return requestPipelining_.empty();
    }

    bool isStop() const
    {
        return stopWorking_;
    }

    void stop()
    {
        stopWorking_ = true;
    }

    size_t numberOfRequestsParsed() const
    {
        return requestsCounter_;
    }

    trantor::MsgBuffer &getBuffer()
    {
        return sendBuffer_;
    }

    std::vector<std::pair<HttpResponsePtr, bool>> &getResponseBuffer()
    {
        assert(loop_->isCurrentThread());
        if (!responseBuffer_)
        {
            responseBuffer_ =
                std::unique_ptr<std::vector<std::pair<HttpResponsePtr, bool>>>(
                    new std::vector<std::pair<HttpResponsePtr, bool>>);
        }
        return *responseBuffer_;
    }

    std::vector<HttpRequestImplPtr> &getRequestBuffer()
    {
        assert(loop_->isCurrentThread());
        if (!requestBuffer_)
        {
            requestBuffer_ = std::unique_ptr<std::vector<HttpRequestImplPtr>>(
                new std::vector<HttpRequestImplPtr>);
        }
        return *requestBuffer_;
    }

  private:
    HttpRequestImplPtr makeRequestForPool(HttpRequestImpl *p);
    bool processRequestLine(const char *begin, const char *end);
    HttpRequestParseStatus status_;
    std::shared_ptr<toolkit::EventPoller> loop_;
    HttpRequestImplPtr request_;
    bool firstRequest_{true};
    WebSocketConnectionImplPtr websockConnPtr_;
    std::deque<std::pair<HttpRequestPtr, std::pair<HttpResponsePtr, bool>>>
        requestPipelining_;
    size_t requestsCounter_{0};
    std::weak_ptr<trantor::TcpConnection> conn_;
    bool stopWorking_{false};
    trantor::MsgBuffer sendBuffer_;
    std::unique_ptr<std::vector<std::pair<HttpResponsePtr, bool>>>
        responseBuffer_;
    std::unique_ptr<std::vector<HttpRequestImplPtr>> requestBuffer_;
    std::vector<HttpRequestImplPtr> requestsPool_;
    size_t currentChunkLength_{0};
    size_t remainContentLength_{0};
#ifdef DROGON_USE_LLHTTP
    // --- llhttp adapter (compiled only when DROGON_USE_LLHTTP is ON) ---
    static llhttp_settings_t makeSettings();
    static int sOnUrl(llhttp_t *, const char *, size_t);
    static int sOnHeaderField(llhttp_t *, const char *, size_t);
    static int sOnHeaderValue(llhttp_t *, const char *, size_t);
    static int sOnHeaderValueComplete(llhttp_t *);
    static int sOnHeadersComplete(llhttp_t *);
    static int sOnBody(llhttp_t *, const char *, size_t);
    static int sOnMessageComplete(llhttp_t *);
    int onUrl(const char *at, size_t len);
    int onHeaderField(const char *at, size_t len);
    int onHeaderValue(const char *at, size_t len);
    int onHeaderValueComplete();
    int onHeadersComplete();
    int onBody(const char *at, size_t len);
    int onMessageComplete();
    void processUrl();
    void initLlhttpState();
    llhttp_t llhttpParser_;
    llhttp_settings_t settings_;
    std::string urlScratch_;
    std::string headerLine_;
    size_t headerColonPos_{0};
    bool headerValueStarted_{false};
    int adapterError_{0};
    bool headersPaused_{false};
    bool gotMessage_{false};
    bool messageComplete_{false};
    bool streamMode_{false};
    bool syntheticChunkedCL_{false};
    size_t urlSizeAccum_{0};
    size_t headerSizeAccum_{0};
    size_t bodyBytesAccum_{0};
#endif
};

}  // namespace drogon
