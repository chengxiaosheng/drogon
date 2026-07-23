#include "corpus.h"

namespace drogon_bench
{
namespace
{
std::string body(size_t n)
{
    return std::string(n, 'x');
}

std::string simpleRequest()
{
    return "GET / HTTP/1.1\r\nHost: example.com\r\n\r\n";
}

std::string heavyRequestHeaders(size_t n)
{
    std::string s = "GET / HTTP/1.1\r\nHost: example.com\r\n";
    for (size_t i = 0; i < n; ++i)
    {
        s += "X-Custom-Header-";
        s += std::to_string(i);
        s += ": value-number-";
        s += std::to_string(i);
        s += "\r\n";
    }
    s += "\r\n";
    return s;
}

std::string heavyResponseHeaders(size_t n)
{
    std::string s = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n";
    for (size_t i = 0; i < n; ++i)
    {
        s += "X-Custom-Header-";
        s += std::to_string(i);
        s += ": value-number-";
        s += std::to_string(i);
        s += "\r\n";
    }
    s += "Content-Length: 5\r\n\r\nhello";
    return s;
}

std::string postBody(size_t n)
{
    std::string s = "POST /upload HTTP/1.1\r\nHost: example.com\r\n"
                    "Content-Length: ";
    s += std::to_string(n);
    s += "\r\n\r\n";
    s += body(n);
    return s;
}

std::string bodyResponse(size_t n)
{
    std::string s = "HTTP/1.1 200 OK\r\nContent-Length: ";
    s += std::to_string(n);
    s += "\r\n\r\n";
    s += body(n);
    return s;
}

std::string chunkedRequest()
{
    // Two chunks (the second carries an extension) + a terminating zero chunk.
    // No trailer: the legacy parser rejects trailers (a Phase 2 difference).
    std::string s = "POST /upload HTTP/1.1\r\nHost: example.com\r\n"
                    "Transfer-Encoding: chunked\r\n\r\n";
    s += "1000\r\n" + body(4096) + "\r\n";
    s += "800;ext=value\r\n" + body(2048) + "\r\n";
    s += "0\r\n\r\n";
    return s;
}

std::string chunkedResponse()
{
    std::string s = "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n";
    s += "1000\r\n" + body(4096) + "\r\n";
    s += "800\r\n" + body(2048) + "\r\n";
    s += "0\r\n\r\n";
    return s;
}

std::string pipelineGet(size_t n)
{
    std::string one = simpleRequest();
    std::string s;
    s.reserve(one.size() * n);
    for (size_t i = 0; i < n; ++i)
        s += one;
    return s;
}

std::string mixedPipeline()
{
    std::string s = simpleRequest();
    std::string post = "POST /upload HTTP/1.1\r\nHost: example.com\r\n"
                       "Content-Length: 10\r\n\r\n";
    post += body(10);
    s += post;
    s += simpleRequest();
    return s;
}

std::string closeDelimitedResponse()
{
    return "HTTP/1.1 200 OK\r\nConnection: close\r\n\r\n" + body(2048);
}
}  // namespace

const std::vector<CorpusEntry> &requestCorpus()
{
    static const std::vector<CorpusEntry> v = {
        {"req_simple", simpleRequest(), true, 1},
        {"req_headers_heavy", heavyRequestHeaders(30), true, 1},
        {"req_post_body_8k", postBody(8192), true, 1},
        {"req_post_body_1m", postBody(1024 * 1024), true, 1},
        {"req_chunked", chunkedRequest(), true, 1},
        {"req_pipeline_64", pipelineGet(64), true, 64},
        {"req_mixed", mixedPipeline(), true, 3},
    };
    return v;
}

const std::vector<CorpusEntry> &responseCorpus()
{
    static const std::vector<CorpusEntry> v = {
        {"resp_simple",
         "HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\nhello",
         false,
         1},
        {"resp_headers_heavy", heavyResponseHeaders(30), false, 1},
        {"resp_body_8k", bodyResponse(8192), false, 1},
        {"resp_chunked", chunkedResponse(), false, 1},
    };
    return v;
}

const std::vector<CorpusEntry> &responseCorpusLlhttpOnly()
{
    static const std::vector<CorpusEntry> v = {
        {"resp_close_delimited", closeDelimitedResponse(), false, 1},
    };
    return v;
}

std::vector<std::string> fragment(const std::string &s, size_t fragSize)
{
    std::vector<std::string> out;
    if (fragSize == 0 || s.size() <= fragSize)
    {
        out.push_back(s);
        return out;
    }
    for (size_t i = 0; i < s.size(); i += fragSize)
        out.push_back(s.substr(i, fragSize));
    return out;
}
}  // namespace drogon_bench