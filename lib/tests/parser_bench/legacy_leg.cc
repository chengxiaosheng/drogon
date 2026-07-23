#include "legacy_leg.h"
#include "corpus.h"
#include "HttpRequestParser.h"
#include "HttpResponseParser.h"
#include <trantor/net/ParseCursor.h>
#include <trantor/utils/MsgBuffer.h>

using namespace drogon;
using namespace trantor;

namespace drogon_bench
{
namespace
{
template <typename ParseFn, typename CompleteFn>
bool feedAndParse(const std::string &data,
                  size_t fragSize,
                  size_t expectMessages,
                  ParseFn parse,
                  CompleteFn onComplete)
{
    MsgBuffer accum(8192);
    auto frags = fragment(data, fragSize);
    size_t fi = 0;
    size_t completed = 0;
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
        ParseCursor cursor(accum.peek(), accum.readableBytes());
        int res = parse(cursor);
        accum.retrieve(cursor.consumed());
        if (res < 0)
            return false;
        if (res == 0)
        {
            if (fi < frags.size())
            {
                accum.append(frags[fi].data(), frags[fi].size());
                ++fi;
                continue;
            }
            return false;
        }
        if (res == 1 || res == 2)
        {
            ++completed;
            onComplete();
            continue;
        }
        return false;
    }
    return completed == expectMessages;
}
}  // namespace

bool legacyParseRequest(const std::string &data,
                        size_t fragSize,
                        size_t expectMessages)
{
    TcpConnectionPtr nullConn;
    HttpRequestParser parser(nullConn);
    parser.reset();
    return feedAndParse(
        data, fragSize, expectMessages,
        [&parser](ParseCursor &c) { return parser.parseRequest(&c); },
        [&parser]() { parser.reset(); });
}

bool legacyParseResponse(const std::string &data,
                         size_t fragSize,
                         size_t expectMessages)
{
    TcpConnectionPtr nullConn;
    HttpResponseParser parser(nullConn);
    MsgBuffer accum(8192);
    auto frags = fragment(data, fragSize);
    size_t fi = 0;
    size_t completed = 0;
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
        ParseCursor cursor(accum.peek(), accum.readableBytes());
        bool ok = parser.parseResponse(&cursor);
        accum.retrieve(cursor.consumed());
        if (!ok)
            return false;
        if (parser.gotAll())
        {
            ++completed;
            parser.reset();
            continue;
        }
        if (fi < frags.size())
        {
            accum.append(frags[fi].data(), frags[fi].size());
            ++fi;
        }
        else
        {
            return false;
        }
    }
    return completed == expectMessages;
}
}  // namespace drogon_bench