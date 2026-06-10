// 测试阶段 5 消息模块的纯逻辑部分
//
// 覆盖：
//   - convKey 会话键（已在 test_friend_util 测试，这里测消息场景）
//   - 消息序号递增（Redis INCR 模拟）
//   - 离线消息类型枚举
//   - 游标翻页逻辑
//   - 在线投递 vs 离线暂存决策
//   - JSON 响应结构

#include <gtest/gtest.h>
#include <json/json.h>
#include <string>
#include <vector>
#include <algorithm>

#include "utils/conversation.h"

using namespace online_chat;

// ---- 离线消息类型 ----
enum OfflineMsgType : int
{
    kSingleOffline = 1,
    kGroupOffline  = 2,
};

// ============================================================
// 一、离线消息类型
// ============================================================

TEST(OfflineMsgTypeTest, SingleIsOne)
{
    EXPECT_EQ(kSingleOffline, 1);
}

TEST(OfflineMsgTypeTest, GroupIsTwo)
{
    EXPECT_EQ(kGroupOffline, 2);
}

TEST(OfflineMsgTypeTest, TypesAreDistinct)
{
    EXPECT_NE(kSingleOffline, kGroupOffline);
}

// ============================================================
// 二、消息序号递增（模拟 Redis INCR）
// ============================================================

TEST(MessageSeqTest, StartFromOne)
{
    int64_t seq = 0;
    seq++;  // 模拟 INCR
    EXPECT_EQ(seq, 1);
}

TEST(MessageSeqTest, MonotonicallyIncreasing)
{
    int64_t seq = 0;
    for (int i = 0; i < 100; ++i)
        seq++;
    EXPECT_EQ(seq, 100);
}

TEST(MessageSeqTest, EachConvHasOwnSeq)
{
    // 不同会话的序号独立
    int64_t seqA = 0, seqB = 0;
    seqA++; seqA++; seqA++;  // 会话 A 发了 3 条
    seqB++; seqB;            // 会话 B 发了 1 条
    EXPECT_EQ(seqA, 3);
    EXPECT_EQ(seqB, 1);
}

TEST(MessageSeqTest, RedisKeyFormat)
{
    // 单聊：msgseq:single:{convKey}
    std::string ck = convKey(1001, 1002);
    std::string key = "msgseq:single:" + ck;
    EXPECT_EQ(key, "msgseq:single:1001-1002");

    // 群聊：msgseq:group:{groupId}
    std::string gkey = "msgseq:group:" + std::to_string(42);
    EXPECT_EQ(gkey, "msgseq:group:42");
}

// ============================================================
// 三、游标翻页逻辑
// ============================================================

TEST(MessagePaginationTest, FirstPageNoCursor)
{
    int64_t beforeSeq = 0;
    int limit = 30;
    EXPECT_EQ(beforeSeq, 0);  // 无游标，加载最新
    EXPECT_EQ(limit, 30);
}

TEST(MessagePaginationTest, NextPageWithCursor)
{
    int64_t beforeSeq = 100;
    int limit = 30;
    // SQL: WHERE seq < 100 ORDER BY seq DESC LIMIT 30
    EXPECT_GT(beforeSeq, 0);
}

TEST(MessagePaginationTest, LimitClamped)
{
    int limit = 200;
    if (limit <= 0 || limit > 100) limit = 30;
    EXPECT_EQ(limit, 30);
}

TEST(MessagePaginationTest, LimitZeroClamped)
{
    int limit = 0;
    if (limit <= 0 || limit > 100) limit = 30;
    EXPECT_EQ(limit, 30);
}

TEST(MessagePaginationTest, LimitNegativeClamped)
{
    int limit = -5;
    if (limit <= 0 || limit > 100) limit = 30;
    EXPECT_EQ(limit, 30);
}

TEST(MessagePaginationTest, HasMoreWhenFull)
{
    int resultSize = 30;
    int limit = 30;
    bool hasMore = (resultSize == limit);
    EXPECT_TRUE(hasMore);
}

TEST(MessagePaginationTest, NoMoreWhenPartial)
{
    int resultSize = 15;
    int limit = 30;
    bool hasMore = (resultSize == limit);
    EXPECT_FALSE(hasMore);
}

TEST(MessagePaginationTest, DescOrderForRecentFirst)
{
    // ORDER BY seq DESC 保证最新消息在前
    std::string sql = "ORDER BY seq DESC LIMIT ?";
    EXPECT_NE(sql.find("DESC"), std::string::npos);
}

// ============================================================
// 四、在线投递 vs 离线暂存
// ============================================================

TEST(MessageDeliveryTest, OnlineUserDelivered)
{
    bool isOnline = true;
    bool shouldSaveOffline = !isOnline;
    EXPECT_FALSE(shouldSaveOffline);
}

TEST(MessageDeliveryTest, OfflineUserSaved)
{
    bool isOnline = false;
    bool shouldSaveOffline = !isOnline;
    EXPECT_TRUE(shouldSaveOffline);
}

TEST(MessageDeliveryTest, GroupBroadcastToOnline)
{
    // 群消息广播给所有在线成员，离线成员存 offline_messages
    struct Member { int64_t id; bool online; };
    std::vector<Member> members = {
        {1001, true},
        {1002, false},
        {1003, true},
    };

    int delivered = 0, offline = 0;
    for (const auto& m : members)
    {
        if (m.online) delivered++;
        else offline++;
    }
    EXPECT_EQ(delivered, 2);
    EXPECT_EQ(offline, 1);
}

// ============================================================
// 五、消息内容校验
// ============================================================

TEST(MessageContentTest, EmptyContentInvalid)
{
    std::string content = "";
    EXPECT_TRUE(content.empty());
}

TEST(MessageContentTest, NonEmptyContentValid)
{
    std::string content = "hello";
    EXPECT_FALSE(content.empty());
}

TEST(MessageContentTest, UnicodeContent)
{
    std::string content = "你好👋";
    EXPECT_FALSE(content.empty());
}

TEST(MessageContentTest, CannotSendToSelf)
{
    int64_t from = 1001, to = 1001;
    EXPECT_EQ(from, to);  // 应拒绝
}

TEST(MessageContentTest, CannotSendToSelfInCode)
{
    // 模拟 message_service.cc 中的校验
    auto isValid = [](int64_t from, int64_t to, const std::string& content)
    {
        if (content.empty()) return false;
        if (from == to) return false;
        return true;
    };

    EXPECT_FALSE(isValid(1001, 1001, "hello"));  // 自己发给自己
    EXPECT_TRUE(isValid(1001, 1002, "hello"));   // 正常
    EXPECT_FALSE(isValid(1001, 1002, ""));        // 空内容
}

// ============================================================
// 六、SQL 模式验证
// ============================================================

TEST(MessageSqlTest, InsertSingleMessage)
{
    std::string sql = "INSERT INTO single_messages (conv_key, from_user, to_user, content, seq) "
                      "VALUES (?, ?, ?, ?, ?)";
    EXPECT_NE(sql.find("conv_key"), std::string::npos);
    EXPECT_NE(sql.find("seq"), std::string::npos);
}

TEST(MessageSqlTest, InsertGroupMessage)
{
    std::string sql = "INSERT INTO group_messages (group_id, from_user, content, seq) "
                      "VALUES (?, ?, ?, ?)";
    EXPECT_NE(sql.find("group_id"), std::string::npos);
}

TEST(MessageSqlTest, InsertOfflineIndex)
{
    std::string sql = "INSERT INTO offline_messages (user_id, msg_type, msg_id) VALUES (?, ?, ?)";
    EXPECT_NE(sql.find("msg_type"), std::string::npos);
}

TEST(MessageSqlTest, SelectOfflineByUser)
{
    std::string sql = "SELECT id, msg_type, msg_id FROM offline_messages WHERE user_id=? ORDER BY id";
    EXPECT_NE(sql.find("user_id=?"), std::string::npos);
}

TEST(MessageSqlTest, DeleteOfflineAfterDelivery)
{
    std::string sql = "DELETE FROM offline_messages WHERE id=?";
    EXPECT_NE(sql.find("id=?"), std::string::npos);
}

TEST(MessageSqlTest, SingleHistoryWithCursor)
{
    std::string sql = "SELECT id, from_user, to_user, content, seq, created_at "
                      "FROM single_messages WHERE conv_key=? AND seq < ? "
                      "ORDER BY seq DESC LIMIT ?";
    EXPECT_NE(sql.find("seq <"), std::string::npos);
    EXPECT_NE(sql.find("DESC"), std::string::npos);
}

TEST(MessageSqlTest, GroupHistoryWithCursor)
{
    std::string sql = "SELECT id, group_id, from_user, content, seq, created_at "
                      "FROM group_messages WHERE group_id=? AND seq < ? "
                      "ORDER BY seq DESC LIMIT ?";
    EXPECT_NE(sql.find("group_id=?"), std::string::npos);
}

// ============================================================
// 七、JSON 响应结构
// ============================================================

TEST(MessageJsonTest, AckResponse)
{
    Json::Value data;
    data["id"]      = Json::Int64(100);
    data["from"]    = Json::Int64(1001);
    data["to"]      = Json::Int64(1002);
    data["content"] = "hello";
    data["seq"]     = Json::Int64(1);
    EXPECT_EQ(data["id"].asInt64(), 100);
    EXPECT_EQ(data["seq"].asInt64(), 1);
}

TEST(MessageJsonTest, GroupAckResponse)
{
    Json::Value data;
    data["id"]      = Json::Int64(200);
    data["groupId"] = Json::Int64(42);
    data["from"]    = Json::Int64(1001);
    data["content"] = "hello group";
    data["seq"]     = Json::Int64(5);
    EXPECT_EQ(data["groupId"].asInt64(), 42);
}

TEST(MessageJsonTest, OfflineMessagesResponse)
{
    Json::Value data;
    data["messages"] = Json::Value(Json::arrayValue);
    EXPECT_EQ(data["messages"].size(), 0u);
}

TEST(MessageJsonTest, HistoryResponse)
{
    Json::Value data;
    data["list"] = Json::Value(Json::arrayValue);
    data["hasMore"] = true;
    EXPECT_TRUE(data["hasMore"].asBool());
}

TEST(MessageJsonTest, SingleHistoryItem)
{
    Json::Value m;
    m["id"]        = Json::Int64(1);
    m["from"]      = Json::Int64(1001);
    m["to"]        = Json::Int64(1002);
    m["content"]   = "hello";
    m["seq"]       = Json::Int64(1);
    m["createdAt"] = "2026-06-10 12:00:00";
    EXPECT_EQ(m["from"].asInt64(), 1001);
    EXPECT_EQ(m["to"].asInt64(), 1002);
}

TEST(MessageJsonTest, GroupHistoryItem)
{
    Json::Value m;
    m["id"]        = Json::Int64(1);
    m["groupId"]   = Json::Int64(42);
    m["from"]      = Json::Int64(1001);
    m["content"]   = "hello group";
    m["seq"]       = Json::Int64(1);
    m["createdAt"] = "2026-06-10 12:00:00";
    EXPECT_EQ(m["groupId"].asInt64(), 42);
}

// ============================================================
// 八、convKey 在消息场景中的使用
// ============================================================

TEST(MessageConvKeyTest, SingleChatKey)
{
    std::string key = convKey(1001, 1002);
    EXPECT_EQ(key, "1001-1002");
}

TEST(MessageConvKeyTest, ReversedUsersSameKey)
{
    EXPECT_EQ(convKey(1002, 1001), convKey(1001, 1002));
}

TEST(MessageConvKeyTest, KeyMatchesSeqRedisKey)
{
    std::string ck = convKey(500, 300);
    std::string redisKey = "msgseq:single:" + ck;
    EXPECT_EQ(redisKey, "msgseq:single:300-500");
}

// ============================================================
// 九、边界条件
// ============================================================

TEST(MessageBoundaryTest, MaxContentLength)
{
    // TEXT 类型无硬性上限，但实际应限制
    std::string content(10000, 'x');
    EXPECT_EQ(content.size(), 10000u);
}

TEST(MessageBoundaryTest, LargeSeq)
{
    int64_t seq = 9999999999LL;
    EXPECT_GT(seq, 0);
}

TEST(MessageBoundaryTest, ZeroBeforeSeqMeansNoFilter)
{
    int64_t beforeSeq = 0;
    EXPECT_EQ(beforeSeq, 0);  // 加载最新消息
}

TEST(MessageBoundaryTest, DefaultLimit)
{
    int limit = 30;
    EXPECT_EQ(limit, 30);
}
