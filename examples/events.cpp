// Tell SDK — events, identify, revenue, logging.
//
//   cmake -B build -DTELL_BUILD_EXAMPLES=ON && cmake --build build
//   ./build/tell_events

#include "tell/tell.hpp"

int main() {
    auto client = tell::Tell::create(
        tell::TellConfig::development("feed1e11feed1e11feed1e11feed1e11")
    );

    // Track events
    client->track("user_123", "Page Viewed",
        tell::Props().add("url", "/home").add("referrer", "google"));

    // Identify users
    client->identify("user_123",
        tell::Props().add("name", "Jane").add("plan", "pro"));

    // Revenue
    client->revenue("user_123", 49.99, "USD", "order_456",
        tell::Props().add("product", "annual_plan"));

    // Structured logging
    client->log_error("DB connection failed", "api",
        tell::Props().add("host", "db.internal").add("retries", 3));

    client->log_info("User signed in", "auth",
        tell::Props().add("method", "oauth"));

    client->close();
}
