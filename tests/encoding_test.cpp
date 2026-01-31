// tests/encoding_test.cpp
// Unit tests for hand-written FlatBuffer encoding.

#include <gtest/gtest.h>
#include "encoding.hpp"
#include <cstring>

using namespace tell;
using namespace tell::encoding;

TEST(EncodingTest, EventBasicStructure) {
    EventParams params;
    params.event_type = EventType::Track;
    params.timestamp = 1706000000000;

    std::vector<uint8_t> buf;
    encode_event_into(buf, params);

    // Must have root offset in first 4 bytes
    EXPECT_GE(buf.size(), 4u);

    uint32_t root_offset;
    std::memcpy(&root_offset, buf.data(), 4);
    EXPECT_GT(root_offset, 0u);
    EXPECT_LT(root_offset, buf.size());
}

TEST(EncodingTest, EventWithDeviceId) {
    uint8_t device_id[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};

    EventParams params;
    params.event_type = EventType::Track;
    params.timestamp = 1706000000000;
    params.device_id = device_id;

    std::vector<uint8_t> buf;
    encode_event_into(buf, params);

    // Search for device_id bytes in output
    bool found = false;
    for (size_t i = 0; i + 16 <= buf.size(); i++) {
        if (std::memcmp(&buf[i], device_id, 16) == 0) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found) << "device_id bytes not found in encoded output";
}

TEST(EncodingTest, EventWithEventName) {
    EventParams params;
    params.event_type = EventType::Track;
    params.timestamp = 1706000000000;
    params.event_name = "Page Viewed";
    params.event_name_len = 11;

    std::vector<uint8_t> buf;
    encode_event_into(buf, params);

    // Search for event name string in output
    std::string name(buf.begin(), buf.end());
    EXPECT_NE(name.find("Page Viewed"), std::string::npos);
}

TEST(EncodingTest, EventWithPayload) {
    const char* json = "{\"url\":\"/home\"}";
    EventParams params;
    params.event_type = EventType::Track;
    params.timestamp = 1706000000000;
    params.payload = reinterpret_cast<const uint8_t*>(json);
    params.payload_len = std::strlen(json);

    std::vector<uint8_t> buf;
    encode_event_into(buf, params);

    std::string output(buf.begin(), buf.end());
    EXPECT_NE(output.find("/home"), std::string::npos);
}

TEST(EncodingTest, EventDataMultipleEvents) {
    std::vector<EventParams> events;

    EventParams e1;
    e1.event_type = EventType::Track;
    e1.timestamp = 1000;
    e1.event_name = "Event1";
    e1.event_name_len = 6;
    events.push_back(e1);

    EventParams e2;
    e2.event_type = EventType::Identify;
    e2.timestamp = 2000;
    events.push_back(e2);

    std::vector<uint8_t> buf;
    size_t start = encode_event_data_into(buf, events);

    EXPECT_EQ(start, 0u);
    EXPECT_GT(buf.size(), 0u);

    // Verify root offset
    uint32_t root_offset;
    std::memcpy(&root_offset, buf.data() + start, 4);
    EXPECT_GT(root_offset, 0u);
}

TEST(EncodingTest, LogEntryBasic) {
    LogEntryParams params;
    params.event_type = LogEventType::Log;
    params.level = LogLevel::Error;
    params.timestamp = 1706000000000;

    const char* svc = "api";
    params.service = svc;
    params.service_len = 3;

    std::vector<uint8_t> buf;
    encode_log_entry_into(buf, params);

    EXPECT_GE(buf.size(), 4u);
    std::string output(buf.begin(), buf.end());
    EXPECT_NE(output.find("api"), std::string::npos);
}

TEST(EncodingTest, LogDataMultipleEntries) {
    std::vector<LogEntryParams> logs;

    LogEntryParams l1;
    l1.level = LogLevel::Info;
    l1.timestamp = 1000;
    const char* svc1 = "auth";
    l1.service = svc1;
    l1.service_len = 4;
    logs.push_back(l1);

    LogEntryParams l2;
    l2.level = LogLevel::Error;
    l2.timestamp = 2000;
    logs.push_back(l2);

    std::vector<uint8_t> buf;
    size_t start = encode_log_data_into(buf, logs);
    EXPECT_EQ(start, 0u);
    EXPECT_GT(buf.size(), 0u);
}

TEST(EncodingTest, BatchEncoding) {
    uint8_t api_key[16] = {0xa1, 0xb2, 0xc3, 0xd4, 0xe5, 0xf6, 0x07, 0x18,
                           0x29, 0x3a, 0x4b, 0x5c, 0x6d, 0x7e, 0x8f, 0x90};
    uint8_t data[] = {1, 2, 3, 4};

    BatchParams params;
    params.api_key = api_key;
    params.schema_type = SchemaType::Event;
    params.version = 100;
    params.batch_id = 42;
    params.data = data;
    params.data_len = 4;

    std::vector<uint8_t> buf;
    encode_batch_into(buf, params);

    EXPECT_GT(buf.size(), 0u);

    // Verify api_key bytes are present
    bool found = false;
    for (size_t i = 0; i + 16 <= buf.size(); i++) {
        if (std::memcmp(&buf[i], api_key, 16) == 0) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found) << "api_key bytes not found in batch output";
}

TEST(EncodingTest, BatchDefaultVersion) {
    uint8_t api_key[16] = {};
    uint8_t data[] = {0};

    BatchParams params;
    params.api_key = api_key;
    params.schema_type = SchemaType::Log;
    params.version = 0; // should default to 100
    params.data = data;
    params.data_len = 1;

    std::vector<uint8_t> buf;
    encode_batch_into(buf, params);
    EXPECT_GT(buf.size(), 0u);
}
