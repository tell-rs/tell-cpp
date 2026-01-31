// tests/validation_test.cpp
// Unit tests for input validation.

#include <gtest/gtest.h>
#include "validation.hpp"

using namespace tell::validation;

TEST(ValidationTest, ValidUserId) {
    EXPECT_TRUE(check_user_id("user_123"));
}

TEST(ValidationTest, EmptyUserId) {
    EXPECT_FALSE(check_user_id(""));
}

TEST(ValidationTest, ValidEventName) {
    EXPECT_TRUE(check_event_name("Page Viewed"));
}

TEST(ValidationTest, EmptyEventName) {
    EXPECT_FALSE(check_event_name(""));
}

TEST(ValidationTest, LongEventName) {
    std::string long_name(257, 'x');
    EXPECT_FALSE(check_event_name(long_name));
}

TEST(ValidationTest, MaxLengthEventName) {
    std::string name(256, 'x');
    EXPECT_TRUE(check_event_name(name));
}

TEST(ValidationTest, ValidLogMessage) {
    EXPECT_TRUE(check_log_message("Something happened"));
}

TEST(ValidationTest, EmptyLogMessage) {
    EXPECT_FALSE(check_log_message(""));
}

TEST(ValidationTest, LongLogMessage) {
    std::string msg(65537, 'x');
    EXPECT_FALSE(check_log_message(msg));
}

TEST(ValidationTest, ValidApiKey) {
    auto bytes = validate_and_decode_api_key("a1b2c3d4e5f60718293a4b5c6d7e8f90");
    EXPECT_EQ(bytes[0], 0xa1);
    EXPECT_EQ(bytes[1], 0xb2);
}

TEST(ValidationTest, ShortApiKey) {
    EXPECT_THROW(validate_and_decode_api_key("abc"), tell::TellError);
}

TEST(ValidationTest, NonHexApiKey) {
    EXPECT_THROW(validate_and_decode_api_key("zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz"), tell::TellError);
}

TEST(ValidationTest, UpperCaseHexApiKey) {
    auto bytes = validate_and_decode_api_key("A1B2C3D4E5F60718293A4B5C6D7E8F90");
    EXPECT_EQ(bytes[0], 0xA1);
    EXPECT_EQ(bytes[1], 0xB2);
}
