#pragma once

#include <drogon/HttpController.h>
using drogon::Get;
using drogon::Post;
using drogon::Delete;
using drogon::Put;

// 认证控制器
//
// 提供三个 HTTP 接口：
//   POST /api/auth/send-code   发送验证码
//   POST /api/auth/register    注册
//   POST /api/auth/login       登录
namespace online_chat {

class AuthController : public drogon::HttpController<AuthController>
{
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(AuthController::sendCode,  "/api/auth/send-code", Post);
    ADD_METHOD_TO(AuthController::register_, "/api/auth/register",  Post);
    ADD_METHOD_TO(AuthController::login,     "/api/auth/login",     Post);
    METHOD_LIST_END

    void sendCode(const drogon::HttpRequestPtr& req,
                  std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    void register_(const drogon::HttpRequestPtr& req,
                   std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    void login(const drogon::HttpRequestPtr& req,
               std::function<void(const drogon::HttpResponsePtr&)>&& callback);
};

}  // namespace online_chat
