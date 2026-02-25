// Full TellConfig builder — all available options with defaults.
//
//   cmake -B build -DTELL_BUILD_EXAMPLES=ON && cmake --build build
//   ./build/tell_config

#include "tell/tell.hpp"
#include <chrono>
#include <iostream>

int main() {
    auto config = tell::TellConfig::builder("feed1e11feed1e11feed1e11feed1e11")
        .endpoint("collect.tell.rs:50000")                        // default: collect.tell.rs:50000
        .batch_size(100)                                          // default: 100 events per batch
        .flush_interval(std::chrono::milliseconds(10000))         // default: 10s between flushes
        .max_retries(3)                                           // default: 3 retry attempts
        .close_timeout(std::chrono::milliseconds(5000))           // default: 5s graceful shutdown
        .network_timeout(std::chrono::milliseconds(30000))        // default: 30s TCP timeout
        .on_error([](const tell::TellError& e) {                  // default: errors are silent
            std::cerr << "[Tell] " << e.what() << std::endl;
        })
        .build();

    auto client = tell::Tell::create(std::move(config));

    client->track("user_1", "Test");
    client->close();
}
