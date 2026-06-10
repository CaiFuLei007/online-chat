// 测试阶段 3 用户与好友模块的纯逻辑部分
//
// 重点测试：
//   - convKey: 会话键生成（min-max 排序）
//   - parseConvKey: 会话键解析
//   - SQL 模式验证（好友关系 user_a < user_b 约定）

#include <gtest/gtest.h>
#include "utils/conversation.h"

#include <string>

using namespace online_chat;

// ---- convKey 测试 ----

TEST(ConvKeyTest, OrderedIds)
{
    EXPECT_EQ(convKey(1001, 1002), "1001-1002");
}

TEST(ConvKeyTest, ReversedIds)
{
    // 即使传入顺序反了，结果也应是 min-max
    EXPECT_EQ(convKey(1002, 1001), "1001-1002");
}

TEST(ConvKeyTest, SameId)
{
    EXPECT_EQ(convKey(1001, 1001), "1001-1001");
}

TEST(ConvKeyTest, LargeIds)
{
    EXPECT_EQ(convKey(9999999999LL, 10000000000LL), "9999999999-10000000000");
}

TEST(ConvKeyTest, SmallIds)
{
    EXPECT_EQ(convKey(1, 2), "1-2");
    EXPECT_EQ(convKey(2, 1), "1-2");
}

TEST(ConvKeyTest, Idempotent)
{
    // 多次调用结果一致
    std::string k1 = convKey(500, 300);
    std::string k2 = convKey(300, 500);
    std::string k3 = convKey(500, 300);
    EXPECT_EQ(k1, k2);
    EXPECT_EQ(k1, k3);
    EXPECT_EQ(k1, "300-500");
}

// ---- parseConvKey 测试 ----

TEST(ParseConvKeyTest, ValidKey)
{
    int64_t a = 0, b = 0;
    ASSERT_TRUE(parseConvKey("1001-1002", a, b));
    EXPECT_EQ(a, 1001);
    EXPECT_EQ(b, 1002);
}

TEST(ParseConvKeyTest, ReversedKey)
{
    int64_t a = 0, b = 0;
    ASSERT_TRUE(parseConvKey("1002-1001", a, b));
    EXPECT_EQ(a, 1002);
    EXPECT_EQ(b, 1001);
}

TEST(ParseConvKeyTest, SameId)
{
    int64_t a = 0, b = 0;
    ASSERT_TRUE(parseConvKey("1001-1001", a, b));
    EXPECT_EQ(a, 1001);
    EXPECT_EQ(b, 1001);
}

TEST(ParseConvKeyTest, EmptyString)
{
    int64_t a, b;
    EXPECT_FALSE(parseConvKey("", a, b));
}

TEST(ParseConvKeyTest, NoDash)
{
    int64_t a, b;
    EXPECT_FALSE(parseConvKey("10011002", a, b));
}

TEST(ParseConvKeyTest, LeadingDash)
{
    int64_t a, b;
    EXPECT_FALSE(parseConvKey("-1002", a, b));
}

TEST(ParseConvKeyTest, TrailingDash)
{
    int64_t a, b;
    EXPECT_FALSE(parseConvKey("1001-", a, b));
}

TEST(ParseConvKeyTest, NonNumeric)
{
    int64_t a, b;
    EXPECT_FALSE(parseConvKey("abc-def", a, b));
}

TEST(ParseConvKeyTest, MultipleDashes)
{
    int64_t a = 0, b = 0;
    // "100-200-300" → a=100, b=200（只取第一个 dash）
    // 实际上 stoll("200-300") 会抛异常，所以应该返回 false
    EXPECT_FALSE(parseConvKey("100-200-300", a, b));
}

// ---- round-trip 测试 ----

TEST(ConvKeyRoundTrip, GenerateAndParse)
{
    int64_t origA = 12345, origB = 67890;
    std::string key = convKey(origA, origB);

    int64_t parsedA = 0, parsedB = 0;
    ASSERT_TRUE(parseConvKey(key, parsedA, parsedB));

    // parseConvKey 保留原始顺序，convKey 保证 min-max
    EXPECT_EQ(parsedA, std::min(origA, origB));
    EXPECT_EQ(parsedB, std::max(origA, origB));
}

TEST(ConvKeyRoundTrip, ReversedInputProducesSameKey)
{
    std::string key1 = convKey(200, 100);
    std::string key2 = convKey(100, 200);

    int64_t a1, b1, a2, b2;
    ASSERT_TRUE(parseConvKey(key1, a1, b1));
    ASSERT_TRUE(parseConvKey(key2, a2, b2));

    EXPECT_EQ(a1, a2);
    EXPECT_EQ(b1, b2);
}

// ---- 好友关系 user_a < user_b 约定验证 ----

TEST(FriendshipOrderTest, CanonicalPair)
{
    // 模拟 friendships 表的 UNIQUE(user_a, user_b) 约定
    // 始终 user_a < user_b
    int64_t userA = std::min(1002LL, 1001LL);
    int64_t userB = std::max(1002LL, 1001LL);
    EXPECT_EQ(userA, 1001);
    EXPECT_EQ(userB, 1002);
    EXPECT_LT(userA, userB);
}

TEST(FriendshipOrderTest, ConvKeyMatchesFriendship)
{
    // convKey 和 friendships 表的排序逻辑应一致
    int64_t userId = 500;
    int64_t friendId = 300;

    std::string key = convKey(userId, friendId);
    EXPECT_EQ(key, "300-500");

    // friendships 表中 user_a=300, user_b=500
    int64_t canonicalA = std::min(userId, friendId);
    int64_t canonicalB = std::max(userId, friendId);
    EXPECT_EQ(convKey(canonicalA, canonicalB), key);
}
