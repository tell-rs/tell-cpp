// src/config.cpp
// Configuration builder and presets.

#include "tell/config.hpp"
#include "validation.hpp"

namespace tell {

// --- TellConfig presets ---

TellConfigBuilder TellConfig::builder(const std::string& api_key) {
    return TellConfigBuilder(api_key);
}

TellConfig TellConfig::production(const std::string& api_key) {
    return TellConfig::builder(api_key).build();
}

TellConfig TellConfig::development(const std::string& api_key) {
    return TellConfig::builder(api_key)
        .endpoint("localhost:50000")
        .batch_size(10)
        .flush_interval(std::chrono::milliseconds(2000))
        .build();
}

// --- TellConfigBuilder ---

TellConfigBuilder::TellConfigBuilder(const std::string& api_key)
    : api_key_(api_key) {}

TellConfigBuilder& TellConfigBuilder::service(std::string service) {
    config_.service_ = std::move(service);
    return *this;
}

TellConfigBuilder& TellConfigBuilder::endpoint(std::string endpoint) {
    config_.endpoint_ = std::move(endpoint);
    return *this;
}

TellConfigBuilder& TellConfigBuilder::batch_size(size_t size) {
    config_.batch_size_ = size;
    return *this;
}

TellConfigBuilder& TellConfigBuilder::flush_interval(std::chrono::milliseconds interval) {
    config_.flush_interval_ = interval;
    return *this;
}

TellConfigBuilder& TellConfigBuilder::max_retries(uint32_t retries) {
    config_.max_retries_ = retries;
    return *this;
}

TellConfigBuilder& TellConfigBuilder::close_timeout(std::chrono::milliseconds timeout) {
    config_.close_timeout_ = timeout;
    return *this;
}

TellConfigBuilder& TellConfigBuilder::network_timeout(std::chrono::milliseconds timeout) {
    config_.network_timeout_ = timeout;
    return *this;
}

TellConfigBuilder& TellConfigBuilder::on_error(TellConfig::ErrorCallback callback) {
    config_.on_error_ = std::move(callback);
    return *this;
}

TellConfig TellConfigBuilder::build() const {
    TellConfig result = config_;
    result.api_key_bytes_ = validation::validate_and_decode_api_key(api_key_);
    return result;
}

} // namespace tell
