// 测试 ws_protocol.h — makeEnvelope / parseEnvelope
// 依赖 jsoncpp（libjsoncpp-dev），不依赖 Drogon

#include <gtest/gtest.h>
#include "utils/ws_protocol.h"

using namespace online_chat::ws;

// makeEnvelope 生成的 JSON 可被 parseEnvelope 正确解析
TEST(WsProtocolTest, RoundTrip)
{
    Json::Value data;
    data["from"] = 1001;
    data["to"]   = 1002;
    data["text"] = "hello";

    std::string raw = makeEnvelope(CHAT_SINGLE, 42, data);
    EXPECT_FALSE(raw.empty());

    std::string type;
    int64_t seq = 0;
    Json::Value parsed;
    ASSERT_TRUE(parseEnvelope(raw, type, seq, parsed));
    EXPECT_EQ(type, CHAT_SINGLE);
    EXPECT_EQ(seq, 42);
    EXPECT_EQ(parsed["from"].asInt(), 1001);
    EXPECT_EQ(parsed["to"].asInt(), 1002);
    EXPECT_EQ(parsed["text"].asString(), "hello");
}

// 解析空字符串失败
TEST(WsProtocolTest, ParseEmptyFails)
{
    std::string type;
    int64_t seq;
    Json::Value data;
    EXPECT_FALSE(parseEnvelope("", type, seq, data));
}

// 解析非法 JSON 失败
TEST(WsProtocolTest, ParseInvalidJsonFails)
{
    std::string type;
    int64_t seq;
    Json::Value data;
    EXPECT_FALSE(parseEnvelope("{not json!!!", type, seq, data));
}

// 解析缺少 type 字段失败
TEST(WsProtocolTest, ParseMissingTypeFails)
{
    std::string type;
    int64_t seq;
    Json::Value data;
    EXPECT_FALSE(parseEnvelope(R"({"seq":1,"data":{}})", type, seq, data));
}

// type 不是字符串时失败
TEST(WsProtocolTest, ParseNonStringTypeFails)
{
    std::string type;
    int64_t seq;
    Json::Value data;
    EXPECT_FALSE(parseEnvelope(R"({"type":123,"data":{}})", type, seq, data));
}

// 缺少 seq 字段时 seq 默认为 0
TEST(WsProtocolTest, ParseMissingSeqDefaultsZero)
{
    std::string type;
    int64_t seq = -1;
    Json::Value data;
    ASSERT_TRUE(parseEnvelope(R"({"type":"ping","data":{}})", type, seq, data));
    EXPECT_EQ(seq, 0);
    EXPECT_EQ(type, "ping");
}

// 缺少 data 字段时 data 为 null
TEST(WsProtocolTest, ParseMissingDataDefaultsNull)
{
    std::string type;
    int64_t seq;
    Json::Value data;
    ASSERT_TRUE(parseEnvelope(R"({"type":"pong","seq":1})", type, seq, data));
    EXPECT_TRUE(data.isNull());
}

// 各 type 常量值正确
TEST(WsProtocolTest, TypeConstants)
{
    EXPECT_STREQ(CHAT_SINGLE, "chat_single");
    EXPECT_STREQ(CHAT_GROUP, "chat_group");
    EXPECT_STREQ(NOTIFY_APPLY, "notify_apply");
    EXPECT_STREQ(NOTIFY_RESULT, "notify_apply_result");
    EXPECT_STREQ(PRESENCE, "presence");
    EXPECT_STREQ(KICKED, "kicked");
    EXPECT_STREQ(ACK, "ack");
    EXPECT_STREQ(PING, "ping");
    EXPECT_STREQ(PONG, "pong");
    EXPECT_STREQ(ERROR, "error");
}

// data 为嵌套对象时 round-trip 正确
TEST(WsProtocolTest, NestedDataRoundTrip)
{
    Json::Value data;
    data["user"]["id"]       = 1001;
    data["user"]["nickname"] = "Alice";
    data["content"]          = "你好👋";

    std::string raw = makeEnvelope(CHAT_GROUP, 999, data);

    std::string type;
    int64_t seq;
    Json::Value parsed;
    ASSERT_TRUE(parseEnvelope(raw, type, seq, parsed));
    EXPECT_EQ(type, CHAT_GROUP);
    EXPECT_EQ(seq, 999);
    EXPECT_EQ(parsed["user"]["id"].asInt(), 1001);
    EXPECT_EQ(parsed["user"]["nickname"].asString(), "Alice");
    EXPECT_EQ(parsed["content"].asString(), "你好👋");
}

// 负数 seq 正确 round-trip
TEST(WsProtocolTest, NegativeSeqRoundTrip)
{
    Json::Value data;
    std::string raw = makeEnvelope(ERROR, -1, data);

    std::string type;
    int64_t seq;
    Json::Value parsed;
    ASSERT_TRUE(parseEnvelope(raw, type, seq, parsed));
    EXPECT_EQ(seq, -1);
}
