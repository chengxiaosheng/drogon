#include "corpus.h"
#include "legacy_leg.h"
#include "llhttp_leg.h"
#include <drogon/HttpAppFramework.h>
#include <Util/logger.h>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

using namespace drogon_bench;
using Clock = std::chrono::steady_clock;

namespace
{
struct FragMode
{
    const char *name;
    size_t size;
};

const std::vector<FragMode> &fragModes()
{
    static const std::vector<FragMode> v = {
        {"whole", 0},
        {"1460", 1460},
        {"64", 64},
        {"1", 1},
    };
    return v;
}

struct Result
{
    double nsPerMsg;
    double mbPerS;
    bool ok;
};

template <typename Fn>
Result measure(Fn &parseOnce, const std::string &data, size_t nMsg)
{
    if (!parseOnce())
        return {0, 0, false};

    size_t warm = 0;
    auto t0 = Clock::now();
    while (std::chrono::duration<double>(Clock::now() - t0).count() < 0.1)
    {
        parseOnce();
        ++warm;
    }
    if (warm == 0)
        warm = 1;

    double perIter =
        std::chrono::duration<double>(Clock::now() - t0).count() / warm;
    size_t reps = (size_t)(0.1 / perIter);
    if (reps < 1)
        reps = 1;
    if (reps > 1000000)
        reps = 1000000;

    double times[5];
    for (int i = 0; i < 5; ++i)
    {
        auto s = Clock::now();
        for (size_t r = 0; r < reps; ++r)
            parseOnce();
        times[i] = std::chrono::duration<double>(Clock::now() - s).count();
    }
    std::sort(times, times + 5);
    double med = times[2];
    double nsPerMsg = med / reps / nMsg * 1e9;
    double mbPerS = (reps * data.size()) / med / 1e6;
    return {nsPerMsg, mbPerS, true};
}

void runRow(const char *parserName,
            const char *kind,
            const CorpusEntry &e,
            size_t fragSize,
            const char *fragName,
            const std::function<bool()> &parseOnce)
{
    Result r = measure(parseOnce, e.data, e.messageCount);
    std::printf("%s,%s,%s,%s,%.1f,%.1f,%d\n",
                parserName,
                kind,
                e.name.c_str(),
                fragName,
                r.nsPerMsg,
                r.mbPerS,
                r.ok ? 1 : 0);
    std::fflush(stdout);
}
}  // namespace

int main()
{
    drogon::app().setLogLevel(toolkit::LError);
    drogon::app().setClientMaxBodySize(64 * 1024 * 1024);
    drogon::app().setClientMaxMemoryBodySize(64 * 1024 * 1024);

    std::printf("parser,kind,corpus,fragmentation,ns_per_msg,MB_per_s,ok\n");
    std::fflush(stdout);

    for (const auto &fm : fragModes())
    {
        for (const auto &e : requestCorpus())
        {
            runRow("legacy", "request", e, fm.size, fm.name,
                   [&]() {
                       return legacyParseRequest(e.data, fm.size, e.messageCount);
                   });
            runRow("llhttp", "request", e, fm.size, fm.name,
                   [&]() {
                       return llhttpParseRequest(e.data, fm.size, e.messageCount);
                   });
        }
        for (const auto &e : responseCorpus())
        {
            runRow("legacy", "response", e, fm.size, fm.name,
                   [&]() {
                       return legacyParseResponse(e.data, fm.size, e.messageCount);
                   });
            runRow("llhttp", "response", e, fm.size, fm.name,
                   [&]() {
                       return llhttpParseResponse(e.data, fm.size, e.messageCount);
                   });
        }
        for (const auto &e : responseCorpusLlhttpOnly())
        {
            runRow("llhttp", "response", e, fm.size, fm.name,
                   [&]() {
                       return llhttpParseResponse(e.data, fm.size, e.messageCount);
                   });
        }
    }

    return 0;
}