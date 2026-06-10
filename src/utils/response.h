#pragma once

#include "utils/errors.h"

#include <drogon/drogon.h>
#include <drogon/utils/Utilities.h>
#include <json/json.h>

namespace online_chat {

// 统一 JSON 响应结构
//
// 所有 HTTP 接口统一返回如下格式：
//   { "code": 0, "msg": "success", "data": { ... } }
//
// code=0 表示成功，非 0 对应 ErrorCode 枚举。
// 前端只需判断 code===0 即为成功，否则按 code 分支处理。
class Response
{
public:
    // 构造成功响应（可附带 data）
    static HttpResponsePtr ok(const Json::Value& data = Json::Value::null,
                              HttpStatusCode status = k200OK)
    {
        Json::Value body;
        body["code"] = 0;
        body["msg"]  = "success";
        body["data"] = data;
        return jsonResp(body, status);
    }

    // 构造成功响应（携带自定义 message，用于注册/发送验证码等需要提示的场景）
    static HttpResponsePtr ok(const std::string& message,
                              const Json::Value& data = Json::Value::null)
    {
        Json::Value body;
        body["code"] = 0;
        body["msg"]  = message;
        body["data"] = data;
        return jsonResp(body, k200OK);
    }

    // 构造失败响应
    static HttpResponsePtr fail(ErrorCode code,
                                HttpStatusCode status = k400BadRequest)
    {
        Json::Value body;
        body["code"] = static_cast<int>(code);
        body["msg"]  = errorMessage(code);
        body["data"] = Json::Value::null;
        return jsonResp(body, status);
    }

    // 构造失败响应（携带自定义 message 补充细节，如"邮箱 xxx 已注册"）
    static HttpResponsePtr fail(ErrorCode code,
                                const std::string& message,
                                HttpStatusCode status = k400BadRequest)
    {
        Json::Value body;
        body["code"] = static_cast<int>(code);
        body["msg"]  = message;
        body["data"] = Json::Value::null;
        return jsonResp(body, status);
    }

    // 401 未登录（token 无效/过期）专用，前端可统一跳转登录
    static HttpResponsePtr unauthorized(const std::string& message = "unauthorized")
    {
        return fail(ErrorCode::UNAUTHORIZED, message, k401Unauthorized);
    }

    // 403 权限不足
    static HttpResponsePtr forbidden(const std::string& message = "forbidden")
    {
        return fail(ErrorCode::FORBIDDEN, message, k403Forbidden);
    }

    // 404 资源不存在
    static HttpResponsePtr notFound(const std::string& message = "not found")
    {
        return fail(ErrorCode::NOT_FOUND, message, k404NotFound);
    }

private:
    // 构造 JSON 响应
    static HttpResponsePtr jsonResp(const Json::Value& body, HttpStatusCode status)
    {
        auto resp = HttpResponse::newHttpJsonResponse(body);
        resp->setStatusCode(status);
        return resp;
    }
};

}  // namespace online_chat
