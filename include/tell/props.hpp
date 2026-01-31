// include/tell/props.hpp
// Properties builder — writes JSON bytes directly, no DOM allocation.

#pragma once

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace tell {

// Pre-serialized JSON properties buffer.
//
// Writes JSON bytes directly into a buffer, skipping any intermediate
// JSON DOM. Each string value is safely escaped.
//
// Example:
//   auto props = Props().add("url", "/home").add("status", 200);
class Props {
public:
    Props() { buf_.reserve(256); }

    Props& add(const std::string& key, const std::string& value) {
        begin_field(key);
        buf_.push_back('"');
        write_escaped(value.data(), value.size());
        buf_.push_back('"');
        return *this;
    }

    Props& add(const std::string& key, const char* value) {
        begin_field(key);
        buf_.push_back('"');
        write_escaped(value, std::strlen(value));
        buf_.push_back('"');
        return *this;
    }

    Props& add(const std::string& key, int64_t value) {
        begin_field(key);
        char tmp[24];
        int n = std::snprintf(tmp, sizeof(tmp), "%lld", static_cast<long long>(value));
        if (n > 0) buf_.insert(buf_.end(), tmp, tmp + n);
        return *this;
    }

    Props& add(const std::string& key, int value) {
        begin_field(key);
        char tmp[16];
        int n = std::snprintf(tmp, sizeof(tmp), "%d", value);
        if (n > 0) buf_.insert(buf_.end(), tmp, tmp + n);
        return *this;
    }

    Props& add(const std::string& key, double value) {
        begin_field(key);
        char tmp[64];
        int n = std::snprintf(tmp, sizeof(tmp), "%g", value);
        if (n > 0) buf_.insert(buf_.end(), tmp, tmp + n);
        return *this;
    }

    Props& add(const std::string& key, bool value) {
        begin_field(key);
        if (value) {
            const char* t = "true";
            buf_.insert(buf_.end(), t, t + 4);
        } else {
            const char* f = "false";
            buf_.insert(buf_.end(), f, f + 5);
        }
        return *this;
    }

    // Finish building and return the JSON bytes as "{...}".
    std::vector<uint8_t> to_json_bytes() const {
        std::vector<uint8_t> result;
        result.reserve(buf_.size() + 2);
        result.push_back('{');
        result.insert(result.end(), buf_.begin(), buf_.end());
        result.push_back('}');
        return result;
    }

    bool empty() const noexcept { return count_ == 0; }
    size_t size() const noexcept { return count_; }

    // Access raw inner bytes (without braces), for merging.
    const std::vector<uint8_t>& raw() const noexcept { return buf_; }

private:
    std::vector<uint8_t> buf_;
    size_t count_ = 0;

    void begin_field(const std::string& key) {
        if (count_ > 0) buf_.push_back(',');
        buf_.push_back('"');
        write_escaped(key.data(), key.size());
        buf_.push_back('"');
        buf_.push_back(':');
        count_++;
    }

    static bool needs_escape(char c) {
        return c == '"' || c == '\\' || static_cast<unsigned char>(c) < 0x20;
    }

    void write_escaped(const char* s, size_t len) {
        // Fast path: bulk-copy runs of safe characters.
        size_t i = 0;
        while (i < len) {
            // Find next char that needs escaping
            size_t run_start = i;
            while (i < len && !needs_escape(s[i])) ++i;

            // Bulk-copy the safe run
            if (i > run_start) {
                buf_.insert(buf_.end(),
                    reinterpret_cast<const uint8_t*>(s + run_start),
                    reinterpret_cast<const uint8_t*>(s + i));
            }

            // Escape the special char
            if (i < len) {
                char c = s[i];
                switch (c) {
                    case '"':  buf_.push_back('\\'); buf_.push_back('"'); break;
                    case '\\': buf_.push_back('\\'); buf_.push_back('\\'); break;
                    case '\b': buf_.push_back('\\'); buf_.push_back('b'); break;
                    case '\f': buf_.push_back('\\'); buf_.push_back('f'); break;
                    case '\n': buf_.push_back('\\'); buf_.push_back('n'); break;
                    case '\r': buf_.push_back('\\'); buf_.push_back('r'); break;
                    case '\t': buf_.push_back('\\'); buf_.push_back('t'); break;
                    default: {
                        char hex[7];
                        std::snprintf(hex, sizeof(hex), "\\u%04x", static_cast<unsigned char>(c));
                        buf_.insert(buf_.end(), hex, hex + 6);
                        break;
                    }
                }
                ++i;
            }
        }
    }
};

} // namespace tell
