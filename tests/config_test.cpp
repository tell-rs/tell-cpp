// tests/config_test.cpp
// Unit tests for TellConfig and builder.

#include <gtest/gtest.h>
#include "tell/config.hpp"

using namespace tell;

TEST(ConfigTest, ValidApiKey) {
    auto config = TellConfig::production("feed1e11feed1e11feed1e11feed1e11");
    EXPECT_EQ(config.endpoint(), "collect.tell.rs:50000");
    EXPECT_EQ(config.batch_size(), 100u);
    EXPECT_EQ(config.max_retries(), 3u);
}

TEST(ConfigTest, InvalidApiKeyLength) {
    EXPECT_THROW(TellConfig::production("tooshort"), TellError);
}

TEST(ConfigTest, InvalidApiKeyHex) {
    EXPECT_THROW(TellConfig::production("zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz"), TellError);
}

TEST(ConfigTest, EmptyApiKey) {
    EXPECT_THROW(TellConfig::production(""), TellError);
}

TEST(ConfigTest, DevelopmentPreset) {
    auto config = TellConfig::development("feed1e11feed1e11feed1e11feed1e11");
    EXPECT_EQ(config.endpoint(), "localhost:50000");
    EXPECT_EQ(config.batch_size(), 10u);
    EXPECT_EQ(config.flush_interval(), std::chrono::milliseconds(2000));
}

TEST(ConfigTest, ProductionDefaults) {
    auto config = TellConfig::production("feed1e11feed1e11feed1e11feed1e11");
    EXPECT_EQ(config.batch_size(), 100u);
    EXPECT_EQ(config.flush_interval(), std::chrono::milliseconds(10000));
    EXPECT_EQ(config.max_retries(), 3u);
    EXPECT_EQ(config.close_timeout(), std::chrono::milliseconds(5000));
    EXPECT_EQ(config.network_timeout(), std::chrono::milliseconds(30000));
}

TEST(ConfigTest, BuilderCustomValues) {
    auto config = TellConfig::builder("feed1e11feed1e11feed1e11feed1e11")
        .endpoint("custom:9000")
        .batch_size(50)
        .flush_interval(std::chrono::milliseconds(5000))
        .max_retries(5)
        .close_timeout(std::chrono::milliseconds(10000))
        .network_timeout(std::chrono::milliseconds(60000))
        .build();

    EXPECT_EQ(config.endpoint(), "custom:9000");
    EXPECT_EQ(config.batch_size(), 50u);
    EXPECT_EQ(config.flush_interval(), std::chrono::milliseconds(5000));
    EXPECT_EQ(config.max_retries(), 5u);
    EXPECT_EQ(config.close_timeout(), std::chrono::milliseconds(10000));
    EXPECT_EQ(config.network_timeout(), std::chrono::milliseconds(60000));
}

TEST(ConfigTest, ApiKeyDecodedCorrectly) {
    auto config = TellConfig::production("feed1e11feed1e11feed1e11feed1e11");
    auto bytes = config.api_key_bytes();
    EXPECT_EQ(bytes[0], 0xfe);
    EXPECT_EQ(bytes[1], 0xed);
    EXPECT_EQ(bytes[2], 0x1e);
    EXPECT_EQ(bytes[15], 0x11);
}

TEST(ConfigTest, OnErrorCallback) {
    bool called = false;
    auto config = TellConfig::builder("feed1e11feed1e11feed1e11feed1e11")
        .on_error([&called](const TellError&) { called = true; })
        .build();

    EXPECT_TRUE(config.on_error() != nullptr);
    config.on_error()(TellError::network("test"));
    EXPECT_TRUE(called);
}
