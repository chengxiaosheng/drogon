/**
 *
 *  HttpMessageBody.h
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
#include <Network/Buffer.h>
#include <string_view>
#include <memory>
#include <string>

namespace drogon
{
class HttpMessageBody
{
  public:
    enum class BodyType
    {
        kNone = 0,
        kString,
        kStringView,
        kBuffer
    };

    BodyType bodyType()
    {
        return type_;
    }

    virtual const char *data() const
    {
        return nullptr;
    }

    virtual char *data()
    {
        return nullptr;
    }

    virtual size_t length() const
    {
        return 0;
    }

    virtual std::string_view getString() const = 0;

    virtual void append(const char * /*buf*/, size_t /*len*/)
    {
    }

    virtual ~HttpMessageBody()
    {
    }

  protected:
    BodyType type_{BodyType::kNone};
};

class HttpMessageStringBody : public HttpMessageBody
{
  public:
    HttpMessageStringBody()
    {
        type_ = BodyType::kString;
    }

    HttpMessageStringBody(const std::string &body) : body_(body)
    {
        type_ = BodyType::kString;
    }

    HttpMessageStringBody(std::string &&body) : body_(std::move(body))
    {
        type_ = BodyType::kString;
    }

    const char *data() const override
    {
        return body_.data();
    }

    char *data() override
    {
        return const_cast<char *>(body_.data());
    }

    size_t length() const override
    {
        return body_.length();
    }

    std::string_view getString() const override
    {
        return std::string_view{body_.data(), body_.length()};
    }

    void append(const char *buf, size_t len) override
    {
        body_.append(buf, len);
    }

  private:
    std::string body_;
};

class HttpMessageStringViewBody : public HttpMessageBody
{
  public:
    HttpMessageStringViewBody(const char *buf, size_t len) : body_(buf, len)
    {
        type_ = BodyType::kStringView;
    }

    const char *data() const override
    {
        return body_.data();
    }

    char *data() override
    {
        return const_cast<char *>(body_.data());
    }

    size_t length() const override
    {
        return body_.length();
    }

    std::string_view getString() const override
    {
        return body_;
    }

  private:
    std::string_view body_;
};

/// 持有 toolkit::Buffer 的 body 后端：用于响应体由 Buffer 承载时的零拷贝发送。
/// 由 HttpResponseImpl::setBody(shared_ptr<Buffer>) 设定；renderToBuffer 对
/// Buffer body 仅渲染头部，body 由发送侧单独 send(shared_ptr<Buffer>) 零拷贝发出。
class HttpMessageBufferBody : public HttpMessageBody
{
  public:
    HttpMessageBufferBody()
    {
        type_ = BodyType::kBuffer;
    }
    explicit HttpMessageBufferBody(std::shared_ptr<toolkit::Buffer> buffer)
        : buffer_(std::move(buffer))
    {
        type_ = BodyType::kBuffer;
    }

    const char *data() const override
    {
        return buffer_ ? buffer_->data() : nullptr;
    }
    char *data() override
    {
        return buffer_ ? buffer_->data() : nullptr;
    }
    size_t length() const override
    {
        return buffer_ ? buffer_->size() : 0;
    }
    std::string_view getString() const override
    {
        return buffer_ ? std::string_view{buffer_->data(), buffer_->size()}
                       : std::string_view{};
    }
    void append(const char * /*buf*/, size_t /*len*/) override
    {
        // Buffer body 由 setBody 一次性设定，不支持增量 append
    }

    std::shared_ptr<toolkit::Buffer> buffer() const
    {
        return buffer_;
    }

  private:
    std::shared_ptr<toolkit::Buffer> buffer_;
};

}  // namespace drogon
