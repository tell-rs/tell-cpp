// tests/client_test.cpp
// Client lifecycle, super properties, concurrency, and timeout tests.

#include "tell/client.hpp"
#include "tell/config.hpp"
#include "tell/props.hpp"
#include "tell/types.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <string>
#include <thread>
#include <vector>

namespace tell {
namespace {

// Helper: create a client with short timeouts, no retries, swallowed errors.
std::unique_ptr<Tell> make_test_client() {
    auto config = TellConfig::builder("a1b2c3d4e5f60718293a4b5c6d7e8f90")
        .endpoint("localhost:19999")
        .batch_size(10)
        .flush_interval(std::chrono::milliseconds(100))
        .close_timeout(std::chrono::milliseconds(2000))
        .network_timeout(std::chrono::milliseconds(500))
        .max_retries(0)
        .on_error([](const TellError&) {})
        .build();
    return Tell::create(std::move(config));
}

// ==================== Lifecycle ====================

TEST(ClientTest, CreateAndClose) {
    auto client = make_test_client();
    client->close();
}

TEST(ClientTest, CreateAndDestroy) {
    auto client = make_test_client();
    // Destructor cleans up without hanging.
}

TEST(ClientTest, FlushThenClose) {
    auto client = make_test_client();
    client->track("user_1", "Event A");
    client->flush();
    client->close();
}

TEST(ClientTest, MultipleFlushes) {
    auto client = make_test_client();
    client->track("user_1", "Event A");
    client->flush();
    client->track("user_1", "Event B");
    client->flush();
    client->close();
}

TEST(ClientTest, CloseWithoutFlush) {
    auto client = make_test_client();
    client->track("user_1", "Event A");
    client->log_info("message");
    client->close();
}

// ==================== All API Methods ====================

TEST(ClientTest, AllMethodsComplete) {
    auto client = make_test_client();

    client->track("user_1", "Page Viewed", Props().add("url", "/home"));
    client->identify("user_1", Props().add("name", "Jane"));
    client->group("user_1", "group_1", Props().add("plan", "pro"));
    client->revenue("user_1", 49.99, "USD", "order_1", Props().add("product", "plan"));
    client->alias("old_user", "user_1");

    client->log_emergency("emergency");
    client->log_alert("alert");
    client->log_critical("critical");
    client->log_error("error");
    client->log_warning("warning");
    client->log_notice("notice");
    client->log_info("info");
    client->log_debug("debug");
    client->log_trace("trace");
    client->log(LogLevel::Info, "generic", "svc", Props().add("k", "v"));

    client->flush();
    client->close();
}

// ==================== Super Properties ====================

TEST(ClientTest, RegisterProps) {
    auto client = make_test_client();
    client->register_props(Props().add("plan", "pro").add("org", "Acme"));
    client->track("user_1", "Event A");
    client->close();
}

TEST(ClientTest, RegisterPropsOverwrite) {
    // Register same key twice — second must overwrite, not duplicate.
    auto client = make_test_client();
    client->register_props(Props().add("plan", "free"));
    client->register_props(Props().add("plan", "pro"));
    client->track("user_1", "Event A");
    client->close();
}

TEST(ClientTest, RegisterPropsMultipleCalls) {
    auto client = make_test_client();
    client->register_props(Props().add("a", 1));
    client->register_props(Props().add("b", "two"));
    client->register_props(Props().add("c", true));
    client->track("user_1", "Event A");
    client->close();
}

TEST(ClientTest, UnregisterProps) {
    auto client = make_test_client();
    client->register_props(Props().add("plan", "pro").add("org", "Acme"));
    client->unregister("plan");
    client->track("user_1", "Event A");
    client->close();
}

TEST(ClientTest, UnregisterNonExistent) {
    auto client = make_test_client();
    client->register_props(Props().add("plan", "pro"));
    client->unregister("nonexistent");
    client->close();
}

TEST(ClientTest, UnregisterEmpty) {
    auto client = make_test_client();
    client->unregister("anything");
    client->close();
}

TEST(ClientTest, RegisterUnregisterReregister) {
    auto client = make_test_client();
    client->register_props(Props().add("a", 1).add("b", 2));
    client->unregister("a");
    client->register_props(Props().add("c", 3));
    client->track("user_1", "Event A");
    client->close();
}

TEST(ClientTest, UnregisterAllThenTrack) {
    auto client = make_test_client();
    client->register_props(Props().add("x", 1));
    client->unregister("x");
    // Super props map is now empty — track should still work.
    client->track("user_1", "Event A");
    client->close();
}

// ==================== Session ====================

TEST(ClientTest, ResetSession) {
    auto client = make_test_client();
    client->track("user_1", "Before Reset");
    client->reset_session();
    client->track("user_1", "After Reset");
    client->close();
}

// ==================== Validation Errors ====================

TEST(ClientTest, ValidationErrors) {
    std::atomic<int> error_count{0};
    auto config = TellConfig::builder("a1b2c3d4e5f60718293a4b5c6d7e8f90")
        .endpoint("localhost:19999")
        .max_retries(0)
        .network_timeout(std::chrono::milliseconds(500))
        .close_timeout(std::chrono::milliseconds(2000))
        .on_error([&](const TellError& e) {
            EXPECT_EQ(e.kind(), ErrorKind::Validation);
            error_count.fetch_add(1);
        })
        .build();
    auto client = Tell::create(std::move(config));

    client->track("", "Event");              // empty user_id
    client->track("user", "");               // empty event_name
    client->identify("");                    // empty user_id
    client->group("user", "");               // empty group_id
    client->revenue("user", -1, "USD", "o"); // negative amount
    client->revenue("user", 10, "", "o");    // empty currency
    client->revenue("user", 10, "USD", "");  // empty order_id
    client->alias("", "user");               // empty previous_id
    client->log(LogLevel::Info, "");         // empty message

    client->close();

    EXPECT_EQ(error_count.load(), 9);
}

// ==================== Concurrency ====================

TEST(ClientTest, ConcurrentTrack) {
    auto client = make_test_client();
    constexpr int kThreads = 8;
    constexpr int kEventsPerThread = 100;

    std::vector<std::thread> threads;
    threads.reserve(kThreads);
    for (int t = 0; t < kThreads; t++) {
        threads.emplace_back([&client, t]() {
            for (int i = 0; i < kEventsPerThread; i++) {
                client->track("user_" + std::to_string(t),
                              "Event_" + std::to_string(i),
                              Props().add("thread", t).add("seq", i));
            }
        });
    }

    for (auto& t : threads) t.join();
    client->close();
}

TEST(ClientTest, ConcurrentMixedOps) {
    auto client = make_test_client();
    std::vector<std::thread> threads;

    // Track threads
    for (int t = 0; t < 4; t++) {
        threads.emplace_back([&client, t]() {
            for (int i = 0; i < 50; i++) {
                client->track("user_" + std::to_string(t), "Event");
            }
        });
    }

    // Register/unregister thread
    threads.emplace_back([&client]() {
        for (int i = 0; i < 50; i++) {
            client->register_props(Props().add("key", i));
            client->unregister("key");
        }
    });

    // Log thread
    threads.emplace_back([&client]() {
        for (int i = 0; i < 50; i++) {
            client->log_info("msg_" + std::to_string(i));
        }
    });

    // Session reset thread
    threads.emplace_back([&client]() {
        for (int i = 0; i < 20; i++) {
            client->reset_session();
        }
    });

    for (auto& t : threads) t.join();
    client->close();
}

TEST(ClientTest, ConcurrentFlush) {
    auto client = make_test_client();

    for (int i = 0; i < 50; i++) {
        client->track("user_1", "Event_" + std::to_string(i));
    }

    // Multiple threads calling flush simultaneously — tests the
    // per-request promise/future completion (Bug #4 fix).
    std::vector<std::thread> threads;
    for (int t = 0; t < 4; t++) {
        threads.emplace_back([&client]() {
            client->flush();
        });
    }

    for (auto& t : threads) t.join();
    client->close();
}

// ==================== Timeout ====================

TEST(ClientTest, FlushReturnsWithinTimeout) {
    auto config = TellConfig::builder("a1b2c3d4e5f60718293a4b5c6d7e8f90")
        .endpoint("localhost:19999")
        .max_retries(0)
        .network_timeout(std::chrono::milliseconds(200))
        .close_timeout(std::chrono::milliseconds(1000))
        .on_error([](const TellError&) {})
        .build();
    auto client = Tell::create(std::move(config));

    client->track("user_1", "Event");

    auto start = std::chrono::steady_clock::now();
    client->flush();
    auto elapsed = std::chrono::steady_clock::now() - start;

    EXPECT_LT(elapsed, std::chrono::seconds(3));
    client->close();
}

TEST(ClientTest, CloseReturnsWithinTimeout) {
    auto config = TellConfig::builder("a1b2c3d4e5f60718293a4b5c6d7e8f90")
        .endpoint("localhost:19999")
        .max_retries(0)
        .network_timeout(std::chrono::milliseconds(200))
        .close_timeout(std::chrono::milliseconds(1000))
        .on_error([](const TellError&) {})
        .build();
    auto client = Tell::create(std::move(config));

    client->track("user_1", "Event");

    auto start = std::chrono::steady_clock::now();
    client->close();
    auto elapsed = std::chrono::steady_clock::now() - start;

    EXPECT_LT(elapsed, std::chrono::seconds(3));
}

} // namespace
} // namespace tell
