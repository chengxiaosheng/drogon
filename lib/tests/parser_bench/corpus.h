#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace drogon_bench
{
// A single HTTP message (or a pipelined batch) used by the microbenchmark.
struct CorpusEntry
{
    std::string name;
    std::string data;       // raw wire bytes (one or more complete messages)
    bool isRequest;         // request vs response
    size_t messageCount;    // number of complete messages in `data`
};

// Corpus valid for BOTH the legacy and llhttp legs. Divergent inputs (e.g.
// chunk trailers, which the legacy parser rejects) are intentionally excluded;
// they belong to the Phase 2 correctness/difference suite.
const std::vector<CorpusEntry> &requestCorpus();
const std::vector<CorpusEntry> &responseCorpus();

// Response entries only llhttp can parse (close-delimited bodies): the legacy
// response parser calls conn_->shutdown() on this path and cannot be driven
// with a null connection.
const std::vector<CorpusEntry> &responseCorpusLlhttpOnly();

// Split `s` into contiguous fragments of `fragSize` bytes (last may be
// shorter). fragSize == 0 yields a single fragment holding the whole buffer.
std::vector<std::string> fragment(const std::string &s, size_t fragSize);
}  // namespace drogon_bench