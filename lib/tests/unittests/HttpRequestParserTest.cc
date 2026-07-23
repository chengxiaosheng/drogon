#include <drogon/drogon_test.h>
#include <drogon/HttpAppFramework.h>
#include <drogon/HttpTypes.h>
#include <trantor/net/ParseCursor.h>
#include <trantor/utils/MsgBuffer.h>
#include <string>
#include <vector>
#include "../../lib/src/HttpRequestImpl.h"
#include "../../lib/src/HttpRequestParser.h"

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

// Drive the parser over `data` (optionally fragmented); returns the sequence of
// parseRequest return codes. The last completed request is returned via `req`.
std::vector<int> drive(const std::string &data,
                       size_t fragSize,
                       HttpRequestImplPtr *req = nullptr)
{
    TcpConnectionPtr nullConn;
    HttpRequestParser parser(nullConn);
    parser.reset();
    MsgBuffer accum(8192);
    auto frags = fragment(data, fragSize);
    size_t fi = 0;
    std::vector<int> codes;
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
        int res = parser.parseRequest(&c);
        accum.retrieve(c.consumed());
        codes.push_back(res);
        if (res == 1 || res == 2)
        {
            if (req)
                *req = parser.requestImpl();
            parser.reset();
            continue;
        }
        if (res == 3)
            continue;  // stream headers complete; keep parsing the body
        if (res == 0)
        {
            if (fi < frags.size())
            {
                accum.append(frags[fi].data(), frags[fi].size());
                ++fi;
                continue;
            }
            break;
        }
        break;  // <0 error
    }
    return codes;
}
}  // namespace

DROGON_TEST(RequestParserBasicGet)
{
    HttpRequestImplPtr req;
    auto codes = drive("GET / HTTP/1.1\r\nHost: example.com\r\n\r\n", 0, &req);
    CHECK(codes.size() == 1);
    CHECK(codes[0] == 1);
    CHECK(req->method() == Get);
    CHECK(req->path() == "/");
    CHECK(req->getVersion() == Version::kHttp11);
}

DROGON_TEST(RequestParserAbsoluteForm)
{
    HttpRequestImplPtr req;
    auto codes = drive(
        "GET http://host/a/b?x=1&y=2 HTTP/1.1\r\nHost: host\r\n\r\n", 0, &req);
    CHECK(codes[0] == 1);
    CHECK(req->path() == "/a/b");
    CHECK(req->query() == "x=1&y=2");
}

DROGON_TEST(RequestParserPostContentLength)
{
    HttpRequestImplPtr req;
    std::string body(100, 'z');
    std::string msg = "POST /u HTTP/1.1\r\nHost: h\r\nContent-Length: 100\r\n\r\n" + body;
    auto codes = drive(msg, 0, &req);
    CHECK(codes[0] == 1);
    CHECK(req->method() == Post);
    CHECK(req->body() == body);
    CHECK(req->realContentLength() == 100);
}

DROGON_TEST(RequestParserChunked)
{
    HttpRequestImplPtr req;
    std::string msg = "POST /u HTTP/1.1\r\nHost: h\r\nTransfer-Encoding: chunked\r\n\r\n"
                      "5\r\nhello\r\n6\r\n world\r\n0\r\n\r\n";
    auto codes = drive(msg, 0, &req);
    CHECK(codes[0] == 1);
    CHECK(req->body() == "hello world");
    // Non-stream chunked: synthetic content-length injected, TE removed
    CHECK(req->getHeader("content-length") == "11");
    CHECK(req->getHeader("transfer-encoding").empty());
}

DROGON_TEST(RequestParserPipelined)
{
    std::string one = "GET / HTTP/1.1\r\nHost: h\r\n\r\n";
    auto codes = drive(one + one + one, 0);
    CHECK(codes.size() == 3);
    CHECK(codes[0] == 1);
    CHECK(codes[1] == 1);
    CHECK(codes[2] == 1);
}

DROGON_TEST(RequestParserByteSplit)
{
    HttpRequestImplPtr whole, split2, split3;
    std::string body(50, 'a');
    std::string msg = "POST /u HTTP/1.1\r\nHost: h\r\nContent-Length: 50\r\n\r\n" + body;
    drive(msg, 0, &whole);
    drive(msg, 2, &split2);
    drive(msg, 3, &split3);
    CHECK(whole->body() == body);
    CHECK(split2->body() == body);
    CHECK(split3->body() == body);
    CHECK(split2->method() == Post);
    CHECK(split3->path() == "/u");
}

DROGON_TEST(RequestParserSmugglingTeCl)
{
    // TE + CL: legacy silently lets CL win (smuggling vector); llhttp rejects.
    std::string msg = "POST /u HTTP/1.1\r\nHost: h\r\nContent-Length: 5\r\nTransfer-Encoding: chunked\r\n\r\nhello";
    auto codes = drive(msg, 0);
#ifdef DROGON_USE_LLHTTP
    CHECK(codes[0] < 0);
#else
    CHECK(codes[0] == 1);
#endif
}

DROGON_TEST(RequestParserNoColonHeader)
{
    // Header line without a colon: legacy silently ends the header section
    // (desync); llhttp rejects with 400.
    std::string msg = "GET / HTTP/1.1\r\nHost: h\r\nBadHeaderNoColon\r\n\r\n";
    auto codes = drive(msg, 0);
#ifdef DROGON_USE_LLHTTP
    CHECK(codes[0] < 0);
#else
    CHECK(codes[0] == 1);
#endif
}

DROGON_TEST(RequestParserExpect100ContinueNoConn)
{
    // No connection available -> adapter/legacy both return -1 (cannot send 100).
    std::string msg = "POST /u HTTP/1.1\r\nHost: h\r\nContent-Length: 5\r\nExpect: 100-continue\r\n\r\nhello";
    auto codes = drive(msg, 0);
    CHECK(codes[0] == -1);
}

DROGON_TEST(RequestParserStreamMode)
{
    app().enableRequestStream(true);
    std::string body(10, 'b');
    std::string msg = "POST /u HTTP/1.1\r\nHost: h\r\nContent-Length: 10\r\n\r\n" + body;
    HttpRequestImplPtr req;
    auto codes = drive(msg, 0, &req);
    app().enableRequestStream(false);
    CHECK(codes.size() == 2);
    CHECK(codes[0] == 3);  // stream headers complete
    CHECK(codes[1] == 1);  // stream body complete (1, like non-stream)
    CHECK(req->isStreamMode());
    CHECK(req->realContentLength() == 10);
}