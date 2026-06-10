#pragma once

#include <drogon/drogon.h>
#include <drogon/nosql/RedisClient.h>
#include <string>

// Redis 客户端工具
//
// Drogon >= 1.7 内置 Redis 客户端支持，通过 config.json 的 redis_clients 配置连接池。
// 本类仅封装获取客户端的便捷方法。
//
// Redis 在本项目中承担：
//   - 验证码暂存（verifycode:{email}，TTL 5min）
//   - 发送限频（verifycode_limit:{email}，TTL 60s）
//   - 会话/单点登录（session:{userId}，存当前有效 token）
//   - 在线状态（online:{userId}，心跳续期）
//   - 消息序号生成（msgseq:single:{convKey} / msgseq:group:{groupId}，INCR）
namespace online_chat {

class RedisClient
{
public:
    // 获取默认 Redis 客户端（连接池，线程安全）
    static drogon::nosql::RedisClientPtr get()
    {
        return drogon::app().getRedisClient();
    }

    // 获取指定名称的 Redis 客户端（config 中 name 字段）
    static drogon::nosql::RedisClientPtr get(const std::string& name)
    {
        return drogon::app().getRedisClient(name);
    }
};

}  // namespace online_chat
