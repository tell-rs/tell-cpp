// src/client.cpp
// Tell client implementation — full spec API.

#include "tell/client.hpp"
#include "validation.hpp"
#include "worker.hpp"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <map>
#include <mutex>
#include <random>
#include <shared_mutex>

namespace tell {

// Generate a v4 UUID as 16 bytes.
static void generate_uuid(uint8_t out[16]) {
    static thread_local std::mt19937_64 gen(std::random_device{}());
    std::uniform_int_distribution<uint64_t> dist;

    uint64_t a = dist(gen);
    uint64_t b = dist(gen);
    std::memcpy(out, &a, 8);
    std::memcpy(out + 8, &b, 8);

    // Set version 4 and variant bits
    out[6] = (out[6] & 0x0F) | 0x40; // version 4
    out[8] = (out[8] & 0x3F) | 0x80; // variant 1
}

static uint64_t now_ms() {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count()
    );
}

// Append raw bytes from a string literal to a buffer.
static inline void append_lit(std::vector<uint8_t>& buf, const char* s, size_t n) {
    buf.insert(buf.end(), reinterpret_cast<const uint8_t*>(s),
               reinterpret_cast<const uint8_t*>(s) + n);
}

// Append a string value with JSON escaping (bulk-copy, matching Props::write_escaped).
static inline bool needs_escape(char c) {
    return c == '"' || c == '\\' || static_cast<unsigned char>(c) < 0x20;
}

static inline void append_escaped(std::vector<uint8_t>& buf, const std::string& s) {
    size_t i = 0;
    while (i < s.size()) {
        size_t run_start = i;
        while (i < s.size() && !needs_escape(s[i])) ++i;
        if (i > run_start) {
            buf.insert(buf.end(),
                reinterpret_cast<const uint8_t*>(s.data() + run_start),
                reinterpret_cast<const uint8_t*>(s.data() + i));
        }
        if (i < s.size()) {
            char c = s[i];
            switch (c) {
                case '"':  buf.push_back('\\'); buf.push_back('"'); break;
                case '\\': buf.push_back('\\'); buf.push_back('\\'); break;
                case '\b': buf.push_back('\\'); buf.push_back('b'); break;
                case '\f': buf.push_back('\\'); buf.push_back('f'); break;
                case '\n': buf.push_back('\\'); buf.push_back('n'); break;
                case '\r': buf.push_back('\\'); buf.push_back('r'); break;
                case '\t': buf.push_back('\\'); buf.push_back('t'); break;
                default: {
                    char hex[7];
                    std::snprintf(hex, sizeof(hex), "\\u%04x", static_cast<unsigned char>(c));
                    buf.insert(buf.end(), hex, hex + 6);
                    break;
                }
            }
            ++i;
        }
    }
}

// Parse Props raw bytes into a map, upserting entries.
// Props raw format: "key1":value1,"key2":value2,...
static void parse_props_into_map(
    const std::vector<uint8_t>& raw,
    std::map<std::string, std::vector<uint8_t>>& map)
{
    size_t i = 0;
    size_t n = raw.size();
    while (i < n) {
        if (raw[i] != '"') break;
        i++; // skip opening quote

        // Read key, unescaping
        std::string key;
        while (i < n && raw[i] != '"') {
            if (raw[i] == '\\' && i + 1 < n) {
                char esc = static_cast<char>(raw[i + 1]);
                switch (esc) {
                    case '"':  key.push_back('"'); break;
                    case '\\': key.push_back('\\'); break;
                    case '/':  key.push_back('/'); break;
                    case 'b':  key.push_back('\b'); break;
                    case 'f':  key.push_back('\f'); break;
                    case 'n':  key.push_back('\n'); break;
                    case 'r':  key.push_back('\r'); break;
                    case 't':  key.push_back('\t'); break;
                    default:   key.push_back('\\'); key.push_back(esc); break;
                }
                i += 2;
            } else {
                key.push_back(static_cast<char>(raw[i]));
                i++;
            }
        }
        if (i < n) i++; // skip closing quote
        if (i < n && raw[i] == ':') i++; // skip colon

        // Read value (raw JSON bytes)
        size_t value_start = i;
        if (i < n && raw[i] == '"') {
            i++; // skip opening quote
            while (i < n) {
                if (raw[i] == '\\' && i + 1 < n) {
                    i += 2;
                } else if (raw[i] == '"') {
                    i++;
                    break;
                } else {
                    i++;
                }
            }
        } else {
            while (i < n && raw[i] != ',') i++;
        }

        map[std::move(key)] = std::vector<uint8_t>(
            raw.begin() + static_cast<ptrdiff_t>(value_start),
            raw.begin() + static_cast<ptrdiff_t>(i));

        if (i < n && raw[i] == ',') i++;
    }
}

// Build a JSON payload by merging key:value with optional super props and event props.
// Super props come before event props so event-specific keys override (last-key-wins).
static std::vector<uint8_t> merge_json_payload(
    const char* key_colon, size_t key_colon_len,
    const std::string& value,
    const std::vector<uint8_t>* props_json = nullptr,
    const std::vector<uint8_t>* super_props_raw = nullptr)
{
    const uint8_t* props_inner = nullptr;
    size_t props_inner_len = 0;
    if (props_json && props_json->size() > 2 && (*props_json)[0] == '{') {
        props_inner = props_json->data() + 1;
        props_inner_len = props_json->size() - 1;
    }

    std::vector<uint8_t> buf;
    buf.reserve(2 + key_colon_len + value.size() + 2
        + (super_props_raw ? super_props_raw->size() + 1 : 0)
        + (props_inner ? props_inner_len + 1 : 0));

    buf.push_back('{');
    append_lit(buf, key_colon, key_colon_len);

    buf.push_back('"');
    append_escaped(buf, value);
    buf.push_back('"');

    if (super_props_raw && !super_props_raw->empty()) {
        buf.push_back(',');
        buf.insert(buf.end(), super_props_raw->begin(), super_props_raw->end());
    }

    if (props_inner) {
        buf.push_back(',');
        buf.insert(buf.end(), props_inner, props_inner + props_inner_len);
    } else {
        buf.push_back('}');
    }

    return buf;
}

// ---

struct Tell::Inner {
    uint8_t device_id[16] = {};
    mutable std::shared_mutex session_mutex;
    uint8_t session_id[16] = {};
    mutable std::shared_mutex super_props_mutex;
    std::map<std::string, std::vector<uint8_t>> super_props_map;
    TellConfig::ErrorCallback on_error;
    std::unique_ptr<Worker> worker;
    std::chrono::milliseconds close_timeout;

    void report_error(TellError err) const {
        if (on_error) {
            on_error(err);
        }
    }

    void read_session_id(uint8_t out[16]) const {
        std::shared_lock<std::shared_mutex> lock(session_mutex);
        std::memcpy(out, session_id, 16);
    }

    std::vector<uint8_t> read_super_props() const {
        std::shared_lock<std::shared_mutex> lock(super_props_mutex);
        if (super_props_map.empty()) return {};
        std::vector<uint8_t> result;
        bool first = true;
        for (const auto& [key, value] : super_props_map) {
            if (!first) result.push_back(',');
            first = false;
            result.push_back('"');
            append_escaped(result, key);
            result.push_back('"');
            result.push_back(':');
            result.insert(result.end(), value.begin(), value.end());
        }
        return result;
    }
};

Tell::Tell(TellConfig config) : inner_(std::make_unique<Inner>()) {
    generate_uuid(inner_->device_id);
    generate_uuid(inner_->session_id);
    inner_->on_error = config.on_error();
    inner_->close_timeout = config.close_timeout();
    inner_->worker = std::make_unique<Worker>(std::move(config));
}

Tell::~Tell() = default;
Tell::Tell(Tell&&) noexcept = default;
Tell& Tell::operator=(Tell&&) noexcept = default;

std::unique_ptr<Tell> Tell::create(TellConfig config) {
    return std::unique_ptr<Tell>(new Tell(std::move(config)));
}

// --- Events ---

void Tell::track(const std::string& user_id, const std::string& event_name,
                 const Props& properties) {
    if (!validation::check_user_id(user_id)) {
        inner_->report_error(TellError::validation("userId", "is required"));
        return;
    }
    if (!validation::check_event_name(event_name)) {
        inner_->report_error(TellError::validation("eventName",
            event_name.empty() ? "is required" : "must be at most 256 characters"));
        return;
    }

    auto props_json = properties.empty() ? std::vector<uint8_t>{} : properties.to_json_bytes();
    auto sp_raw = inner_->read_super_props();
    auto payload = merge_json_payload("\"user_id\":", 10, user_id,
                                       props_json.empty() ? nullptr : &props_json,
                                       sp_raw.empty() ? nullptr : &sp_raw);

    QueuedEvent event;
    event.event_type = EventType::Track;
    event.timestamp = now_ms();
    std::memcpy(event.device_id, inner_->device_id, 16);
    inner_->read_session_id(event.session_id);
    event.event_name = event_name;
    event.payload = std::move(payload);

    inner_->worker->send_event(std::move(event));
}

void Tell::identify(const std::string& user_id, const Props& traits) {
    if (!validation::check_user_id(user_id)) {
        inner_->report_error(TellError::validation("userId", "is required"));
        return;
    }

    std::vector<uint8_t> buf;
    buf.reserve(64 + user_id.size());
    append_lit(buf, "{\"user_id\":\"", 12);
    append_escaped(buf, user_id);
    buf.push_back('"');

    if (!traits.empty()) {
        append_lit(buf, ",\"traits\":", 10);
        auto traits_json = traits.to_json_bytes();
        buf.insert(buf.end(), traits_json.begin(), traits_json.end());
    }
    buf.push_back('}');

    QueuedEvent event;
    event.event_type = EventType::Identify;
    event.timestamp = now_ms();
    std::memcpy(event.device_id, inner_->device_id, 16);
    inner_->read_session_id(event.session_id);
    event.payload = std::move(buf);

    inner_->worker->send_event(std::move(event));
}

void Tell::group(const std::string& user_id, const std::string& group_id,
                 const Props& properties) {
    if (!validation::check_user_id(user_id)) {
        inner_->report_error(TellError::validation("userId", "is required"));
        return;
    }
    if (group_id.empty()) {
        inner_->report_error(TellError::validation("groupId", "is required"));
        return;
    }

    auto props_json = properties.empty() ? std::vector<uint8_t>{} : properties.to_json_bytes();
    auto sp_raw = inner_->read_super_props();

    std::vector<uint8_t> buf;
    buf.reserve(80 + user_id.size() + group_id.size() + sp_raw.size() + props_json.size());
    buf.push_back('{');

    append_lit(buf, "\"group_id\":\"", 12);
    append_escaped(buf, group_id);
    buf.push_back('"');

    append_lit(buf, ",\"user_id\":\"", 12);
    append_escaped(buf, user_id);
    buf.push_back('"');

    if (!sp_raw.empty()) {
        buf.push_back(',');
        buf.insert(buf.end(), sp_raw.begin(), sp_raw.end());
    }

    if (!props_json.empty() && props_json.size() > 2 && props_json[0] == '{') {
        buf.push_back(',');
        buf.insert(buf.end(), props_json.begin() + 1, props_json.end());
    } else {
        buf.push_back('}');
    }

    QueuedEvent event;
    event.event_type = EventType::Group;
    event.timestamp = now_ms();
    std::memcpy(event.device_id, inner_->device_id, 16);
    inner_->read_session_id(event.session_id);
    event.payload = std::move(buf);

    inner_->worker->send_event(std::move(event));
}

void Tell::revenue(const std::string& user_id, double amount,
                   const std::string& currency, const std::string& order_id,
                   const Props& properties) {
    if (!validation::check_user_id(user_id)) {
        inner_->report_error(TellError::validation("userId", "is required"));
        return;
    }
    if (amount <= 0.0) {
        inner_->report_error(TellError::validation("amount", "must be positive"));
        return;
    }
    if (currency.empty()) {
        inner_->report_error(TellError::validation("currency", "is required"));
        return;
    }
    if (order_id.empty()) {
        inner_->report_error(TellError::validation("orderId", "is required"));
        return;
    }

    auto props_json = properties.empty() ? std::vector<uint8_t>{} : properties.to_json_bytes();
    auto sp_raw = inner_->read_super_props();

    char amount_str[64];
    int amount_len = std::snprintf(amount_str, sizeof(amount_str), "%g", amount);

    std::vector<uint8_t> buf;
    buf.reserve(120 + user_id.size() + currency.size() + order_id.size()
                + sp_raw.size() + props_json.size());
    buf.push_back('{');

    append_lit(buf, "\"user_id\":\"", 11);
    append_escaped(buf, user_id);
    buf.push_back('"');

    append_lit(buf, ",\"amount\":", 10);
    if (amount_len > 0) buf.insert(buf.end(), amount_str, amount_str + amount_len);

    append_lit(buf, ",\"currency\":\"", 13);
    append_escaped(buf, currency);
    buf.push_back('"');

    append_lit(buf, ",\"order_id\":\"", 13);
    append_escaped(buf, order_id);
    buf.push_back('"');

    if (!sp_raw.empty()) {
        buf.push_back(',');
        buf.insert(buf.end(), sp_raw.begin(), sp_raw.end());
    }

    if (!props_json.empty() && props_json.size() > 2 && props_json[0] == '{') {
        buf.push_back(',');
        buf.insert(buf.end(), props_json.begin() + 1, props_json.end());
    } else {
        buf.push_back('}');
    }

    QueuedEvent event;
    event.event_type = EventType::Track;
    event.timestamp = now_ms();
    std::memcpy(event.device_id, inner_->device_id, 16);
    inner_->read_session_id(event.session_id);
    event.event_name = "Order Completed";
    event.payload = std::move(buf);

    inner_->worker->send_event(std::move(event));
}

void Tell::alias(const std::string& previous_id, const std::string& user_id) {
    if (previous_id.empty()) {
        inner_->report_error(TellError::validation("previousId", "is required"));
        return;
    }
    if (!validation::check_user_id(user_id)) {
        inner_->report_error(TellError::validation("userId", "is required"));
        return;
    }

    std::vector<uint8_t> buf;
    buf.reserve(40 + previous_id.size() + user_id.size());
    append_lit(buf, "{\"previous_id\":\"", 16);
    append_escaped(buf, previous_id);
    append_lit(buf, "\",\"user_id\":\"", 13);
    append_escaped(buf, user_id);
    buf.push_back('"');
    buf.push_back('}');

    QueuedEvent event;
    event.event_type = EventType::Alias;
    event.timestamp = now_ms();
    std::memcpy(event.device_id, inner_->device_id, 16);
    inner_->read_session_id(event.session_id);
    event.payload = std::move(buf);

    inner_->worker->send_event(std::move(event));
}

// --- Logging ---

void Tell::log(LogLevel level, const std::string& message,
               const std::string& service, const Props& data) {
    if (!validation::check_log_message(message)) {
        inner_->report_error(TellError::validation("message",
            message.empty() ? "is required" : "must be at most 65536 characters"));
        return;
    }
    if (!validation::check_service_name(service)) {
        inner_->report_error(TellError::validation("service", "must be at most 256 characters"));
        return;
    }

    auto data_json = data.empty() ? std::vector<uint8_t>{} : data.to_json_bytes();
    auto payload = merge_json_payload("\"message\":", 10, message,
                                       data_json.empty() ? nullptr : &data_json);

    QueuedLog entry;
    entry.level = level;
    entry.timestamp = now_ms();
    inner_->read_session_id(entry.session_id);
    entry.service = service;
    entry.payload = std::move(payload);

    inner_->worker->send_log(std::move(entry));
}

void Tell::log_emergency(const std::string& m, const std::string& s, const Props& d) { log(LogLevel::Emergency, m, s, d); }
void Tell::log_alert(const std::string& m, const std::string& s, const Props& d)     { log(LogLevel::Alert, m, s, d); }
void Tell::log_critical(const std::string& m, const std::string& s, const Props& d)  { log(LogLevel::Critical, m, s, d); }
void Tell::log_error(const std::string& m, const std::string& s, const Props& d)     { log(LogLevel::Error, m, s, d); }
void Tell::log_warning(const std::string& m, const std::string& s, const Props& d)   { log(LogLevel::Warning, m, s, d); }
void Tell::log_notice(const std::string& m, const std::string& s, const Props& d)    { log(LogLevel::Notice, m, s, d); }
void Tell::log_info(const std::string& m, const std::string& s, const Props& d)      { log(LogLevel::Info, m, s, d); }
void Tell::log_debug(const std::string& m, const std::string& s, const Props& d)     { log(LogLevel::Debug, m, s, d); }
void Tell::log_trace(const std::string& m, const std::string& s, const Props& d)     { log(LogLevel::Trace, m, s, d); }

// --- Super Properties ---

void Tell::register_props(const Props& properties) {
    if (properties.empty()) return;
    std::unique_lock<std::shared_mutex> lock(inner_->super_props_mutex);
    parse_props_into_map(properties.raw(), inner_->super_props_map);
}

void Tell::unregister(const std::string& key) {
    std::unique_lock<std::shared_mutex> lock(inner_->super_props_mutex);
    inner_->super_props_map.erase(key);
}

// --- Session ---

void Tell::reset_session() {
    std::unique_lock<std::shared_mutex> lock(inner_->session_mutex);
    generate_uuid(inner_->session_id);
}

// --- Lifecycle ---

void Tell::flush() {
    auto f = inner_->worker->send_flush();
    f.wait_for(inner_->close_timeout);
}

void Tell::close() {
    auto f = inner_->worker->send_close();
    f.wait_for(inner_->close_timeout);
}

} // namespace tell
