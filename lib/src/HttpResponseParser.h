/**
 *
 *  @file HttpResponseParser.h
 *  @author An Tao
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

#include "impl_forwards.h"
#ifdef DROGON_USE_LLHTTP
#include <llhttp.h>
#include <string>
#endif
#include <Util/util.h>
#include <trantor/net/TcpConnection.h>
#include <trantor/utils/MsgBuffer.h>
#include <list>
#include <mutex>

namespace drogon
{
class HttpResponseParser : public toolkit::noncopyable
{
  public:
    enum class HttpResponseParseStatus
    {
        kExpectResponseLine,
        kExpectHeaders,
        kExpectBody,
        kExpectChunkLen,
        kExpectChunkBody,
        kExpectLastEmptyChunk,
        kExpectClose,
        kGotAll,
    };

    explicit HttpResponseParser(const trantor::TcpConnectionPtr &connPtr);

    // default copy-ctor, dtor and assignment are fine

    // return false if any error
    bool parseResponse(trantor::ParseCursor *buf);
    bool parseResponseOnClose();

    bool gotAll() const
    {
        return status_ == HttpResponseParseStatus::kGotAll;
    }

    void setForHeadMethod()
    {
        parseResponseForHeadMethod_ = true;
    }

    void reset();

    const HttpResponseImplPtr &responseImpl() const
    {
        return responsePtr_;
    }

  private:
    bool processResponseLine(const char *begin, const char *end);

    HttpResponseParseStatus status_;
    HttpResponseImplPtr responsePtr_;
    bool parseResponseForHeadMethod_{false};
    size_t leftBodyLength_{0};
    size_t currentChunkLength_{0};
    std::weak_ptr<trantor::TcpConnection> conn_;
#ifdef DROGON_USE_LLHTTP
    static llhttp_settings_t makeSettings();
    static int sOnHeaderField(llhttp_t *, const char *, size_t);
    static int sOnHeaderValue(llhttp_t *, const char *, size_t);
    static int sOnHeaderValueComplete(llhttp_t *);
    static int sOnHeadersComplete(llhttp_t *);
    static int sOnBody(llhttp_t *, const char *, size_t);
    static int sOnMessageComplete(llhttp_t *);
    int onHeaderField(const char *at, size_t len);
    int onHeaderValue(const char *at, size_t len);
    int onHeaderValueComplete();
    int onHeadersComplete();
    int onBody(const char *at, size_t len);
    int onMessageComplete();
    void initLlhttpState();
    llhttp_t llhttpParser_{};
    llhttp_settings_t settings_{};
    std::string headerLine_;
    size_t headerColonPos_{0};
    bool headerValueStarted_{false};
    bool expectClose_{false};
    bool syntheticCL_{false};
#endif
};

}  // namespace drogon
