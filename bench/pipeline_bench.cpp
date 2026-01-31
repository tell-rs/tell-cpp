// bench/pipeline_bench.cpp
// Pipeline benchmarks (enqueue + flush over TCP) — mirrors tell-bench/benches/pipeline.rs.

#include <benchmark/benchmark.h>
#include "bench_common.hpp"
#include "tell/tell.hpp"

#include <atomic>
#include <cstring>
#include <thread>

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

using namespace tell;
using namespace tell_bench;

// Null TCP server: accepts connections and discards all data.
// Returns the "host:port" string and a stop flag.
struct NullServer {
    std::string address;
    std::atomic<bool> stop{false};
    std::thread thread;
    int listen_fd = -1;

    NullServer() {
        listen_fd = ::socket(AF_INET, SOCK_STREAM, 0);
        int opt = 1;
        ::setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port = 0; // OS-assigned port
        ::bind(listen_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
        ::listen(listen_fd, 8);

        socklen_t len = sizeof(addr);
        ::getsockname(listen_fd, reinterpret_cast<sockaddr*>(&addr), &len);
        address = "127.0.0.1:" + std::to_string(ntohs(addr.sin_port));

        thread = std::thread([this]() {
            while (!stop.load(std::memory_order_relaxed)) {
                fd_set fds;
                FD_ZERO(&fds);
                FD_SET(listen_fd, &fds);
                timeval tv{0, 100000}; // 100ms poll
                if (::select(listen_fd + 1, &fds, nullptr, nullptr, &tv) > 0) {
                    int client_fd = ::accept(listen_fd, nullptr, nullptr);
                    if (client_fd >= 0) {
                        // Drain in a detached thread
                        std::thread([this, client_fd]() {
                            char buf[64 * 1024];
                            while (!stop.load(std::memory_order_relaxed)) {
                                ssize_t n = ::recv(client_fd, buf, sizeof(buf), 0);
                                if (n <= 0) break;
                            }
                            ::close(client_fd);
                        }).detach();
                    }
                }
            }
        });
    }

    ~NullServer() {
        stop.store(true, std::memory_order_relaxed);
        if (listen_fd >= 0) ::close(listen_fd);
        if (thread.joinable()) thread.join();
    }
};

// --- pipeline_flush (events) ---

static void BM_PipelineFlush(benchmark::State& state) {
    size_t scenario_idx = static_cast<size_t>(state.range(0));
    const auto& scenario = SCENARIOS[scenario_idx];

    NullServer server;

    auto config = TellConfig::builder("a1b2c3d4e5f60718293a4b5c6d7e8f90")
        .endpoint(server.address)
        .batch_size(scenario.events_per_batch)
        .flush_interval(std::chrono::milliseconds(3600000))
        .build();
    auto client = Tell::create(std::move(config));

    // Warm up connection
    client->track("warmup", "Warmup");
    client->flush();

    // Build payload to approximate target size
    std::string padding(scenario.payload_size > 30 ? scenario.payload_size - 30 : 0, 'x');

    for (auto _ : state) {
        for (size_t i = 0; i < scenario.events_per_batch; ++i) {
            client->track("user_bench_123", "Page Viewed",
                Props().add("data", padding));
        }
        client->flush();
    }

    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) *
                            static_cast<int64_t>(scenario.events_per_batch));
    state.SetLabel(scenario.name);
    client->close();
}

BENCHMARK(BM_PipelineFlush)
    ->DenseRange(0, SCENARIO_COUNT - 1)
    ->Unit(benchmark::kMillisecond)
    ->MinTime(5.0)
    ->Iterations(20);

// --- pipeline_log_flush ---

static void BM_PipelineLogFlush(benchmark::State& state) {
    size_t scenario_idx = static_cast<size_t>(state.range(0));
    const auto& scenario = SCENARIOS[scenario_idx];

    NullServer server;

    auto config = TellConfig::builder("a1b2c3d4e5f60718293a4b5c6d7e8f90")
        .endpoint(server.address)
        .batch_size(scenario.events_per_batch)
        .flush_interval(std::chrono::milliseconds(3600000))
        .build();
    auto client = Tell::create(std::move(config));

    // Warm up
    client->log_info("warmup", "bench");
    client->flush();

    std::string padding(scenario.payload_size > 30 ? scenario.payload_size - 30 : 0, 'x');

    for (auto _ : state) {
        for (size_t i = 0; i < scenario.events_per_batch; ++i) {
            client->log_error("Connection failed", "api",
                Props().add("context", padding));
        }
        client->flush();
    }

    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) *
                            static_cast<int64_t>(scenario.events_per_batch));
    state.SetLabel(scenario.name);
    client->close();
}

// Only first 2 scenarios (realtime_small and typical), matching Rust
BENCHMARK(BM_PipelineLogFlush)
    ->DenseRange(0, 1)
    ->Unit(benchmark::kMillisecond)
    ->MinTime(5.0)
    ->Iterations(20);

BENCHMARK_MAIN();
