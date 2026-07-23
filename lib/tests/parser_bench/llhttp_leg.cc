#include "llhttp_leg.h"
#include "corpus.h"
#include <llhttp.h>

namespace drogon_bench
{
namespace
{
struct Counters
{
    int messages = 0;
    long headerBytes = 0;
    long bodyBytes = 0;
};

static int on_hdr_field(llhttp_t *p, const char *, size_t l)
{
    static_cast<Counters *>(p->data)->headerBytes += (long)l;
    return 0;
}
static int on_hdr_value(llhttp_t *p, const char *, size_t l)
{
    static_cast<Counters *>(p->data)->headerBytes += (long)l;
    return 0;
}
static int on_body(llhttp_t *p, const char *, size_t l)
{
    static_cast<Counters *>(p->data)->bodyBytes += (long)l;
    return 0;
}
static int on_msg_complete(llhttp_t *p)
{
    static_cast<Counters *>(p->data)->messages++;
    return 0;
}

bool parse(const std::string &data,
           size_t fragSize,
           llhttp_type_t type,
           size_t expectMessages)
{
    Counters cnt;
    llhttp_t parser;
    llhttp_settings_t settings;
    llhttp_settings_init(&settings);
    settings.on_header_field = on_hdr_field;
    settings.on_header_value = on_hdr_value;
    settings.on_body = on_body;
    settings.on_message_complete = on_msg_complete;
    llhttp_init(&parser, type, &settings);
    parser.data = &cnt;
    auto frags = fragment(data, fragSize);
    for (auto &f : frags)
    {
        llhttp_errno_t err = llhttp_execute(&parser, f.data(), f.size());
        if (err != HPE_OK)
            return false;  // we never pause, so HPE_PAUSED is unexpected here
    }
    // For close-delimited responses (no Content-Length / chunked) llhttp keeps
    // the message open until EOF; signal it so on_message_complete fires. For
    // CL/chunked responses the message is already complete, so skip finish.
    if (type == HTTP_RESPONSE && cnt.messages < (int)expectMessages)
    {
        (void)llhttp_finish(&parser);
    }

    // Touch the counters so the compiler cannot elide the callback work.
    if (cnt.messages < 0 || cnt.headerBytes < 0 || cnt.bodyBytes < 0)
        return false;
    return cnt.messages == (int)expectMessages;
}
}  // namespace

bool llhttpParseRequest(const std::string &data,
                        size_t fragSize,
                        size_t expectMessages)
{
    return parse(data, fragSize, HTTP_REQUEST, expectMessages);
}
bool llhttpParseResponse(const std::string &data,
                         size_t fragSize,
                         size_t expectMessages)
{
    return parse(data, fragSize, HTTP_RESPONSE, expectMessages);
}
}  // namespace drogon_bench