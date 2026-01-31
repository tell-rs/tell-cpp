// bench/encoding_bench.cpp
// Encoding benchmarks — mirrors tell-bench/benches/encoding.rs.

#include <benchmark/benchmark.h>
#include "bench_common.hpp"
#include "encoding.hpp"

using namespace tell;
using namespace tell::encoding;
using namespace tell_bench;

// --- encode_event ---

static void BM_EncodeEvent(benchmark::State& state) {
    size_t scenario_idx = static_cast<size_t>(state.range(0));
    const auto& scenario = SCENARIOS[scenario_idx];
    auto payload = generate_payload(scenario.payload_size);

    uint8_t device_id[16];
    uint8_t session_id[16];
    std::memset(device_id, 0x42, 16);
    std::memset(session_id, 0x43, 16);

    std::vector<uint8_t> buf;
    for (auto _ : state) {
        buf.clear();
        EventParams params;
        params.event_type = EventType::Track;
        params.timestamp = 1700000000000;
        params.device_id = device_id;
        params.session_id = session_id;
        params.event_name = "Page Viewed";
        params.event_name_len = 11;
        params.payload = payload.data();
        params.payload_len = payload.size();
        encode_event_into(buf, params);
        benchmark::DoNotOptimize(buf.data());
    }
    state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) *
                            static_cast<int64_t>(payload.size()));
    state.SetLabel(scenario.name);
}

BENCHMARK(BM_EncodeEvent)->DenseRange(0, SCENARIO_COUNT - 1);

// --- encode_event_data (batch of pre-encoded events) ---

static void BM_EncodeEventData(benchmark::State& state) {
    size_t batch_size = static_cast<size_t>(state.range(0));

    uint8_t device_id[16];
    uint8_t session_id[16];
    std::memset(device_id, 0x42, 16);
    std::memset(session_id, 0x43, 16);
    auto payload = generate_payload(200);

    std::vector<EventParams> params(batch_size);
    for (auto& p : params) {
        p.event_type = EventType::Track;
        p.timestamp = 1700000000000;
        p.device_id = device_id;
        p.session_id = session_id;
        p.event_name = "Page Viewed";
        p.event_name_len = 11;
        p.payload = payload.data();
        p.payload_len = payload.size();
    }

    std::vector<uint8_t> buf;
    buf.reserve(64 * 1024);
    for (auto _ : state) {
        buf.clear();
        encode_event_data_into(buf, params);
        benchmark::DoNotOptimize(buf.data());
    }
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) *
                            static_cast<int64_t>(batch_size));
}

BENCHMARK(BM_EncodeEventData)->Arg(10)->Arg(100)->Arg(500);

// --- encode_full_batch (events -> event_data -> batch) ---

static void BM_EncodeFullBatch(benchmark::State& state) {
    size_t scenario_idx = static_cast<size_t>(state.range(0));
    const auto& scenario = SCENARIOS[scenario_idx];
    auto payload = generate_payload(scenario.payload_size);

    uint8_t api_key[16];
    uint8_t device_id[16];
    uint8_t session_id[16];
    std::memset(api_key, 0xA1, 16);
    std::memset(device_id, 0x42, 16);
    std::memset(session_id, 0x43, 16);

    std::vector<EventParams> params(scenario.events_per_batch);
    for (auto& p : params) {
        p.event_type = EventType::Track;
        p.timestamp = 1700000000000;
        p.device_id = device_id;
        p.session_id = session_id;
        p.event_name = "Page Viewed";
        p.event_name_len = 11;
        p.payload = payload.data();
        p.payload_len = payload.size();
    }

    std::vector<uint8_t> data_buf;
    std::vector<uint8_t> batch_buf;
    data_buf.reserve(64 * 1024);

    for (auto _ : state) {
        data_buf.clear();
        batch_buf.clear();
        size_t start = encode_event_data_into(data_buf, params);

        BatchParams bp;
        bp.api_key = api_key;
        bp.schema_type = SchemaType::Event;
        bp.version = 100;
        bp.batch_id = 1;
        bp.data = data_buf.data() + start;
        bp.data_len = data_buf.size() - start;
        encode_batch_into(batch_buf, bp);
        benchmark::DoNotOptimize(batch_buf.data());
    }
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) *
                            static_cast<int64_t>(scenario.events_per_batch));
    state.SetLabel(scenario.name);
}

BENCHMARK(BM_EncodeFullBatch)->DenseRange(0, SCENARIO_COUNT - 1);

// --- encode_log_entry ---

static void BM_EncodeLogEntry(benchmark::State& state) {
    size_t scenario_idx = static_cast<size_t>(state.range(0));
    const auto& scenario = SCENARIOS[scenario_idx];
    auto payload = generate_payload(scenario.payload_size);

    uint8_t session_id[16];
    std::memset(session_id, 0x43, 16);

    std::vector<uint8_t> buf;
    for (auto _ : state) {
        buf.clear();
        LogEntryParams params;
        params.event_type = LogEventType::Log;
        params.session_id = session_id;
        params.level = LogLevel::Error;
        params.timestamp = 1700000000000;
        params.source = "bench-host";
        params.source_len = 10;
        params.service = "api";
        params.service_len = 3;
        params.payload = payload.data();
        params.payload_len = payload.size();
        encode_log_entry_into(buf, params);
        benchmark::DoNotOptimize(buf.data());
    }
    state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) *
                            static_cast<int64_t>(payload.size()));
    state.SetLabel(scenario.name);
}

BENCHMARK(BM_EncodeLogEntry)->DenseRange(0, SCENARIO_COUNT - 1);

// --- encode_log_batch (log entries -> log_data -> batch) ---

static void BM_EncodeLogBatch(benchmark::State& state) {
    size_t batch_size = static_cast<size_t>(state.range(0));
    auto payload = generate_payload(200);

    uint8_t api_key[16];
    uint8_t session_id[16];
    std::memset(api_key, 0xA1, 16);
    std::memset(session_id, 0x43, 16);

    std::vector<LogEntryParams> params(batch_size);
    for (auto& p : params) {
        p.event_type = LogEventType::Log;
        p.session_id = session_id;
        p.level = LogLevel::Info;
        p.timestamp = 1700000000000;
        p.source = "bench-host";
        p.source_len = 10;
        p.service = "api";
        p.service_len = 3;
        p.payload = payload.data();
        p.payload_len = payload.size();
    }

    std::vector<uint8_t> data_buf;
    std::vector<uint8_t> batch_buf;
    data_buf.reserve(64 * 1024);

    for (auto _ : state) {
        data_buf.clear();
        batch_buf.clear();
        size_t start = encode_log_data_into(data_buf, params);

        BatchParams bp;
        bp.api_key = api_key;
        bp.schema_type = SchemaType::Log;
        bp.version = 100;
        bp.batch_id = 1;
        bp.data = data_buf.data() + start;
        bp.data_len = data_buf.size() - start;
        encode_batch_into(batch_buf, bp);
        benchmark::DoNotOptimize(batch_buf.data());
    }
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) *
                            static_cast<int64_t>(batch_size));
}

BENCHMARK(BM_EncodeLogBatch)->Arg(10)->Arg(100)->Arg(500);

BENCHMARK_MAIN();
