#pragma once

#include <json/json.h>
#include <string>

// WebSocket 消息协议定义
//
// 所有 WS 消息统一使用 JSON 信封格式：
//   { "type": "...", "seq": 0, "data": { ... } }
//
// 本头文件定义 type 常量与构造工具，供 WsGateway / 客户端共用。
namespace online_chat {
namespace ws {

// ---- type 常量 ----
// 聊天消息
constexpr const char* CHAT_SINGLE       = "chat_single";        // 单聊消息
constexpr const char* CHAT_GROUP        = "chat_group";         // 群聊消息

// 通知
constexpr const char* NOTIFY_APPLY      = "notify_apply";       // 收到新的加好友/加群申请
constexpr const char* NOTIFY_RESULT     = "notify_apply_result";// 自己的申请被同意/拒绝

// 在线状态
constexpr const char* PRESENCE          = "presence";           // 好友上线/下线

// 会话管理
constexpr const char* KICKED            = "kicked";             // 被新登录挤下线

// 确认 / 心跳 / 错误
constexpr const char* ACK               = "ack";                // 服务端确认消息已落库
constexpr const char* PING              = "ping";               // 客户端心跳
constexpr const char* PONG              = "pong";               // 服务端心跳回复
constexpr const char* ERROR             = "error";              // 服务端错误通知

// ---- 构造工具 ----

// 构造 WS 消息 JSON 信封
inline std::string makeEnvelope(const std::string& type,
                                int64_t seq,
                                const Json::Value& data)
{
    Json::Value env;
    env["type"] = type;
    env["seq"]  = Json::Int64(seq);
    env["data"] = data;
    return env.toStyledString();
}

// 解析 WS 消息信封（返回 false 表示格式错误）
inline bool parseEnvelope(const std::string& raw,
                          std::string& type,
                          int64_t& seq,
                          Json::Value& data)
{
    Json::CharReaderBuilder rb;
    std::string errs;
    std::istringstream iss(raw);
    Json::Value env;
    if (!Json::parseFromStream(rb, iss, &env, &errs))
        return false;
    if (!env.isMember("type") || !env["type"].isString())
        return false;

    type = env["type"].asString();
    seq  = env.isMember("seq") ? env["seq"].asInt64() : 0;
    data = env.isMember("data") ? env["data"] : Json::Value::null;
    return true;
}

}  // namespace ws
}  // namespace online_chat
