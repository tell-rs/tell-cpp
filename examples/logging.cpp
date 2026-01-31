// examples/logging.cpp
// Structured logging at all severity levels.

#include "tell/tell.hpp"
#include <iostream>

int main() {
    try {
        auto client = tell::Tell::create(
            tell::TellConfig::development("a1b2c3d4e5f60718293a4b5c6d7e8f90")
        );

        // Structured logging at different levels
        client->log_info("Server started", "api",
            tell::Props().add("port", 8080).add("workers", 4));

        client->log_warning("High memory usage", "api",
            tell::Props().add("used_mb", 3800).add("total_mb", 4096));

        client->log_error("Database connection failed", "api",
            tell::Props()
                .add("host", "db.internal")
                .add("error", "connection refused")
                .add("retry_count", 3));

        client->log_debug("Cache miss", "cache",
            tell::Props().add("key", "user:123:profile").add("ttl_remaining", 0));

        client->log_critical("Disk space critical", "infra",
            tell::Props().add("mount", "/data").add("used_percent", 98.5));

        // Generic log with explicit level
        client->log(tell::LogLevel::Notice, "Deployment completed", "deploy",
            tell::Props().add("version", "3.1.0").add("commit", "abc123f"));

        // Close
        client->close();

        std::cout << "Logs sent successfully." << std::endl;

    } catch (const tell::TellError& e) {
        std::cerr << "Fatal: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
