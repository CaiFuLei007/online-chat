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
                  drogon::FilterChainCallback&& fccb) override;
};

}  // namespace online_chat
