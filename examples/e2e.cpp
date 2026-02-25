// examples/e2e.cpp
// End-to-end smoke test — sends every API method to a real collector.
//
// Start your Tell server, then:
//
//   cmake -B build -DTELL_BUILD_EXAMPLES=ON && cmake --build build
//   ./build/tell_e2e
//
// Override endpoint:
//
//   TELL_ENDPOINT=collect.tell.rs:50000 ./build/tell_e2e
//
// Then verify on the collector that all events arrived.

#include "tell/tell.hpp"
#include <cstdlib>
#include <iostream>
#include <string>

static const char* API_KEY = "feed1e11feed1e11feed1e11feed1e11";
static const char* USER    = "e2e_user_cpp";

static void send(const char* label) {
    std::cout << "  -> " << label << std::endl;
}

int main() {
    std::string endpoint = "localhost:50000";
    if (const char* env = std::getenv("TELL_ENDPOINT")) {
        endpoint = env;
    }

    std::cout << std::endl;
    std::cout << "  Tell C++ SDK — E2E smoke test" << std::endl;
    std::cout << "  Endpoint: " << endpoint << std::endl;
    std::cout << std::endl;

    try {
        auto client = tell::Tell::create(
            tell::TellConfig::builder(API_KEY)
                .endpoint(endpoint)
                .service("cpp-e2e")
                .batch_size(10)
                .on_error([](const tell::TellError& err) {
                    std::cerr << "  !! " << err.what() << std::endl;
                })
                .build()
        );

        // -- Super properties --
        send("register super properties");
        client->register_props(
            tell::Props()
                .add("sdk", "cpp")
                .add("sdk_version", "0.1.0")
                .add("test", "e2e"));

        // -- Track --
        send("track with Props");
        client->track(USER, tell::Events::PAGE_VIEWED,
            tell::Props()
                .add("url", "/home")
                .add("referrer", "google")
                .add("screen", "1920x1080"));

        send("track with Props::add chaining");
        client->track(USER, tell::Events::FEATURE_USED,
            tell::Props()
                .add("feature", "export")
                .add("format", "csv")
                .add("rows", 1500));

        send("track with no properties");
        client->track(USER, "App Opened");

        // -- Identify --
        send("identify");
        client->identify(USER,
            tell::Props()
                .add("name", "E2E Test User")
                .add("email", "e2e@tell.app")
                .add("plan", "pro")
                .add("created_at", "2025-01-01T00:00:00Z"));

        // -- Group --
        send("group");
        client->group(USER, "org_cpp_sdk",
            tell::Props()
                .add("name", "Tell Engineering")
                .add("plan", "enterprise")
                .add("seats", 50));

        // -- Revenue --
        send("revenue with properties");
        client->revenue(USER, 49.99, "USD", "order_e2e_001",
            tell::Props()
                .add("product", "pro_annual")
                .add("coupon", "LAUNCH50"));

        send("revenue without properties");
        client->revenue(USER, 9.99, "USD", "order_e2e_002");

        // -- Alias --
        send("alias");
        client->alias("anon_visitor_abc", USER);

        // -- Logging — all 9 levels --
        send("log_emergency");
        client->log_emergency("System failure — disk full", "storage",
            tell::Props().add("disk", "/dev/sda1").add("usage_pct", 100));

        send("log_alert");
        client->log_alert("Database replication lag > 30s", "db",
            tell::Props().add("lag_seconds", 34));

        send("log_critical");
        client->log_critical("Payment gateway unreachable", "billing",
            tell::Props().add("gateway", "stripe").add("timeout_ms", 5000));

        send("log_error");
        client->log_error("Failed to send email", "notifications",
            tell::Props().add("recipient", "user@example.com").add("error", "SMTP timeout"));

        send("log_warning");
        client->log_warning("Rate limit approaching", "api",
            tell::Props().add("current_rps", 950).add("limit_rps", 1000));

        send("log_notice");
        client->log_notice("New deployment started", "deploy",
            tell::Props().add("version", "2.1.0").add("region", "us-east-1"));

        send("log_info");
        client->log_info("User signed in", "auth",
            tell::Props().add("method", "oauth").add("provider", "github"));

        send("log_debug");
        client->log_debug("Cache miss for key", "cache",
            tell::Props().add("key", "user:e2e:profile").add("ttl_remaining", 0));

        send("log_trace");
        client->log_trace("Entering request handler", "http",
            tell::Props().add("method", "GET").add("path", "/api/v1/events"));

        send("log with no service/data");
        client->log_info("Heartbeat");

        // -- Unregister --
        send("unregister 'test' super property");
        client->unregister("test");

        send("track after unregister (should lack 'test' key)");
        client->track(USER, "Post Unregister",
            tell::Props().add("step", "verify_unregister"));

        // -- Session reset --
        send("reset_session");
        client->reset_session();

        send("track after reset (new session_id)");
        client->track(USER, "Post Reset",
            tell::Props().add("step", "verify_new_session"));

        // -- Flush & close --
        send("flush");
        client->flush();
        std::cout << "  .. flush ok" << std::endl;

        send("close");
        client->close();
        std::cout << "  .. close ok" << std::endl;

    } catch (const tell::TellError& e) {
        std::cerr << "Fatal: " << e.what() << std::endl;
        return 1;
    }

    std::cout << std::endl;
    std::cout << "  Done — 23 calls sent. Verify on the collector." << std::endl;
    std::cout << std::endl;

    return 0;
}
