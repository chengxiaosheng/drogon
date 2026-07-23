#include <drogon/drogon_test.h>
#include <drogon/HttpTypes.h>
#include <trantor/net/ParseCursor.h>
#include <trantor/utils/MsgBuffer.h>
#include <algorithm>
#include <string>
#include <vector>
#include "../../lib/src/HttpResponseImpl.h"
#include "../../lib/src/HttpResponseParser.h"

using namespace drogon;
using namespace trantor;

namespace
{
std::vector<std::string> fragment(const std::string &s, size_t fs)
{
    if (fs == 0 || s.size() <= fs)
        return {s};
    std::vector<std::string> out;
    for (size_t i = 0; i < s.size(); i += fs)
        out.push_back(s.substr(i, fs));
    return out;
}

struct R
{
    bool ok;
    bool gotAll;
    HttpResponseImplPtr resp;
    size_t consumed;
};

// Drive the response parser over `data` (optionally fragmented), accumulating
// unconsumed bytes in a MsgBuffer (the parser does not retain them itself).
R drive(const std::string &data, bool head = false, size_t fragSize = 0)
{
    TcpConnectionPtr nullConn;
    HttpResponseParser parser(nullConn);
    if (head)
        parser.setForHeadMethod();
    auto frags = fragment(data, fragSize);
    MsgBuffer accum(8192);
    size_t fi = 0;
    R r{true, false, nullptr, 0};
    for (;;)
    {
        if (accum.readableBytes() == 0)
        {
            if (fi < frags.size())
            {
                accum.append(frags[fi].data(), frags[fi].size());
                ++fi;
            }
            else
            {
                break;
            }
        }
        ParseCursor c(accum.peek(), accum.readableBytes());
        r.ok = parser.parseResponse(&c);
        r.consumed += c.consumed();
        accum.retrieve(c.consumed());
        r.resp = parser.responseImpl();
        if (!r.ok)
            break;
        if (parser.gotAll())
        {
            r.gotAll = true;
            break;
        }
        if (fi < frags.size())
        {
            accum.append(frags[fi].data(), frags[fi].size());
            ++fi;
        }
        else
        {
            break;  // incomplete
        }
    }
    r.gotAll = r.gotAll || parser.gotAll();
    return r;
}
}  // namespace

DROGON_TEST(ResponseParserContentLength)
{
    auto r = drive("HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\nhello");
    CHECK(r.ok);
    CHECK(r.gotAll);
    CHECK(r.resp->statusCode() == k200OK);
    CHECK(std::string(r.resp->body()) == "hello");
}

DROGON_TEST(ResponseParser204NoBody)
{
    auto r = drive("HTTP/1.1 204 No Content\r\n\r\n");
    CHECK(r.ok);
    CHECK(r.gotAll);
    CHECK(r.resp->statusCode() == k204NoContent);
    CHECK(r.resp->getBodyLength() == 0);
}

DROGON_TEST(ResponseParser304NoBody)
{
    auto r = drive("HTTP/1.1 304 Not Modified\r\nETag: \"x\"\r\n\r\n");
    CHECK(r.ok);
#ifdef DROGON_USE_LLHTTP
    CHECK(r.gotAll);  // llhttp: 304 has no body
#else
    CHECK(!r.gotAll);  // legacy: 304 treated as close-delimited
#endif
    CHECK(r.resp->statusCode() == k304NotModified);
    CHECK(r.resp->getBodyLength() == 0);
}

DROGON_TEST(ResponseParserChunked)
{
    auto r = drive("HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n"
                   "5\r\nhello\r\n6\r\n world\r\n0\r\n\r\n");
    CHECK(r.ok);
    CHECK(r.gotAll);
    CHECK(std::string(r.resp->body()) == "hello world");
    CHECK(r.resp->getHeader("content-length") == "11");
    CHECK(r.resp->getHeader("transfer-encoding").empty());
}

DROGON_TEST(ResponseParserHeadSkipsBody)
{
    auto r = drive("HTTP/1.1 200 OK\r\nContent-Length: 100\r\n\r\n", true);
    CHECK(r.ok);
    CHECK(r.gotAll);
    CHECK(r.resp->getBodyLength() == 0);
}

DROGON_TEST(ResponseParser101UpgradeHandoff)
{
    std::string ws = "\x81\x05hello";
    std::string msg = "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: x\r\n\r\n" + ws;
    auto r = drive(msg);
    CHECK(r.ok);
    CHECK(r.gotAll);
    CHECK(msg.size() - r.consumed == ws.size());
}

DROGON_TEST(ResponseParserCloseDelimited)
{
    std::string body(20, 'c');
    std::string msg = "HTTP/1.1 200 OK\r\nConnection: close\r\n\r\n" + body;
    TcpConnectionPtr nullConn;
    HttpResponseParser parser(nullConn);
    MsgBuffer accum(8192);
    accum.append(msg.data(), msg.size());
    ParseCursor c(accum.peek(), accum.readableBytes());
    bool ok = parser.parseResponse(&c);
    accum.retrieve(c.consumed());
    CHECK(ok);
    CHECK(!parser.gotAll());
    bool closed = parser.parseResponseOnClose();
    CHECK(closed);
    CHECK(parser.gotAll());
    CHECK(std::string(parser.responseImpl()->body()) == body);
}

DROGON_TEST(ResponseParserByteSplit)
{
    std::string body(40, 'd');
    std::string msg = "HTTP/1.1 200 OK\r\nContent-Length: 40\r\n\r\n" + body;
    auto whole = drive(msg);
    auto split2 = drive(msg, false, 2);
    auto split3 = drive(msg, false, 3);
    CHECK(whole.gotAll);
    CHECK(split2.gotAll);
    CHECK(split3.gotAll);
    CHECK(std::string(split2.resp->body()) == body);
    CHECK(std::string(split3.resp->body()) == body);
}