// doFilter 定义放在 .cc（out-of-line 虚函数），确保编译器在本翻译单元
// 生成 vtable 与 DrObject 静态注册符号，否则 Drogon 反射找不到该过滤器
#include "filters/jwt_filter.h"

#include "utils/jwt_util.h"
#include "utils/response.h"
#include "dao/redis_client.h"

namespace online_chat {

void JwtFilter::doFilter(const drogon::HttpRequestPtr& req,
                         drogon::FilterCallback&& fcb,
                         drogon::FilterChainCallback&& fccb)
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
            if (result.type() != drogon::nosql::RedisResultType::kString
                || result.asString() != token)
            {
                fcb(Response::unauthorized("session 已失效，请重新登录"));
                return;
            }

            // 通过：注入 userId 和 role 到请求属性
            req->attributes()->insert("userId", payload->userId);
            req->attributes()->insert("role", payload->role);
            fccb();
        },
        [fcb](const drogon::nosql::RedisException& e)
        {
            LOG_ERROR << "Redis session check error: " << e.what();
            fcb(Response::fail(ErrorCode::INTERNAL_ERROR, "session 校验失败",
                               drogon::k500InternalServerError));
        },
        "GET %s", sessionKey.c_str());
}

}  // namespace online_chat
