/**
 *
 *  HttpResponseParserLlhttp.cc
 *
 *  llhttp-backed implementation of HttpResponseParser (compiled only when
 *  DROGON_USE_LLHTTP is ON). Same class name and public API as the legacy
 *  parser; HttpClientImpl.cc / WebSocketClientImpl.cc are unchanged.
 *  parseResponse() returns bool and uses gotAll(); message boundary is
 *  reproduced via llhttp pause (plan §4.3 / §4.4.8).
 *
 *  Copyright 2018, An Tao.  All rights reserved.
 *  https://github.com/an-tao/drogon
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Drogon
 *
 */

#include "HttpResponseParser.h"
#include <llhttp.h>
#include <drogon/HttpTypes.h>
#include <trantor/utils/MsgBuffer.h>
#include <string>
#include "HttpResponseImpl.h"

using namespace trantor;
using namespace drogon;

int HttpResponseParser::sOnHeaderField(llhttp_t *p, const char *at, size_t len)
{
    return static_cast<HttpResponseParser *>(p->data)->onHeaderField(at, len);
}
int HttpResponseParser::sOnHeaderValue(llhttp_t *p, const char *at, size_t len)
{
    return static_cast<HttpResponseParser *>(p->data)->onHeaderValue(at, len);
}
int HttpResponseParser::sOnHeaderValueComplete(llhttp_t *p)
{
    return static_cast<HttpResponseParser *>(p->data)->onHeaderValueComplete();
}
int HttpResponseParser::sOnHeadersComplete(llhttp_t *p)
{
    return static_cast<HttpResponseParser *>(p->data)->onHeadersComplete();
}
int HttpResponseParser::sOnBody(llhttp_t *p, const char *at, size_t len)
{
    return static_cast<HttpResponseParser *>(p->data)->onBody(at, len);
}
int HttpResponseParser::sOnMessageComplete(llhttp_t *p)
{
    return static_cast<HttpResponseParser *>(p->data)->onMessageComplete();
}

llhttp_settings_t HttpResponseParser::makeSettings()
{
    llhttp_settings_t s;
    llhttp_settings_init(&s);
    s.on_header_field = &HttpResponseParser::sOnHeaderField;
    s.on_header_value = &HttpResponseParser::sOnHeaderValue;
    s.on_header_value_complete = &HttpResponseParser::sOnHeaderValueComplete;
    s.on_headers_complete = &HttpResponseParser::sOnHeadersComplete;
    s.on_body = &HttpResponseParser::sOnBody;
    s.on_message_complete = &HttpResponseParser::sOnMessageComplete;
    return s;
}

HttpResponseParser::HttpResponseParser(const TcpConnectionPtr &connPtr)
    : status_(HttpResponseParseStatus::kExpectResponseLine),
      responsePtr_(new HttpResponseImpl),
      conn_(connPtr)
{
    settings_ = makeSettings();
    initLlhttpState();
}

void HttpResponseParser::initLlhttpState()
{
    llhttp_init(&llhttpParser_, HTTP_RESPONSE, &settings_);
    llhttpParser_.data = this;
#ifdef DROGON_LLHTTP_LENIENT
    llhttp_set_lenient_optional_cr_before_lf(&llhttpParser_, 1);
    llhttp_set_lenient_optional_lf_after_cr(&llhttpParser_, 1);
    llhttp_set_lenient_headers(&llhttpParser_, 1);
    llhttp_set_lenient_header_value_relaxed(&llhttpParser_, 1);
    llhttp_set_lenient_optional_crlf_after_chunk(&llhttpParser_, 1);
    llhttp_set_lenient_spaces_after_chunk_size(&llhttpParser_, 1);
    llhttp_set_lenient_version(&llhttpParser_, 1);
#else
    llhttp_set_lenient_optional_cr_before_lf(&llhttpParser_, 0);
    llhttp_set_lenient_optional_lf_after_cr(&llhttpParser_, 0);
    llhttp_set_lenient_headers(&llhttpParser_, 0);
    llhttp_set_lenient_header_value_relaxed(&llhttpParser_, 0);
    llhttp_set_lenient_optional_crlf_after_chunk(&llhttpParser_, 0);
    llhttp_set_lenient_spaces_after_chunk_size(&llhttpParser_, 0);
    llhttp_set_lenient_version(&llhttpParser_, 0);
#endif
    headerLine_.clear();
    headerColonPos_ = 0;
    headerValueStarted_ = false;
    expectClose_ = false;
    syntheticCL_ = false;
}

void HttpResponseParser::reset()
{
    status_ = HttpResponseParseStatus::kExpectResponseLine;
    responsePtr_.reset(new HttpResponseImpl);
    parseResponseForHeadMethod_ = false;
    leftBodyLength_ = 0;
    currentChunkLength_ = 0;
    initLlhttpState();
}

int HttpResponseParser::onHeaderField(const char *at, size_t len)
{
    headerLine_.append(at, len);
    return 0;
}

int HttpResponseParser::onHeaderValue(const char *at, size_t len)
{
    if (!headerValueStarted_)
    {
        headerColonPos_ = headerLine_.size();
        headerLine_.push_back(':');
        headerValueStarted_ = true;
    }
    headerLine_.append(at, len);
    return 0;
}

int HttpResponseParser::onHeaderValueComplete()
{
    if (!headerLine_.empty())
    {
        responsePtr_->addHeader(headerLine_.data(),
                                headerLine_.data() + headerColonPos_,
                                headerLine_.data() + headerLine_.size());
    }
    headerLine_.clear();
    headerValueStarted_ = false;
    return 0;
}

int HttpResponseParser::onHeadersComplete()
{
    if (llhttpParser_.http_major == 1)
    {
        responsePtr_->setVersion(llhttpParser_.http_minor == 1
                                      ? Version::kHttp11
                                      : Version::kHttp10);
    }
    else
    {
        responsePtr_->setVersion(Version::kHttp10);
    }
    responsePtr_->setStatusCode(static_cast<HttpStatusCode>(
        llhttp_get_status_code(&llhttpParser_)));

    // HEAD response: no body (mirror legacy setForHeadMethod_).
    if (parseResponseForHeadMethod_)
        return 1;
    // 101 upgrade: pause as upgrade; remaining bytes handed to the websocket
    // layer (plan §4.4.8).
    if (responsePtr_->statusCode() == k101SwitchingProtocols)
        return 2;

    const std::string &cl = responsePtr_->getHeaderBy("content-length");
    const std::string &te = responsePtr_->getHeaderBy("transfer-encoding");
    if (te == "chunked")
    {
        syntheticCL_ = true;
    }
    else if (cl.empty())
    {
        // No CL / no chunked: read until connection close, except for the
        // no-body status codes (llhttp fires message_complete for those).
        int code = responsePtr_->statusCode();
        if (code != k204NoContent && code != k304NotModified &&
            code / 100 != 1)
        {
            expectClose_ = true;
            auto connPtr = conn_.lock();
            if (connPtr)
                connPtr->shutdown();  // retained legacy behavior (§4.4.8)
        }
    }
    return 0;
}

int HttpResponseParser::onBody(const char *at, size_t len)
{
    if (!responsePtr_->bodyPtr_)
        responsePtr_->bodyPtr_ = std::make_shared<HttpMessageStringBody>();
    responsePtr_->bodyPtr_->append(at, len);
    return 0;
}

int HttpResponseParser::onMessageComplete()
{
    if (syntheticCL_)
    {
        size_t blen =
            responsePtr_->bodyPtr_ ? responsePtr_->bodyPtr_->length() : 0;
        responsePtr_->addHeader("content-length", std::to_string(blen));
        responsePtr_->removeHeaderBy("transfer-encoding");
    }
    status_ = HttpResponseParseStatus::kGotAll;
    llhttp_pause(&llhttpParser_);
    return 0;
}

bool HttpResponseParser::parseResponse(ParseCursor *buf)
{
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
        return true;  // need more / body in progress
    if (err == HPE_PAUSED)
        return true;  // message complete (status_ set in on_message_complete)
    if (err == HPE_PAUSED_UPGRADE)
    {
        status_ = HttpResponseParseStatus::kGotAll;  // 101 upgrade
        return true;
    }
    return false;
}

bool HttpResponseParser::parseResponseOnClose()
{
    if (expectClose_ && status_ != HttpResponseParseStatus::kGotAll)
    {
        // EOF on a close-delimited response: complete it.
        llhttp_finish(&llhttpParser_);
        return status_ == HttpResponseParseStatus::kGotAll;
    }
    return false;
}