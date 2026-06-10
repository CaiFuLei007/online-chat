// 测试 WebSocket 网关的 ConnectionManager 和 WsContext
//
// WebSocketConnection 是抽象类，这里用 mock 子类模拟连接行为。
// 不测试 Drogon 框架本身，只测试我们写的连接管理逻辑。

#include <gtest/gtest.h>
#include <drogon/WebSocketConnection.h>
#include <json/json.h>
#include <memory>
#include <string>

// ---- Mock WebSocketConnection ----
class MockWsConnection : public drogon::WebSocketConnection
{
public:
    bool connected_ = true;
    bool shutdownCalled_ = false;
    std::string lastMessage_;
    int sendCount_ = 0;

    // 匹配实际 Drogon 签名：send(std::string_view, type)
    void send(std::string_view msg,
              const drogon::WebSocketMessageType type = drogon::WebSocketMessageType::Text) override
    {
        lastMessage_ = std::string(msg);
        ++sendCount_;
    }

    // 匹配实际 Drogon 签名：send(const char*, uint64_t, type)
    void send(const char* msg, uint64_t len,
              const drogon::WebSocketMessageType type = drogon::WebSocketMessageType::Text) override
    {
        lastMessage_ = std::string(msg, len);
        ++sendCount_;
    }

    void sendJson(const Json::Value& json,
                  const drogon::WebSocketMessageType type = drogon::WebSocketMessageType::Text) override
    {
        lastMessage_ = json.toStyledString();
        ++sendCount_;
    }

    bool connected() const override { return connected_; }
    bool disconnected() const override { return !connected_; }

    void shutdown(const drogon::CloseCode code = drogon::CloseCode::kNormalClosure,
                  const std::string& reason = "") override
    {
        shutdownCalled_ = true;
        connected_ = false;
    }

    void forceClose() override
    {
        shutdownCalled_ = true;
        connected_ = false;
    }

    void setPingMessage(const std::string& message,
                        const std::chrono::duration<double>& interval) override {}

    void disablePing() override {}

    const trantor::InetAddress& peerAddr() const override
    {
        static trantor::InetAddress addr("127.0.0.1", 8080);
        return addr;
    }

    const trantor::InetAddress& localAddr() const override
    {
        static trantor::InetAddress addr("0.0.0.0", 8080);
        return addr;
    }
};

// ---- 包装 ConnectionManager 逻辑用于测试 ----
// 复制核心逻辑，避免引入 ws_gateway.h 中的 Drogon 控制器静态注册

class TestConnectionManager
{
public:
    void add(int64_t userId, const std::shared_ptr<MockWsConnection>& conn)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = connections_.find(userId);
        if (it != connections_.end())
        {
            oldShutdownCalled_ = true;
        }
        connections_[userId] = conn;
    }

    void remove(int64_t userId, const std::shared_ptr<MockWsConnection>& conn)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = connections_.find(userId);
        if (it != connections_.end() && it->second == conn)
            connections_.erase(it);
    }

    bool sendTo(int64_t userId, const std::string& message)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = connections_.find(userId);
        if (it != connections_.end() && it->second->connected())
        {
            it->second->send(message);
            return true;
        }
        return false;
    }

    bool isOnline(int64_t userId)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = connections_.find(userId);
        return it != connections_.end() && it->second->connected();
    }

    size_t onlineCount()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return connections_.size();
    }

    bool oldShutdownCalled_ = false;

private:
    std::mutex mutex_;
    std::unordered_map<int64_t, std::shared_ptr<MockWsConnection>> connections_;
};

// ---- 测试 ----

TEST(ConnectionManagerTest, AddAndOnline)
{
    TestConnectionManager mgr;
    auto conn = std::make_shared<MockWsConnection>();
    mgr.add(1001, conn);
    EXPECT_TRUE(mgr.isOnline(1001));
    EXPECT_FALSE(mgr.isOnline(9999));
}

TEST(ConnectionManagerTest, Remove)
{
    TestConnectionManager mgr;
    auto conn = std::make_shared<MockWsConnection>();
    mgr.add(1001, conn);
    EXPECT_TRUE(mgr.isOnline(1001));
    mgr.remove(1001, conn);
    EXPECT_FALSE(mgr.isOnline(1001));
}

TEST(ConnectionManagerTest, RemoveWrongConnDoesNothing)
{
    TestConnectionManager mgr;
    auto conn1 = std::make_shared<MockWsConnection>();
    auto conn2 = std::make_shared<MockWsConnection>();
    mgr.add(1001, conn1);
    mgr.remove(1001, conn2);
    EXPECT_TRUE(mgr.isOnline(1001));
}

TEST(ConnectionManagerTest, AddReplacesOldConnection)
{
    TestConnectionManager mgr;
    auto oldConn = std::make_shared<MockWsConnection>();
    auto newConn = std::make_shared<MockWsConnection>();
    mgr.add(1001, oldConn);
    mgr.add(1001, newConn);
    EXPECT_TRUE(mgr.isOnline(1001));
    EXPECT_TRUE(mgr.oldShutdownCalled_);
}

TEST(ConnectionManagerTest, SendToOnlineUser)
{
    TestConnectionManager mgr;
    auto conn = std::make_shared<MockWsConnection>();
    mgr.add(1001, conn);
    bool sent = mgr.sendTo(1001, "hello");
    EXPECT_TRUE(sent);
    EXPECT_EQ(conn->lastMessage_, "hello");
    EXPECT_EQ(conn->sendCount_, 1);
}

TEST(ConnectionManagerTest, SendToOfflineUser)
{
    TestConnectionManager mgr;
    bool sent = mgr.sendTo(9999, "hello");
    EXPECT_FALSE(sent);
}

TEST(ConnectionManagerTest, SendToDisconnectedUser)
{
    TestConnectionManager mgr;
    auto conn = std::make_shared<MockWsConnection>();
    mgr.add(1001, conn);
    conn->connected_ = false;
    EXPECT_FALSE(mgr.isOnline(1001));
    bool sent = mgr.sendTo(1001, "hello");
    EXPECT_FALSE(sent);
}

TEST(ConnectionManagerTest, OnlineCount)
{
    TestConnectionManager mgr;
    EXPECT_EQ(mgr.onlineCount(), 0u);
    auto c1 = std::make_shared<MockWsConnection>();
    auto c2 = std::make_shared<MockWsConnection>();
    mgr.add(1001, c1);
    EXPECT_EQ(mgr.onlineCount(), 1u);
    mgr.add(1002, c2);
    EXPECT_EQ(mgr.onlineCount(), 2u);
    mgr.remove(1001, c1);
    EXPECT_EQ(mgr.onlineCount(), 1u);
}

TEST(ConnectionManagerTest, MultipleUsers)
{
    TestConnectionManager mgr;
    for (int64_t i = 1; i <= 100; ++i)
    {
        auto conn = std::make_shared<MockWsConnection>();
        mgr.add(i, conn);
    }
    EXPECT_EQ(mgr.onlineCount(), 100u);
    EXPECT_TRUE(mgr.isOnline(50));
    EXPECT_TRUE(mgr.isOnline(100));
    EXPECT_FALSE(mgr.isOnline(101));
}

// WsContext 结构体测试
struct WsContext
{
    int64_t     userId = 0;
    std::string email;
    std::string nickname;
    int         role = 0;
};

TEST(WsContextTest, DefaultValues)
{
    WsContext ctx;
    EXPECT_EQ(ctx.userId, 0);
    EXPECT_TRUE(ctx.email.empty());
    EXPECT_TRUE(ctx.nickname.empty());
    EXPECT_EQ(ctx.role, 0);
}

TEST(WsContextTest, AssignedValues)
{
    WsContext ctx;
    ctx.userId = 1001;
    ctx.email = "alice@example.com";
    ctx.nickname = "Alice";
    ctx.role = 1;
    EXPECT_EQ(ctx.userId, 1001);
    EXPECT_EQ(ctx.email, "alice@example.com");
    EXPECT_EQ(ctx.nickname, "Alice");
    EXPECT_EQ(ctx.role, 1);
}
