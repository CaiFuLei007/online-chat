#pragma once

#include "utils/jwt_util.h"
#include "utils/response.h"
#include "dao/redis_client.h"

#include <drogon/drogon.h>
#include <drogon/HttpFilter.h>
#include <string>

// JWT 鉴权过滤器
//
// 在 Drogon 控制器的 METHOD_LIST_BEGIN 中通过 ADD_METHOD_VIA_REGEX_ADDITIONAL_ARGUMENT
// 或直接在路由前加 Filter 来保护接口。
//
// 用法：
//   METHOD_LIST_BEGIN
//   ADD_METHOD_TO(AuthController::login, "/api/auth/login", Post);  // 无需鉴权
//   ADD_METHOD_TO(UserController::profile, "/api/user/profile", Get, JwtFilter);  // 需要鉴权
//   METHOD_LIST_END
//
// 通过后注入：
//   req->attributes()->get<int64_t>("userId")
//   req->attributes()->get<int>("role")
namespace online_chat {

class JwtFilter : public drogon::HttpFilter<JwtFilter>
{
public:
    void doFilter(const drogon::HttpRequestPtr& req,
                  drogon::FilterCallback&& fcb,
                  drogon::FilterChainCallback&& fccb) override
    {
        // 从 Authorization: Bearer <token> 提取
        const auto& authHeader = req->getHeader("Authorization");
        if (authHeader.empty() || authHeader.substr(0, 7) != "Bearer ")
        {
            fcb(Response::unauthorized("缺少 Authorization 头"));
            return;
        }

        std::string token = authHeader.substr(7);
        if (token.empty())
        {
            fcb(Response::unauthorized("token 为空"));
            return;
        }

        // 1. JWT 签名校验
        auto payload = JwtUtil::verify(token);
        if (!payload.has_value())
        {
            fcb(Response::unauthorized("token 无效或已过期"));
            return;
        }

        // 2. Redis session 校验（防旧 token）
        auto redis = RedisClient::get();
        std::string sessionKey = "session:" + std::to_string(payload->userId);
        redis->execCommandAsync(
            [req, fccb, fcb, token, payload]
            (const drogon::nosql::RedisResult& result)
            {
                if (result.type() != drogon::nosql::RedisResultType::kRedisString
                    || result.getString() != token)
                {
                    fcb(Response::unauthorized("session 已失效，请重新登录"));
                    return;
                }

                // 通过：注入 userId 和 role 到请求属性
                req->attributes()->set("userId", payload->userId);
                req->attributes()->set("role", payload->role);
                fccb();
            },
            [fcb](const drogon::nosql::RedisException& e)
            {
                LOG_ERROR << "Redis session check error: " << e.what();
                fcb(Response::fail(ErrorCode::INTERNAL_ERROR, "session 校验失败",
                                   k500InternalServerError));
            },
            "GET %s", sessionKey.c_str());
    }
};

}  // namespace online_chat
