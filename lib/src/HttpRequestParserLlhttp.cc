/**
 *
 *  HttpRequestParserLlhttp.cc
 *
 *  llhttp-backed implementation of HttpRequestParser (compiled only when
 *  DROGON_USE_LLHTTP is ON). Same class name and public API as the legacy
 *  parser; HttpServer.cc is unchanged. Reproduces the legacy 1/2/3/0/<0
 *  return contract via llhttp pause/resume (plan §4.3).
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
#include <llhttp.h>
#include <drogon/HttpTypes.h>
#include <Util/logger.h>
#include <trantor/utils/MsgBuffer.h>
#include <algorithm>
#include <string>
#include "HttpAppFrameworkImpl.h"
#include "HttpRequestImpl.h"
#include "HttpResponseImpl.h"
#include "HttpUtils.h"

using namespace trantor;
using namespace drogon;

namespace
{
constexpr size_t kMaxRequestLine = 64 * 1024;
constexpr size_t kMaxHeaderLine = 64 * 1024;

bool mapLlhttpMethod(llhttp_method_t m, HttpMethod &out)
{
    switch (m)
    {
        case HTTP_GET:
            out = drogon::Get;
            return true;
        case HTTP_POST:
            out = drogon::Post;
            return true;
        case HTTP_HEAD:
            out = drogon::Head;
            return true;
        case HTTP_PUT:
            out = drogon::Put;
            return true;
        case HTTP_DELETE:
            out = drogon::Delete;
            return true;
        case HTTP_OPTIONS:
            out = drogon::Options;
            return true;
        case HTTP_PATCH:
            out = drogon::Patch;
            return true;
        case HTTP_PROPFIND:
            out = drogon::Propfind;
            return true;
        case HTTP_MKCOL:
            out = drogon::Mkcol;
            return true;
        case HTTP_COPY:
            out = drogon::Copy;
            return true;
        case HTTP_MOVE:
            out = drogon::Move;
            return true;
        default:
            return false;
    }
}

int mapLlhttpError(llhttp_errno_t err)
{
    // §4.4.9 error code map
    switch (err)
    {
        case HPE_INVALID_METHOD:
            return -k405MethodNotAllowed;
        case HPE_INVALID_TRANSFER_ENCODING:
            return -k501NotImplemented;
        case HPE_INVALID_VERSION:
        case HPE_INVALID_URL:
        case HPE_INVALID_CONSTANT:
        case HPE_INVALID_HEADER_TOKEN:
        case HPE_INVALID_CONTENT_LENGTH:
        case HPE_INVALID_CHUNK_SIZE:
        case HPE_INVALID_EOF_STATE:
        case HPE_CLOSED_CONNECTION:
        default:
            return -k400BadRequest;
    }
}
}  // namespace

int HttpRequestParser::sOnUrl(llhttp_t *p, const char *at, size_t len)
{
    return static_cast<HttpRequestParser *>(p->data)->onUrl(at, len);
}
int HttpRequestParser::sOnHeaderField(llhttp_t *p, const char *at, size_t len)
{
    return static_cast<HttpRequestParser *>(p->data)->onHeaderField(at, len);
}
int HttpRequestParser::sOnHeaderValue(llhttp_t *p, const char *at, size_t len)
{
    return static_cast<HttpRequestParser *>(p->data)->onHeaderValue(at, len);
}
int HttpRequestParser::sOnHeaderValueComplete(llhttp_t *p)
{
    return static_cast<HttpRequestParser *>(p->data)->onHeaderValueComplete();
}
int HttpRequestParser::sOnHeadersComplete(llhttp_t *p)
{
    return static_cast<HttpRequestParser *>(p->data)->onHeadersComplete();
}
int HttpRequestParser::sOnBody(llhttp_t *p, const char *at, size_t len)
{
    return static_cast<HttpRequestParser *>(p->data)->onBody(at, len);
}
int HttpRequestParser::sOnMessageComplete(llhttp_t *p)
{
    return static_cast<HttpRequestParser *>(p->data)->onMessageComplete();
}

llhttp_settings_t HttpRequestParser::makeSettings()
{
    llhttp_settings_t s;
    llhttp_settings_init(&s);
    s.on_url = &HttpRequestParser::sOnUrl;
    s.on_header_field = &HttpRequestParser::sOnHeaderField;
    s.on_header_value = &HttpRequestParser::sOnHeaderValue;
    s.on_header_value_complete = &HttpRequestParser::sOnHeaderValueComplete;
    s.on_headers_complete = &HttpRequestParser::sOnHeadersComplete;
    s.on_body = &HttpRequestParser::sOnBody;
    s.on_message_complete = &HttpRequestParser::sOnMessageComplete;
    return s;
}

HttpRequestParser::HttpRequestParser(const TcpConnectionPtr &connPtr)
    : status_(HttpRequestParseStatus::kExpectMethod),
      loop_(connPtr ? connPtr->getLoop() : nullptr),
      conn_(connPtr)
{
    settings_ = makeSettings();
    initLlhttpState();
}

void HttpRequestParser::initLlhttpState()
{
    llhttp_init(&llhttpParser_, HTTP_REQUEST, &settings_);
    llhttpParser_.data = this;
#ifdef DROGON_LLHTTP_LENIENT
    llhttp_set_lenient_optional_cr_before_lf(&llhttpParser_, 1);
    llhttp_set_lenient_optional_lf_after_cr(&llhttpParser_, 1);
    llhttp_set_lenient_headers(&llhttpParser_, 1);
    llhttp_set_lenient_header_value_relaxed(&llhttpParser_, 1);
    llhttp_set_lenient_spaces_after_chunk_size(&llhttpParser_, 1);
    llhttp_set_lenient_optional_crlf_after_chunk(&llhttpParser_, 1);
    llhttp_set_lenient_version(&llhttpParser_, 1);
#else
    llhttp_set_lenient_optional_cr_before_lf(&llhttpParser_, 0);
    llhttp_set_lenient_optional_lf_after_cr(&llhttpParser_, 0);
    llhttp_set_lenient_headers(&llhttpParser_, 0);
    llhttp_set_lenient_header_value_relaxed(&llhttpParser_, 0);
    llhttp_set_lenient_spaces_after_chunk_size(&llhttpParser_, 0);
    llhttp_set_lenient_optional_crlf_after_chunk(&llhttpParser_, 0);
    llhttp_set_lenient_version(&llhttpParser_, 0);
#endif
    urlScratch_.clear();
    headerLine_.clear();
    headerColonPos_ = 0;
    headerValueStarted_ = false;
    adapterError_ = 0;
    headersPaused_ = false;
    gotMessage_ = false;
    messageComplete_ = false;
    streamMode_ = false;
    syntheticChunkedCL_ = false;
    urlSizeAccum_ = 0;
    headerSizeAccum_ = 0;
    bodyBytesAccum_ = 0;
}

void HttpRequestParser::reset()
{
    if (loop_)
        assert(loop_->isCurrentThread());
    remainContentLength_ = 0;
    status_ = HttpRequestParseStatus::kExpectMethod;
    if (requestsPool_.empty())
    {
        request_ = makeRequestForPool(new HttpRequestImpl(loop_));
    }
    else
    {
        auto req = std::move(requestsPool_.back());
        requestsPool_.pop_back();
        request_ = std::move(req);
        request_->setCreationDate(trantor::Date::now());
    }
    initLlhttpState();
}

void HttpRequestParser::processUrl()
{
    const std::string &url = urlScratch_;
    if (url.empty())
    {
        request_->setPath("/");
        return;
    }
    size_t slash = url.find('/');
    if (slash != 0 && slash != std::string::npos && slash + 2 < url.size() &&
        url[slash + 1] == '/')
    {
        slash = url.find('/', slash + 2);
    }
    size_t qpos = (slash != std::string::npos) ? url.find('?', slash)
                                               : url.find('?');
    if (slash != std::string::npos && slash < url.size())
    {
        const char *pathEnd = (qpos != std::string::npos)
                                  ? (url.data() + qpos)
                                  : (url.data() + url.size());
        request_->setPath(url.data() + slash, pathEnd);
    }
    else
    {
        request_->setPath("/");
    }
    if (qpos != std::string::npos && qpos + 1 < url.size())
    {
        request_->setQuery(url.data() + qpos + 1, url.data() + url.size());
    }
}

int HttpRequestParser::onUrl(const char *at, size_t len)
{
    urlSizeAccum_ += len;
    if (urlSizeAccum_ > kMaxRequestLine)
    {
        adapterError_ = -k414RequestURITooLarge;
        return -1;
    }
    urlScratch_.append(at, len);
    return 0;
}

int HttpRequestParser::onHeaderField(const char *at, size_t len)
{
    headerSizeAccum_ += len;
    if (headerSizeAccum_ > kMaxHeaderLine)
    {
        adapterError_ = -k400BadRequest;
        return -1;
    }
    headerLine_.append(at, len);
    return 0;
}

int HttpRequestParser::onHeaderValue(const char *at, size_t len)
{
    if (!headerValueStarted_)
    {
        headerColonPos_ = headerLine_.size();
        headerLine_.push_back(':');
        headerValueStarted_ = true;
    }
    headerSizeAccum_ += len;
    if (headerSizeAccum_ > kMaxHeaderLine)
    {
        adapterError_ = -k400BadRequest;
        return -1;
    }
    headerLine_.append(at, len);
    return 0;
}

int HttpRequestParser::onHeaderValueComplete()
{
    if (!headerLine_.empty())
    {
        request_->addHeader(headerLine_.data(),
                            headerLine_.data() + headerColonPos_,
                            headerLine_.data() + headerLine_.size());
    }
    headerLine_.clear();
    headerValueStarted_ = false;
    headerSizeAccum_ = 0;
    return 0;
}

int HttpRequestParser::onHeadersComplete()
{
    if (llhttpParser_.http_major == 1)
    {
        request_->setVersion(llhttpParser_.http_minor == 1
                                  ? Version::kHttp11
                                  : Version::kHttp10);
    }
    else if (llhttpParser_.http_major == 0)
    {
        request_->setVersion(Version::kHttp10);
    }
    else
    {
        adapterError_ = -k400BadRequest;
        return -1;
    }
    HttpMethod m;
    if (!mapLlhttpMethod(
            static_cast<llhttp_method_t>(llhttp_get_method(&llhttpParser_)), m))
    {
        adapterError_ = -k405MethodNotAllowed;
        return -1;
    }
    request_->setMethod(m);
    processUrl();

    const std::string &cl = request_->getHeaderBy("content-length");
    bool hasBody = false;
    bool chunked = false;
    size_t contentLen = 0;
    if (!cl.empty())
    {
        try
        {
            contentLen = static_cast<size_t>(std::stoull(cl));
        }
        catch (...)
        {
            adapterError_ = -k400BadRequest;
            return -1;
        }
        request_->contentLengthHeaderValue_ = contentLen;
        if (contentLen > 0)
            hasBody = true;
    }
    else
    {
        const std::string &te = request_->getHeaderBy("transfer-encoding");
        if (te == "chunked")
        {
            hasBody = true;
            chunked = true;
        }
        else if (!te.empty())
        {
            adapterError_ = -k501NotImplemented;
            return -1;
        }
    }
    if (contentLen >
        HttpAppFrameworkImpl::instance().getClientMaxBodySize())
    {
        adapterError_ = -k413RequestEntityTooLarge;
        return -1;
    }
    const std::string &expect = request_->expect();
    if (expect == "100-continue" &&
        request_->getVersion() >= Version::kHttp11)
    {
        if (!hasBody)
        {
            adapterError_ = -k400BadRequest;
            return -1;
        }
        auto connPtr = conn_.lock();
        if (!connPtr)
        {
            adapterError_ = -1;
            return -1;
        }
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k100Continue);
        auto httpString = static_cast<HttpResponseImpl *>(resp.get())
                              ->renderToBuffer();
        connPtr->send(std::move(*httpString));
    }
    else if (!expect.empty())
    {
        WarnL << "417ExpectationFailed for \"" << expect << "\"";
        adapterError_ = -k417ExpectationFailed;
        return -1;
    }
    if (chunked)
        syntheticChunkedCL_ = true;

    if (app().isRequestStreamEnabled())
    {
        streamMode_ = true;
        request_->streamStart();
        if (!hasBody)
        {
            gotMessage_ = true;  // stream complete, no body -> return 2
            return HPE_PAUSED;
        }
        headersPaused_ = true;  // -> return 3, resume on next call
        return HPE_PAUSED;
    }
    if (contentLen > 0)
        request_->reserveBodySize(contentLen);
    return 0;  // proceed to body / message_complete
}

int HttpRequestParser::onBody(const char *at, size_t len)
{
    bodyBytesAccum_ += len;
    if (bodyBytesAccum_ >
        HttpAppFrameworkImpl::instance().getClientMaxBodySize())
    {
        adapterError_ = -k413RequestEntityTooLarge;
        return -1;
    }
    request_->appendToBody(at, len);
    return 0;
}

int HttpRequestParser::onMessageComplete()
{
    gotMessage_ = true;
    messageComplete_ = true;
    return HPE_PAUSED;
}

int HttpRequestParser::parseRequest(ParseCursor *buf)
{
    if (headersPaused_)
    {
        llhttp_resume(&llhttpParser_);
        headersPaused_ = false;
    }
    gotMessage_ = false;
    const char *data = buf->peek();
    size_t len = buf->readableBytes();
    llhttp_errno_t err = llhttp_execute(&llhttpParser_, data, len);

    size_t consumed = len;
    if (err != HPE_OK)
    {
        const char *epos = llhttp_get_error_pos(&llhttpParser_);
        if (epos >= data && epos <= data + len)
            consumed = static_cast<size_t>(epos - data);
        else
            consumed = len;
    }
    buf->retrieve(consumed);

    if (err == HPE_OK)
        return 0;  // need more data
    if (err == HPE_PAUSED)
    {
        if (gotMessage_)
        {
            ++requestsCounter_;
            status_ = HttpRequestParseStatus::kGotAll;
            if (syntheticChunkedCL_ && !streamMode_)
            {
                request_->addHeader(
                    "content-length",
                    std::to_string(request_->realContentLength()));
                request_->removeHeaderBy("transfer-encoding");
            }
            return messageComplete_ ? 1 : 2;
        }
        return 3;  // paused at headers (stream mode)
    }
    if (adapterError_ != 0)
        return adapterError_;
    return mapLlhttpError(err);
}