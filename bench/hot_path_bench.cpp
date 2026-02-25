// bench/hot_path_bench.cpp
// SDK API hot-path benchmarks — mirrors tell-bench/benches/hot_path.rs.

#include <benchmark/benchmark.h>
#include "tell/tell.hpp"

using namespace tell;

// Non-routable endpoint — worker spawns but never connects.
// Large batch size + long flush interval prevent auto-flush during bench.
static std::unique_ptr<Tell> make_client() {
    auto config = TellConfig::builder("feed1e11feed1e11feed1e11feed1e11")
        .endpoint("192.0.2.1:50000")
        .batch_size(100000)
        .flush_interval(std::chrono::milliseconds(3600000))
        .max_retries(0)
        .network_timeout(std::chrono::milliseconds(1))
        .build();
    return Tell::create(std::move(config));
}

// --- track ---

static void BM_TrackNoProps(benchmark::State& state) {
    auto client = make_client();
    for (auto _ : state) {
        client->track("user_bench_123", "Page Viewed");
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_TrackNoProps);

static void BM_TrackSmallProps(benchmark::State& state) {
    auto client = make_client();
    for (auto _ : state) {
        client->track("user_bench_123", "Page Viewed",
            Props().add("url", "/home").add("referrer", "google"));
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_TrackSmallProps);

static void BM_TrackLargeProps(benchmark::State& state) {
    auto client = make_client();
    for (auto _ : state) {
        client->track("user_bench_123", "Page Viewed",
            Props()
                .add("url", "/dashboard/analytics/overview")
                .add("referrer", "https://www.google.com/search?q=analytics+platform")
                .add("user_agent", "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36")
                .add("screen_width", 1920)
                .add("screen_height", 1080)
                .add("viewport_width", 1440)
                .add("viewport_height", 900)
                .add("color_depth", 24)
                .add("language", "en-US")
                .add("timezone", "America/New_York")
                .add("session_count", 42)
                .add("page_load_time_ms", 1234)
                .add("dom_ready_ms", 890)
                .add("first_paint_ms", 456));
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_TrackLargeProps);

static void BM_TrackWithSuperProps(benchmark::State& state) {
    auto client = make_client();
    client->register_props(
        Props()
            .add("app_version", "3.1.0")
            .add("env", "production")
            .add("platform", "web")
            .add("sdk_version", "0.1.0")
            .add("deployment_id", "deploy_abc123"));

    for (auto _ : state) {
        client->track("user_bench_123", "Page Viewed",
            Props().add("url", "/home").add("referrer", "google").add("page_type", "landing"));
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_TrackWithSuperProps);

// --- track burst ---

static void BM_TrackBurst(benchmark::State& state) {
    int64_t count = state.range(0);
    auto client = make_client();
    for (auto _ : state) {
        for (int64_t i = 0; i < count; ++i) {
            client->track("user_bench_123", "Page Viewed",
                Props().add("url", "/home"));
        }
    }
    state.SetItemsProcessed(state.iterations() * count);
}
BENCHMARK(BM_TrackBurst)->Arg(100)->Arg(1000)->Arg(10000);

// --- log ---

static void BM_LogError(benchmark::State& state) {
    auto client = make_client();
    for (auto _ : state) {
        client->log_error("Connection refused", "api",
            Props().add("host", "db.internal").add("port", 5432));
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_LogError);

// --- identify ---

static void BM_Identify(benchmark::State& state) {
    auto client = make_client();
    for (auto _ : state) {
        client->identify("user_bench_123",
            Props().add("name", "Jane Doe").add("email", "jane@example.com").add("plan", "pro"));
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Identify);

// --- revenue ---

static void BM_Revenue(benchmark::State& state) {
    auto client = make_client();
    for (auto _ : state) {
        client->revenue("user_bench_123", 49.99, "USD", "order_789",
            Props().add("product", "premium"));
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Revenue);

BENCHMARK_MAIN();
