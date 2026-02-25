// include/tell/config.hpp
// Flat configuration struct with builder pattern — spec §3 defaults.

#pragma once

#include "error.hpp"
#include <array>
#include <chrono>
#include <functional>
#include <string>

namespace tell {

class TellConfigBuilder;

// Configuration for the Tell SDK.
class TellConfig {
public:
    using ErrorCallback = std::function<void(const TellError&)>;

    static TellConfigBuilder builder(const std::string& api_key);

    // Presets (spec requires exactly 2).
    static TellConfig production(const std::string& api_key);
    static TellConfig development(const std::string& api_key);

    const std::array<uint8_t, 16>& api_key_bytes() const noexcept { return api_key_bytes_; }
    const std::string& service() const noexcept { return service_; }
    const std::string& endpoint() const noexcept { return endpoint_; }
    size_t batch_size() const noexcept { return batch_size_; }
    std::chrono::milliseconds flush_interval() const noexcept { return flush_interval_; }
    uint32_t max_retries() const noexcept { return max_retries_; }
    std::chrono::milliseconds close_timeout() const noexcept { return close_timeout_; }
    std::chrono::milliseconds network_timeout() const noexcept { return network_timeout_; }
    const ErrorCallback& on_error() const noexcept { return on_error_; }

private:
    friend class TellConfigBuilder;

    std::array<uint8_t, 16> api_key_bytes_{};
    std::string service_;
    std::string endpoint_ = "collect.tell.rs:50000";
    size_t batch_size_ = 100;
    std::chrono::milliseconds flush_interval_{10000};
    uint32_t max_retries_ = 3;
    std::chrono::milliseconds close_timeout_{5000};
    std::chrono::milliseconds network_timeout_{30000};
    ErrorCallback on_error_;
};

// Fluent builder for TellConfig.
class TellConfigBuilder {
public:
    explicit TellConfigBuilder(const std::string& api_key);

    TellConfigBuilder& service(std::string service);
    TellConfigBuilder& endpoint(std::string endpoint);
    TellConfigBuilder& batch_size(size_t size);
    TellConfigBuilder& flush_interval(std::chrono::milliseconds interval);
    TellConfigBuilder& max_retries(uint32_t retries);
    TellConfigBuilder& close_timeout(std::chrono::milliseconds timeout);
    TellConfigBuilder& network_timeout(std::chrono::milliseconds timeout);
    TellConfigBuilder& on_error(TellConfig::ErrorCallback callback);

    // Build the config. Throws TellError on invalid API key.
    TellConfig build() const;

private:
    std::string api_key_;
    TellConfig config_;
};

} // namespace tell
