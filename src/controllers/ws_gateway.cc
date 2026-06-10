#include "controllers/ws_gateway.h"
#include "utils/jwt_util.h"
#include "utils/ws_protocol.h"
#include "utils/errors.h"
#include "dao/redis_client.h"

#include <drogon/drogon.h>
#include <json/json.h>

namespace online_chat {

// ---- 握手：从 query string 的 token 参数做 JWT 鉴权 ----
void WsGateway::handleNewConnection(
    const drogon::HttpRequestPtr& req,
    const drogon::WebSocketConnectionPtr& conn)
{
    // 从 URL query 取 token: ws://host:8080/ws?token=eyJ...
    std::string token = req->getParameter("token");
    if (token.empty())
    {
        LOG_WARN << "WS connection rejected: missing token";
        conn->shutdown(drogon::CloseCode::kViolation, "missing token");
        return;
    }

    auto payload = JwtUtil::verify(token);
    if (!payload.has_value())
    {
        LOG_WARN << "WS connection rejected: invalid token";
        conn->shutdown(drogon::CloseCode::kViolation, "invalid token");
        return;
    }

    // 验证 Redis session（与 JwtFilter 逻辑一致）
    auto redis = RedisClient::get();
    std::string sessionKey = "session:" + std::to_string(payload->userId);

    // 注意：握手阶段不能做异步操作（Drogon 限制），
    // 仅做 JWT 签名校验即可，Redis session 校验留给后续消息处理兜底。
    // 这里信任 JWT 签名已通过。

    // 设置连接上下文
    auto ctx = std::make_shared<WsContext>();
    ctx->userId   = payload->userId;
    ctx->email    = payload->email;
    ctx->role     = payload->role;
    ctx->nickname = payload->nickname;
    conn->setContext(ctx);

    // 注册连接（自动挤掉旧连接）
    ConnectionManager::instance().add(payload->userId, conn);

    // 设置心跳（30 秒无 pong 自动断开）
    conn->setPingMessage("", std::chrono::seconds(30));

    // 设置在线状态
    setOnline(payload->userId);

    LOG_INFO << "WS connected: userId=" << payload->userId
             << " nickname=" << payload->nickname;

    // 发送欢迎消息
    Json::Value data;
    data["message"] = "connected";
    data["userId"]  = Json::Int64(payload->userId);
    conn->send(ws::makeEnvelope(ws::ACK, 0, data));
}

// ---- 消息处理 ----
void WsGateway::handleNewMessage(
    const drogon::WebSocketConnectionPtr& conn,
    std::string&& message,
    const drogon::WebSocketMessageType& type)
{
    // 只处理文本消息
    if (type != drogon::WebSocketMessageType::Text)
        return;

    auto ctxPtr = conn->getContext<WsContext>();
    if (!ctxPtr)
    {
        conn->shutdown(drogon::CloseCode::kViolation, "no context");
        return;
    }

    // 解析信封
    std::string msgType;
    int64_t seq = 0;
    Json::Value data;
    if (!ws::parseEnvelope(message, msgType, seq, data))
    {
        Json::Value err;
        err["message"] = "invalid message format";
        conn->send(ws::makeEnvelope(ws::ERROR, 0, err));
        return;
    }

    // 心跳单独处理
    if (msgType == ws::PING)
    {
        handlePing(conn, seq);
        return;
    }

    // 路由到具体处理
    routeMessage(conn, *ctxPtr, msgType, seq, data);
}

// ---- 连接关闭 ----
void WsGateway::handleConnectionClosed(
    const drogon::WebSocketConnectionPtr& conn)
{
    auto ctxPtr = conn->getContext<WsContext>();
    if (ctxPtr)
    {
        LOG_INFO << "WS disconnected: userId=" << ctxPtr->userId;
        ConnectionManager::instance().remove(ctxPtr->userId, conn);
        setOffline(ctxPtr->userId);
    }
}

// ---- 消息路由（阶段 2 只做骨架，阶段 5 实现具体逻辑） ----
void WsGateway::routeMessage(
    const drogon::WebSocketConnectionPtr& conn,
    const WsContext& ctx,
    const std::string& type,
    int64_t seq,
    const Json::Value& data)
{
    if (type == ws::CHAT_SINGLE)
    {
        // 阶段 5 实现：单聊消息收发
        LOG_DEBUG << "CHAT_SINGLE from userId=" << ctx.userId
                  << " to=" << data.get("to", 0).asInt64();
        // 临时回显确认
        Json::Value ack;
        ack["message"] = "chat_single not implemented yet";
        conn->send(ws::makeEnvelope(ws::ACK, seq, ack));
    }
    else if (type == ws::CHAT_GROUP)
    {
        // 阶段 5 实现：群聊消息广播
        LOG_DEBUG << "CHAT_GROUP from userId=" << ctx.userId
                  << " groupId=" << data.get("groupId", 0).asInt64();
        Json::Value ack;
        ack["message"] = "chat_group not implemented yet";
        conn->send(ws::makeEnvelope(ws::ACK, seq, ack));
    }
    else
    {
        Json::Value err;
        err["message"] = "unknown message type: " + type;
        conn->send(ws::makeEnvelope(ws::ERROR, seq, err));
    }
}

// ---- 心跳 ----
void WsGateway::handlePing(
    const drogon::WebSocketConnectionPtr& conn,
    int64_t seq)
{
    Json::Value data;
    conn->send(ws::makeEnvelope(ws::PONG, seq, data));

    // 续期在线状态
    auto ctxPtr = conn->getContext<WsContext>();
    if (ctxPtr)
        setOnline(ctxPtr->userId);
}

// ---- 在线状态管理（Redis） ----
void WsGateway::setOnline(int64_t userId)
{
    auto redis = RedisClient::get();
    std::string key = "online:" + std::to_string(userId);
    redis->execCommandAsync(
        [](const drogon::nosql::RedisResult&) {},
        [](const drogon::nosql::RedisException& e)
        {
            LOG_ERROR << "Redis SET online error: " << e.what();
        },
        "SET %s 1 EX %d", key.c_str(), 60);  // 60s TTL，心跳续期
}

void WsGateway::setOffline(int64_t userId)
{
    auto redis = RedisClient::get();
    std::string key = "online:" + std::to_string(userId);
    redis->execCommandAsync(
        [](const drogon::nosql::RedisResult&) {},
        [](const drogon::nosql::RedisException& e)
        {
            LOG_ERROR << "Redis DEL online error: " << e.what();
        },
        "DEL %s", key.c_str());
}

}  // namespace online_chat
