# Tell C++ SDK

<p align="center">
  <img src="https://img.shields.io/badge/status-alpha-orange" alt="Alpha">
  <a href="https://doc.rust-lang.org/edition-guide/"><img src="https://img.shields.io/badge/C++-17-blue.svg" alt="C++17"></a>
  <a href="https://opensource.org/licenses/MIT"><img src="https://img.shields.io/badge/License-MIT-yellow.svg" alt="License: MIT"></a>
</p>

C++ SDK for Tell — product analytics and structured logging, [1,000× faster](#performance) than PostHog and Mixpanel.

- **85 ns per call.** Serializes, encodes, and enqueues. Your thread moves on.
- **Fire & forget.** Synchronous API, background worker thread. Zero I/O blocking.
- **Thread-safe.** Shared via `std::unique_ptr`, internally synchronized with `shared_mutex`.
- **Zero dependencies.** Only the C++17 standard library and pthreads.

## Installation

```bash
# Add as a subdirectory or fetch via CMake
cmake_minimum_required(VERSION 3.14)
add_subdirectory(sdk-cpp)
target_link_libraries(your_target PRIVATE tell)
```

Or build directly:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

## Quick Start

```cpp
#include "tell/tell.hpp"

int main() {
    auto client = tell::Tell::create(
        tell::TellConfig::production("feed1e11feed1e11feed1e11feed1e11")
    );

    // Track events
    client->track("user_123", "Page Viewed",
        tell::Props().add("url", "/home").add("referrer", "google"));

    // Identify users
    client->identify("user_123",
        tell::Props().add("name", "Jane").add("plan", "pro"));

    // Revenue
    client->revenue("user_123", 49.99, "USD", "order_456",
        tell::Props().add("product", "premium_plan"));

    // Structured logging
    client->log_error("DB connection failed", "api",
        tell::Props().add("host", "db.internal"));

    client->close();
}
```

## Performance

**Caller latency** — serialize, encode, and enqueue (Apple M4 Pro):

| Operation | Latency |
|-----------|---------|
| `track` (no props) | 85 ns |
| `track` (small props) | 224 ns |
| `identify` | 272 ns |
| `log` | 273 ns |
| `revenue` | 326 ns |

```bash
cmake -B build -DTELL_BUILD_BENCHMARKS=ON
cmake --build build
./build/tell_hot_path_bench        # caller latency
./build/tell_encoding_bench        # encoding throughput
./build/tell_pipeline_bench        # delivery pipeline
```

## Configuration

```cpp
#include "tell/tell.hpp"

// Production — collect.tell.rs:50000, batch=100, flush=10s
auto config = tell::TellConfig::production("feed1e11feed1e11feed1e11feed1e11");

// Development — localhost:50000, batch=10, flush=2s
auto config = tell::TellConfig::development("feed1e11feed1e11feed1e11feed1e11");

// Custom — see examples/config.cpp for all builder options
auto config = tell::TellConfig::builder("feed1e11feed1e11feed1e11feed1e11")
    .service("my-backend")                // stamped on every event and log
    .endpoint("collect.internal:50000")
    .on_error([](const tell::TellError& e) {
        std::cerr << "[Tell] " << e.what() << std::endl;
    })
    .build();
```

## API

```cpp
auto client = tell::Tell::create(config);

// Events — user_id is always the first parameter
client->track(user_id, event_name, properties);
client->identify(user_id, traits);
client->group(user_id, group_id, properties);
client->revenue(user_id, amount, currency, order_id, properties);
client->alias(previous_id, user_id);

// Super properties — merged into every track/group/revenue call
client->register_props(tell::Props().add("app_version", "2.0"));
client->unregister("app_version");

// Logging
client->log(level, message, service, data);
client->log_info(message, service, data);
client->log_error(message, service, data);
// + log_emergency, log_alert, log_critical, log_warning,
//   log_notice, log_debug, log_trace

// Lifecycle
client->reset_session();
client->flush();
client->close();
```

Properties are built with the `Props` chainable builder:

```cpp
// Chainable builder
client->track("user_123", "Click",
    tell::Props()
        .add("url", "/home")
        .add("count", 42)
        .add("active", true)
        .add("score", 3.14));

// No properties (default parameter)
client->track("user_123", "Click");
```

## Examples

```bash
cmake -B build -DTELL_BUILD_EXAMPLES=ON && cmake --build build

./build/tell_events         # events, identify, revenue, logging
./build/tell_config         # full config builder with all options
./build/tell_logging        # all 9 log severity levels
./build/tell_e2e            # E2E smoke test against a running collector
```

## Requirements

- **C++17** compiler (GCC 7+, Clang 5+, MSVC 2017+)
- **CMake 3.14+**
- No external dependencies

## License

MIT
