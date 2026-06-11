#pragma once

#include "utils/ws_protocol.h"

#include <drogon/WebSocketController.h>
#include <drogon/WebSocketConnection.h>
#include <json/json.h>
#include <string>
#include <unordered_map>
#include <mutex>
#include <memory>

// WebSocket 网关
//
// 职责：
//   1. 连接注册/注销（userId ↔ 连接映射）
//   2. JWT 鉴权（握手时从 query string 读取 token）
//   3. 消息信封解析与按 type 路由
//   4. 心跳 ping/pong
//   5. 在线状态维护（Redis online:{userId}）
//   6. 挤下线（KICKED 推送）
namespace online_chat {

// 每个连接附带的上下文
struct WsContext
{
    int64_t     userId = 0;
    std::string email;
    std::string nickname;
    int         role = 0;
};

// 连接管理器（单例，线程安全）
class ConnectionManager
{
public:
    static ConnectionManager& instance()
    {
        static ConnectionManager mgr;
        return mgr;
    }

    // 注册连接
    void add(int64_t userId, const drogon::WebSocketConnectionPtr& conn)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        // 如果已有旧连接，先踢掉
        auto it = connections_.find(userId);
        if (it != connections_.end() && it->second->connected())
        {
            it->second->shutdown(drogon::CloseCode::kNormalClosure, "kicked by new login");
        }
        connections_[userId] = conn;
    }

    // 注销连接
    void remove(int64_t userId, const drogon::WebSocketConnectionPtr& conn)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = connections_.find(userId);
        // 只移除同一个连接（防止新连接误删）
        if (it != connections_.end() && it->second == conn)
            connections_.erase(it);
    }

    // 向指定用户发送消息
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

    // 挤下线：推送 kicked 通知并关闭该用户当前连接（新登录时调用）
    void kick(int64_t userId, const std::string& reason)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = connections_.find(userId);
        if (it != connections_.end() && it->second->connected())
        {
            Json::Value data;
            data["message"] = reason;
            it->second->send(ws::makeEnvelope(ws::KICKED, 0, data));
            it->second->shutdown(drogon::CloseCode::kNormalClosure,
                                 "kicked by new login");
            connections_.erase(it);
        }
    }

    // 检查用户是否在线
    bool isOnline(int64_t userId)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = connections_.find(userId);
        return it != connections_.end() && it->second->connected();
    }

    // 获取在线用户数
    size_t onlineCount()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return connections_.size();
    }

private:
    ConnectionManager() = default;
    std::mutex mutex_;
    std::unordered_map<int64_t, drogon::WebSocketConnectionPtr> connections_;
};

// WebSocket 网关控制器
class WsGateway : public drogon::WebSocketController<WsGateway>
{
public:
    WS_PATH_LIST_BEGIN
    WS_PATH_ADD("/ws", drogon::Get);
    WS_PATH_LIST_END

    void handleNewConnection(const drogon::HttpRequestPtr& req,
                             const drogon::WebSocketConnectionPtr& conn) override;

    void handleNewMessage(const drogon::WebSocketConnectionPtr& conn,
                          std::string&& message,
                          const drogon::WebSocketMessageType& type) override;

    void handleConnectionClosed(const drogon::WebSocketConnectionPtr& conn) override;

private:
    // 按消息 type 路由
    void routeMessage(const drogon::WebSocketConnectionPtr& conn,
                      const WsContext& ctx,
                      const std::string& type,
                      int64_t seq,
                      const Json::Value& data);

    // 心跳处理
    void handlePing(const drogon::WebSocketConnectionPtr& conn, int64_t seq);

    // 设置在线状态（Redis）
    void setOnline(int64_t userId);
    void setOffline(int64_t userId);
};

}  // namespace online_chat
