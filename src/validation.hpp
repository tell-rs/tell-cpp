// src/validation.hpp
// Internal input validation functions.

#pragma once

#include "tell/error.hpp"
#include <array>
#include <string>

namespace tell {
namespace validation {

// Validate and decode a 32-character hex API key to 16 bytes.
inline std::array<uint8_t, 16> validate_and_decode_api_key(const std::string& api_key) {
    if (api_key.size() != 32) {
        throw TellError::configuration(
            "apiKey must be 32 hex characters, got " + std::to_string(api_key.size()));
    }

    std::array<uint8_t, 16> bytes{};
    for (size_t i = 0; i < 16; i++) {
        auto hex_val = [](char c) -> int {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            return -1;
        };

        int hi = hex_val(api_key[i * 2]);
        int lo = hex_val(api_key[i * 2 + 1]);
        if (hi < 0 || lo < 0) {
            throw TellError::configuration(
                std::string("apiKey contains non-hex character '") + api_key[hi < 0 ? i*2 : i*2+1] + "'");
        }
        bytes[i] = static_cast<uint8_t>((hi << 4) | lo);
    }

    return bytes;
}

inline bool check_user_id(const std::string& user_id) {
    return !user_id.empty();
}

// Validate an event name (non-empty, max 256 chars).
inline bool check_event_name(const std::string& name) {
    return !name.empty() && name.size() <= 256;
}

// Validate a log message (non-empty, max 64KB).
inline bool check_log_message(const std::string& message) {
    return !message.empty() && message.size() <= 65536;
}

// Validate a service name (max 256 chars; empty is allowed — defaults to "app").
inline bool check_service_name(const std::string& service) {
    return service.size() <= 256;
}

} // namespace validation
} // namespace tell
