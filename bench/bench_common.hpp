// bench/bench_common.hpp
// Shared benchmark scenarios — mirrors tell-bench/src/lib.rs.

#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace tell_bench {

struct BenchScenario {
    const char* name;
    size_t events_per_batch;
    size_t payload_size;

    size_t total_bytes() const { return events_per_batch * payload_size; }
};

constexpr BenchScenario SCENARIOS[] = {
    {"realtime_small", 10, 100},
    {"typical", 100, 200},
    {"high_volume", 500, 200},
    {"large_events", 100, 1000},
};

constexpr size_t SCENARIO_COUNT = sizeof(SCENARIOS) / sizeof(SCENARIOS[0]);

// Generate a JSON payload of approximately the given size in bytes.
inline std::vector<uint8_t> generate_payload(size_t size) {
    // Build {"user_id":"user_bench_123","event":"Benchmark Event","data":"xxx..."}
    std::string base = R"({"user_id":"user_bench_123","event":"Benchmark Event"})";

    if (base.size() >= size) {
        return std::vector<uint8_t>(base.begin(), base.end());
    }

    // Replace closing '}' with data field padded to target size
    base.pop_back(); // remove '}'
    base += R"(,"data":")";
    size_t remaining = size - base.size() - 2; // account for closing quote + brace
    base.append(remaining, 'x');
    base += "\"}";

    return std::vector<uint8_t>(base.begin(), base.end());
}

} // namespace tell_bench
