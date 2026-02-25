// src/encoding.hpp
// Hand-written FlatBuffer encoding — zero external dependencies.
// Ported from Rust tell-encoding crate.

#pragma once

#include "tell/types.hpp"
#include <cstdint>
#include <cstring>
#include <vector>

namespace tell {
namespace encoding {

static constexpr size_t API_KEY_LENGTH = 16;
static constexpr size_t UUID_LENGTH = 16;
static constexpr uint8_t DEFAULT_VERSION = 100;

// --- Helper functions (matching Rust helpers.rs) ---

inline void write_u16(std::vector<uint8_t>& buf, uint16_t value) {
    buf.push_back(static_cast<uint8_t>(value));
    buf.push_back(static_cast<uint8_t>(value >> 8));
}

inline void write_u32(std::vector<uint8_t>& buf, uint32_t value) {
    buf.push_back(static_cast<uint8_t>(value));
    buf.push_back(static_cast<uint8_t>(value >> 8));
    buf.push_back(static_cast<uint8_t>(value >> 16));
    buf.push_back(static_cast<uint8_t>(value >> 24));
}

inline void write_i32(std::vector<uint8_t>& buf, int32_t value) {
    write_u32(buf, static_cast<uint32_t>(value));
}

inline void write_u64(std::vector<uint8_t>& buf, uint64_t value) {
    for (int i = 0; i < 8; i++) {
        buf.push_back(static_cast<uint8_t>(value >> (i * 8)));
    }
}

inline void align4(std::vector<uint8_t>& buf) {
    while (buf.size() % 4 != 0) buf.push_back(0);
}

// Write [u32 length][data] and return the start position.
inline size_t write_byte_vector(std::vector<uint8_t>& buf, const uint8_t* data, size_t len) {
    size_t start = buf.size();
    write_u32(buf, len > UINT32_MAX ? 0 : static_cast<uint32_t>(len));
    if (data && len > 0) buf.insert(buf.end(), data, data + len);
    return start;
}

// Write [u32 length][data][null] and return the start position.
inline size_t write_string(std::vector<uint8_t>& buf, const char* s, size_t len) {
    size_t start = buf.size();
    write_u32(buf, len > UINT32_MAX ? 0 : static_cast<uint32_t>(len));
    if (s && len > 0) {
        buf.insert(buf.end(), reinterpret_cast<const uint8_t*>(s),
                   reinterpret_cast<const uint8_t*>(s) + len);
    }
    buf.push_back(0); // null terminator
    return start;
}

inline void patch_offset(std::vector<uint8_t>& buf, size_t offset_pos, size_t target) {
    uint32_t rel = static_cast<uint32_t>(target - offset_pos);
    std::memcpy(&buf[offset_pos], &rel, 4);
}

inline void patch_u32(std::vector<uint8_t>& buf, size_t pos, uint32_t value) {
    std::memcpy(&buf[pos], &value, 4);
}

// --- Event encoding (matching Rust event.rs) ---

struct EventParams {
    EventType event_type = EventType::Unknown;
    uint64_t timestamp = 0;
    const char* service = nullptr;
    size_t service_len = 0;
    const uint8_t* device_id = nullptr;   // 16 bytes or nullptr
    const uint8_t* session_id = nullptr;  // 16 bytes or nullptr
    const char* event_name = nullptr;
    size_t event_name_len = 0;
    const uint8_t* payload = nullptr;
    size_t payload_len = 0;
};

// Encode a single event into buf (standalone FlatBuffer with root offset).
inline void encode_event_into(std::vector<uint8_t>& buf, const EventParams& params) {
    bool has_device_id = params.device_id != nullptr;
    bool has_session_id = params.session_id != nullptr;
    bool has_service = params.service != nullptr;
    bool has_event_name = params.event_name != nullptr;
    bool has_payload = params.payload != nullptr && params.payload_len > 0;

    // VTable: size(u16) + table_size(u16) + 7 field slots = 18 bytes (+ 2 pad)
    // Table: soffset(4) + 4 offsets(16) + timestamp(8) + event_type(1) + pad(3) + service_off(4) = 36

    size_t root_pos = buf.size();
    buf.insert(buf.end(), 4, 0); // root offset placeholder

    // VTable
    size_t vtable_start = buf.size();
    write_u16(buf, 18);                                          // vtable_size = 4 + 7*2
    write_u16(buf, 36);                                          // table_size = 4 + 32
    write_u16(buf, 28);                                          // field 0: event_type at +28
    write_u16(buf, 20);                                          // field 1: timestamp at +20
    write_u16(buf, has_service ? 32 : 0);                        // field 2: service at +32
    write_u16(buf, has_device_id ? 4 : 0);                       // field 3: device_id
    write_u16(buf, has_session_id ? 8 : 0);                      // field 4: session_id
    write_u16(buf, has_event_name ? 12 : 0);                     // field 5: event_name
    write_u16(buf, has_payload ? 16 : 0);                        // field 6: payload
    buf.insert(buf.end(), 2, 0);                                 // vtable alignment padding

    // Table
    size_t table_start = buf.size();
    write_i32(buf, static_cast<int32_t>(table_start - vtable_start)); // soffset

    size_t device_id_off_pos = buf.size();
    write_u32(buf, 0);
    size_t session_id_off_pos = buf.size();
    write_u32(buf, 0);
    size_t event_name_off_pos = buf.size();
    write_u32(buf, 0);
    size_t payload_off_pos = buf.size();
    write_u32(buf, 0);

    write_u64(buf, params.timestamp);
    buf.push_back(static_cast<uint8_t>(params.event_type));
    buf.insert(buf.end(), 3, 0); // padding

    size_t service_off_pos = buf.size();
    write_u32(buf, 0);

    // Vectors and strings
    align4(buf);

    size_t device_id_start = 0;
    if (has_device_id) {
        device_id_start = write_byte_vector(buf, params.device_id, UUID_LENGTH);
        align4(buf);
    }

    size_t session_id_start = 0;
    if (has_session_id) {
        session_id_start = write_byte_vector(buf, params.session_id, UUID_LENGTH);
        align4(buf);
    }

    size_t service_start = 0;
    if (has_service) {
        service_start = write_string(buf, params.service, params.service_len);
        align4(buf);
    }

    size_t event_name_start = 0;
    if (has_event_name) {
        event_name_start = write_string(buf, params.event_name, params.event_name_len);
        align4(buf);
    }

    size_t payload_start = 0;
    if (has_payload) {
        payload_start = write_byte_vector(buf, params.payload, params.payload_len);
    }

    // Root offset (relative to root_pos)
    patch_u32(buf, root_pos, static_cast<uint32_t>(table_start - root_pos));

    if (has_device_id)  patch_offset(buf, device_id_off_pos, device_id_start);
    if (has_session_id) patch_offset(buf, session_id_off_pos, session_id_start);
    if (has_service)    patch_offset(buf, service_off_pos, service_start);
    if (has_event_name) patch_offset(buf, event_name_off_pos, event_name_start);
    if (has_payload)    patch_offset(buf, payload_off_pos, payload_start);
}

// Encode EventData (vector of events) into buf. Returns start position.
inline size_t encode_event_data_into(std::vector<uint8_t>& buf, const std::vector<EventParams>& events) {
    size_t data_start = buf.size();
    size_t count = events.size();

    // Root offset placeholder
    size_t root_pos = buf.size();
    buf.insert(buf.end(), 4, 0);

    // VTable (6 bytes + 2 pad)
    size_t vtable_start = buf.size();
    write_u16(buf, 6);  // vtable_size
    write_u16(buf, 8);  // table_size
    write_u16(buf, 4);  // field 0: events at table+4
    buf.insert(buf.end(), 2, 0); // align vtable

    // Table
    size_t table_start = buf.size();
    write_i32(buf, static_cast<int32_t>(table_start - vtable_start));

    size_t events_off_pos = buf.size();
    write_u32(buf, 0);

    align4(buf);

    // Events vector
    size_t events_vec_start = buf.size();
    write_u32(buf, static_cast<uint32_t>(count));

    size_t offsets_start = buf.size();
    for (size_t i = 0; i < count; i++) {
        write_u32(buf, 0); // placeholder
    }

    align4(buf);

    // Encode events directly
    std::vector<size_t> table_positions;
    table_positions.reserve(count);

    for (const auto& params : events) {
        align4(buf);
        size_t event_start = buf.size();
        encode_event_into(buf, params);

        uint32_t root_offset;
        std::memcpy(&root_offset, &buf[event_start], 4);
        table_positions.push_back(event_start + root_offset);
    }

    // Patch offsets
    for (size_t i = 0; i < count; i++) {
        patch_offset(buf, offsets_start + i * 4, table_positions[i]);
    }

    patch_offset(buf, events_off_pos, events_vec_start);
    patch_u32(buf, root_pos, static_cast<uint32_t>(table_start - data_start));

    return data_start;
}

// --- Log encoding (matching Rust log.rs) ---

struct LogEntryParams {
    LogEventType event_type = LogEventType::Log;
    const uint8_t* session_id = nullptr;  // 16 bytes or nullptr
    LogLevel level = LogLevel::Info;
    uint64_t timestamp = 0;
    const char* source = nullptr;
    size_t source_len = 0;
    const char* service = nullptr;
    size_t service_len = 0;
    const uint8_t* payload = nullptr;
    size_t payload_len = 0;
};

// Encode a single log entry into buf.
inline void encode_log_entry_into(std::vector<uint8_t>& buf, const LogEntryParams& params) {
    bool has_session_id = params.session_id != nullptr;
    bool has_source = params.source != nullptr;
    bool has_service = params.service != nullptr;
    bool has_payload = params.payload != nullptr && params.payload_len > 0;

    // VTable: size(u16) + table_size(u16) + 7 field slots = 18 bytes (+ 2 pad)
    // Table: soffset(4) + 4 offsets(16) + timestamp(8) + event_type(1) + level(1) + pad(2) = 32

    size_t root_pos = buf.size();
    buf.insert(buf.end(), 4, 0);

    size_t vtable_start = buf.size();
    write_u16(buf, 18);                                          // vtable_size = 4 + 7*2
    write_u16(buf, 32);                                          // table_size = 4 + 28
    write_u16(buf, 28);                                          // field 0: event_type at +28
    write_u16(buf, has_session_id ? 4 : 0);                      // field 1: session_id
    write_u16(buf, 29);                                          // field 2: level at +29
    write_u16(buf, 20);                                          // field 3: timestamp at +20
    write_u16(buf, has_source ? 8 : 0);                          // field 4: source
    write_u16(buf, has_service ? 12 : 0);                        // field 5: service
    write_u16(buf, has_payload ? 16 : 0);                        // field 6: payload
    buf.insert(buf.end(), 2, 0); // align vtable to 4 bytes

    size_t table_start = buf.size();
    write_i32(buf, static_cast<int32_t>(table_start - vtable_start));

    size_t session_id_off_pos = buf.size();
    write_u32(buf, 0);
    size_t source_off_pos = buf.size();
    write_u32(buf, 0);
    size_t service_off_pos = buf.size();
    write_u32(buf, 0);
    size_t payload_off_pos = buf.size();
    write_u32(buf, 0);

    write_u64(buf, params.timestamp);
    buf.push_back(static_cast<uint8_t>(params.event_type));
    buf.push_back(static_cast<uint8_t>(params.level));
    buf.insert(buf.end(), 2, 0); // padding

    align4(buf);

    size_t session_id_start = 0;
    if (has_session_id) {
        session_id_start = write_byte_vector(buf, params.session_id, UUID_LENGTH);
        align4(buf);
    }

    size_t source_start = 0;
    if (has_source) {
        source_start = write_string(buf, params.source, params.source_len);
        align4(buf);
    }

    size_t service_start = 0;
    if (has_service) {
        service_start = write_string(buf, params.service, params.service_len);
        align4(buf);
    }

    size_t payload_start = 0;
    if (has_payload) {
        payload_start = write_byte_vector(buf, params.payload, params.payload_len);
    }

    patch_u32(buf, root_pos, static_cast<uint32_t>(table_start - root_pos));

    if (has_session_id) patch_offset(buf, session_id_off_pos, session_id_start);
    if (has_source)     patch_offset(buf, source_off_pos, source_start);
    if (has_service)    patch_offset(buf, service_off_pos, service_start);
    if (has_payload)    patch_offset(buf, payload_off_pos, payload_start);
}

// Encode LogData (vector of log entries) into buf. Returns start position.
inline size_t encode_log_data_into(std::vector<uint8_t>& buf, const std::vector<LogEntryParams>& logs) {
    size_t data_start = buf.size();
    size_t count = logs.size();

    size_t root_pos = buf.size();
    buf.insert(buf.end(), 4, 0);

    size_t vtable_start = buf.size();
    write_u16(buf, 6);
    write_u16(buf, 8);
    write_u16(buf, 4);
    buf.insert(buf.end(), 2, 0);

    size_t table_start = buf.size();
    write_i32(buf, static_cast<int32_t>(table_start - vtable_start));

    size_t logs_off_pos = buf.size();
    write_u32(buf, 0);

    align4(buf);

    size_t logs_vec_start = buf.size();
    write_u32(buf, static_cast<uint32_t>(count));

    size_t offsets_start = buf.size();
    for (size_t i = 0; i < count; i++) {
        write_u32(buf, 0);
    }

    align4(buf);

    std::vector<size_t> table_positions;
    table_positions.reserve(count);

    for (const auto& params : logs) {
        align4(buf);
        size_t entry_start = buf.size();
        encode_log_entry_into(buf, params);

        uint32_t root_offset;
        std::memcpy(&root_offset, &buf[entry_start], 4);
        table_positions.push_back(entry_start + root_offset);
    }

    for (size_t i = 0; i < count; i++) {
        patch_offset(buf, offsets_start + i * 4, table_positions[i]);
    }

    patch_offset(buf, logs_off_pos, logs_vec_start);
    patch_u32(buf, root_pos, static_cast<uint32_t>(table_start - data_start));

    return data_start;
}

// --- Batch encoding (matching Rust batch.rs) ---

struct BatchParams {
    const uint8_t* api_key = nullptr;   // 16 bytes
    SchemaType schema_type = SchemaType::Event;
    uint8_t version = DEFAULT_VERSION;
    uint64_t batch_id = 0;
    const uint8_t* data = nullptr;
    size_t data_len = 0;
};

inline void encode_batch_into(std::vector<uint8_t>& buf, const BatchParams& params) {
    bool has_batch_id = params.batch_id != 0;
    uint8_t version = params.version == 0 ? DEFAULT_VERSION : params.version;

    size_t base = buf.size();

    // Root offset placeholder
    buf.insert(buf.end(), 4, 0);

    // VTable: 4 + 6*2 = 16 bytes
    size_t vtable_start = buf.size();
    write_u16(buf, 16);  // vtable_size
    write_u16(buf, 32);  // table_size = 4 + 28
    write_u16(buf, 4);   // field 0: api_key at table+4
    write_u16(buf, 24);  // field 1: schema_type at table+24
    write_u16(buf, 25);  // field 2: version at table+25
    write_u16(buf, has_batch_id ? 16 : 0); // field 3: batch_id at table+16
    write_u16(buf, 8);   // field 4: data at table+8
    write_u16(buf, 0);   // field 5: source_ip (not used)

    // Table
    size_t table_start = buf.size();
    write_i32(buf, static_cast<int32_t>(table_start - vtable_start));

    size_t api_key_off_pos = buf.size();
    write_u32(buf, 0);

    size_t data_off_pos = buf.size();
    write_u32(buf, 0);

    write_u32(buf, 0); // source_ip placeholder (unused)

    write_u64(buf, params.batch_id);

    buf.push_back(static_cast<uint8_t>(params.schema_type));
    buf.push_back(version);
    buf.insert(buf.end(), 2, 0); // padding

    // Vectors
    align4(buf);

    size_t api_key_start = write_byte_vector(buf, params.api_key, API_KEY_LENGTH);
    align4(buf);

    size_t data_start = write_byte_vector(buf, params.data, params.data_len);

    // Patch offsets
    patch_u32(buf, base, static_cast<uint32_t>(table_start));
    patch_offset(buf, api_key_off_pos, api_key_start);
    patch_offset(buf, data_off_pos, data_start);
}

} // namespace encoding
} // namespace tell
