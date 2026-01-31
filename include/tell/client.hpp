// include/tell/client.hpp
// Tell analytics client — full spec API (§2).

#pragma once

#include "config.hpp"
#include "error.hpp"
#include "props.hpp"
#include "types.hpp"
#include <memory>
#include <string>
#include <vector>

namespace tell {

// The Tell analytics client.
//
// Created via Tell::create(config). Ready to use immediately — no separate
// initialize() step. Internally spawns a background worker thread.
//
// Example:
//   auto client = Tell::create(TellConfig::production("a1b2c3...f90"));
//   client->track("user_123", "Page Viewed", Props().add("url", "/home"));
//   client->close();
class Tell {
public:
    // Create a new Tell client and spawn the background worker.
    // Throws TellError on invalid configuration.
    static std::unique_ptr<Tell> create(TellConfig config);

    ~Tell();

    Tell(const Tell&) = delete;
    Tell& operator=(const Tell&) = delete;
    Tell(Tell&&) noexcept;
    Tell& operator=(Tell&&) noexcept;

    // --- Events (§2.2) ---

    // Track a user action. Never blocks, never throws.
    void track(const std::string& user_id, const std::string& event_name,
               const Props& properties = Props());

    // Identify a user with optional traits.
    void identify(const std::string& user_id, const Props& traits = Props());

    // Associate a user with a group.
    void group(const std::string& user_id, const std::string& group_id,
               const Props& properties = Props());

    // Track a revenue event.
    void revenue(const std::string& user_id, double amount,
                 const std::string& currency, const std::string& order_id,
                 const Props& properties = Props());

    // Link two user identities.
    void alias(const std::string& previous_id, const std::string& user_id);

    // --- Logging (§2.3) ---

    // Send a structured log entry.
    void log(LogLevel level, const std::string& message,
             const std::string& service = "app", const Props& data = Props());

    void log_emergency(const std::string& message, const std::string& service = "app",
                       const Props& data = Props());
    void log_alert(const std::string& message, const std::string& service = "app",
                   const Props& data = Props());
    void log_critical(const std::string& message, const std::string& service = "app",
                      const Props& data = Props());
    void log_error(const std::string& message, const std::string& service = "app",
                   const Props& data = Props());
    void log_warning(const std::string& message, const std::string& service = "app",
                     const Props& data = Props());
    void log_notice(const std::string& message, const std::string& service = "app",
                    const Props& data = Props());
    void log_info(const std::string& message, const std::string& service = "app",
                  const Props& data = Props());
    void log_debug(const std::string& message, const std::string& service = "app",
                   const Props& data = Props());
    void log_trace(const std::string& message, const std::string& service = "app",
                   const Props& data = Props());

    // --- Super Properties ---

    // Register properties that will be merged into every track/group/revenue event.
    void register_props(const Props& properties);

    // Remove a super property by key.
    void unregister(const std::string& key);

    // --- Session ---

    // Rotate the session ID.
    void reset_session();

    // --- Lifecycle (§2.4) ---

    // Force-send all queued batches, blocks until complete.
    void flush();

    // Flush + close connection, blocks up to close_timeout.
    void close();

private:
    explicit Tell(TellConfig config);
    struct Inner;
    std::unique_ptr<Inner> inner_;
};

} // namespace tell
