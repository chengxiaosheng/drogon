# Phase 0 Decision Memo — HTTP parser microbenchmark (legacy vs llhttp)

## Setup
- Toolchain: MSVC (Visual Studio 18 2026), Release; static drogon + static llhttp
  (FetchContent_Declare, v9.4.2, per the approved plan).
- Harness asymmetry is **by design** (plan §3.1):
  - legacy leg = real `HttpRequestParser`/`HttpResponseParser` driven through
    `ParseCursor` over a `MsgBuffer` accumulation buffer (the production
    leftover path) — i.e. **legacy full** (parse + object building).
  - llhttp leg = minimal counting callbacks, **no drogon objects** — i.e. the
    **llhttp parse-phase ceiling**.
  - So ratios are an *upper bound* on llhttp's real advantage; the Phase 1
    adapter will add object building back and Phase 3 A/B measures end-to-end.
- Matrix: 7 request + 5 response corpus entries × {whole, 1460, 64, 1-byte}.
  All 92 cells `ok=1`.

## Logging caveat (response leg)
`app().setLogLevel(LError)` suppresses output, but zltoolkit's `TraceL`/`WriteL`
**always evaluates streamed arguments** even when the level filters the output
(the `LogContextCapture` destructor discards). `HttpResponseParser` has several
`TraceL << ...` per response; `HttpRequestParser::parseRequest` has none. Hence
the legacy **response** leg is inflated by always-on logging cost — a *real*
legacy cost (present in production too) that an llhttp adapter would avoid. The
**request** leg (the gate criteria) is clean.

## Gate-relevant results (whole buffer; ns/msg)

| corpus | legacy | llhttp | ratio |
|---|---:|---:|---:|
| req_headers_heavy | 4957 | 587 | 8.4x |
| req_pipeline_64 | 408 | 36 | 11.2x |
| req_post_body_8k | 1954 | 173 | 11.3x |
| req_post_body_1m | 792133 | 258786 | 3.1x |
| req_chunked | 2652 | 182 | 14.6x |

Body-heavy regression (8k / 1m), all fragmentations: llhttp is **faster in every
cell** — no regression.

Minimum ratio across all 92 cells: ~1.12x (req_chunked, 1-byte). llhttp stays
faster even at 1-byte fragmentation, where the legacy pull model is quadratic
(`findCRLF` rescans the accumulation buffer per byte) while llhttp's push model
is linear.

## Decision gate (plan §3.2)
- **#1 Performance**: llhttp >=1.5x on header-heavy (8.4x) **and** pipeline-64
  (11.2x); body-heavy regression <=10% (none). **MET.**
- **#2 Security/maintainability** (§1.2): TE+CL smuggling, unvalidated
  chunk-size, no-colon header desync, unbounded response parser, ~700-line
  hand-written FSM. **MET** (on paper).
- **Hard-stop** (pipeline-64 whole, llhttp slower): not triggered (llhttp 11.2x
  faster).
- **Stop** (<1.1x everywhere + strictness incompatible): not triggered (min
  ratio 1.12x).

## Decision: PROCEED to Phase 1.

## Artifacts
- Full CSV: `C:\tmp\parser_bench_results.csv`
- Source: `lib/tests/parser_bench/` (corpus, legacy_leg, llhttp_leg, main, CMakeLists)
- Legacy change: 2 behavior-neutral null guards in `HttpRequestParser.cc`
  (ctor + pooling deleter) — no-op for real connections; only enable driving the
  parser with a null `TcpConnectionPtr` in the benchmark.
- `DROGON_BUILD_PARSER_BENCH` (default ON, inside `BUILD_TESTING`) adds a
  FetchContent network dependency to test builds; disable with
  `-DDROGON_BUILD_PARSER_BENCH=OFF` for offline builds.