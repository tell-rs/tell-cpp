// tests/props_test.cpp
// Unit tests for Props builder.

#include <gtest/gtest.h>
#include "tell/props.hpp"
#include <string>

using namespace tell;

TEST(PropsTest, EmptyProps) {
    Props p;
    auto bytes = p.to_json_bytes();
    std::string json(bytes.begin(), bytes.end());
    EXPECT_EQ(json, "{}");
    EXPECT_TRUE(p.empty());
}

TEST(PropsTest, SingleString) {
    Props p;
    p.add("url", "/home");
    auto bytes = p.to_json_bytes();
    std::string json(bytes.begin(), bytes.end());
    EXPECT_EQ(json, R"({"url":"/home"})");
}

TEST(PropsTest, MultipleTypes) {
    Props p;
    p.add("url", "/home")
     .add("count", int64_t(42))
     .add("active", true)
     .add("rate", 3.14);
    auto bytes = p.to_json_bytes();
    std::string json(bytes.begin(), bytes.end());

    EXPECT_NE(json.find("\"url\":\"/home\""), std::string::npos);
    EXPECT_NE(json.find("\"count\":42"), std::string::npos);
    EXPECT_NE(json.find("\"active\":true"), std::string::npos);
    EXPECT_NE(json.find("\"rate\":3.14"), std::string::npos);
}

TEST(PropsTest, BoolFalse) {
    Props p;
    p.add("active", false);
    auto bytes = p.to_json_bytes();
    std::string json(bytes.begin(), bytes.end());
    EXPECT_EQ(json, R"({"active":false})");
}

TEST(PropsTest, EscapesQuotes) {
    Props p;
    p.add("name", "O'Brien\"test");
    auto bytes = p.to_json_bytes();
    std::string json(bytes.begin(), bytes.end());
    EXPECT_NE(json.find("\\\""), std::string::npos);
}

TEST(PropsTest, EscapesBackslash) {
    Props p;
    p.add("path", "C:\\Users\\test");
    auto bytes = p.to_json_bytes();
    std::string json(bytes.begin(), bytes.end());
    EXPECT_NE(json.find("\\\\"), std::string::npos);
}

TEST(PropsTest, IntegerValue) {
    Props p;
    p.add("count", 42);
    auto bytes = p.to_json_bytes();
    std::string json(bytes.begin(), bytes.end());
    EXPECT_EQ(json, R"({"count":42})");
}

TEST(PropsTest, NegativeInteger) {
    Props p;
    p.add("offset", int64_t(-5));
    auto bytes = p.to_json_bytes();
    std::string json(bytes.begin(), bytes.end());
    EXPECT_EQ(json, R"({"offset":-5})");
}

TEST(PropsTest, SizeTracking) {
    Props p;
    EXPECT_EQ(p.size(), 0u);
    p.add("a", "1");
    EXPECT_EQ(p.size(), 1u);
    p.add("b", "2");
    EXPECT_EQ(p.size(), 2u);
}
