#include "controllers/auth_controller.h"
#include "services/auth_service.h"
#include "utils/response.h"

#include <drogon/drogon.h>
#include <json/json.h>

using drogon::k400BadRequest;
using drogon::k401Unauthorized;
using drogon::k403Forbidden;
using drogon::k429TooManyRequests;

namespace online_chat {

// POST /api/auth/send-code
void AuthController::sendCode(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
    auto json = req->getJsonObject();
    if (!json || !json->isMember("email"))
    {
        callback(Response::fail(ErrorCode::INVALID_PARAMS, "缺少 email 参数"));
        return;
    }
    std::string email = (*json)["email"].asString();
    if (email.empty())
    {
        callback(Response::fail(ErrorCode::INVALID_PARAMS, "email 不能为空"));
        return;
    }

    AuthService::sendVerifyCode(
        email,
        [callback](const Json::Value& data)
        {
            callback(Response::ok(data));
        },
        [callback](ErrorCode code, const std::string& msg)
        {
            auto status = (code == ErrorCode::RESEND_TOO_FAST) ? k429TooManyRequests : k400BadRequest;
            callback(Response::fail(code, msg, status));
        });
}

// POST /api/auth/register
void AuthController::register_(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
    auto json = req->getJsonObject();
    if (!json)
    {
        callback(Response::fail(ErrorCode::INVALID_PARAMS, "请求体为空"));
        return;
    }

    // 必填字段校验
    std::vector<std::string> required = {"email", "code", "password"};
    for (const auto& field : required)
    {
        if (!json->isMember(field) || (*json)[field].asString().empty())
        {
            callback(Response::fail(ErrorCode::INVALID_PARAMS, "缺少必填字段: " + field));
            return;
        }
    }

    std::string email    = (*json)["email"].asString();
    std::string code     = (*json)["code"].asString();
    std::string password = (*json)["password"].asString();
    // nickname 可选，默认用邮箱前缀
    std::string nickname = json->isMember("nickname") ? (*json)["nickname"].asString() : email.substr(0, email.find('@'));

    AuthService::registerUser(
        email, code, password, nickname,
        [callback](const Json::Value& data)
        {
            callback(Response::ok("注册成功", data));
        },
        [callback](ErrorCode code, const std::string& msg)
        {
            callback(Response::fail(code, msg));
        });
}

// POST /api/auth/login
void AuthController::login(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
    auto json = req->getJsonObject();
    if (!json)
    {
        callback(Response::fail(ErrorCode::INVALID_PARAMS, "请求体为空"));
        return;
    }

    if (!json->isMember("email") || !json->isMember("password"))
    {
        callback(Response::fail(ErrorCode::INVALID_PARAMS, "缺少 email 或 password"));
        return;
    }

    std::string email    = (*json)["email"].asString();
    std::string password = (*json)["password"].asString();

    AuthService::login(
        email, password,
        [callback](const Json::Value& data)
        {
            callback(Response::ok("登录成功", data));
        },
        [callback](ErrorCode code, const std::string& msg)
        {
            auto status = (code == ErrorCode::ACCOUNT_DISABLED) ? k403Forbidden : k401Unauthorized;
            callback(Response::fail(code, msg, status));
        });
}

}  // namespace online_chat
