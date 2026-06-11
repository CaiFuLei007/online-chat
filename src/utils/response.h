#pragma once

#include "utils/errors.h"

#include <drogon/HttpResponse.h>
#include <drogon/HttpAppFramework.h>
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
    static drogon::HttpResponsePtr ok(const Json::Value& data = Json::Value::null,
                              drogon::HttpStatusCode status = drogon::k200OK)
    {
        Json::Value body;
        body["code"] = 0;
        body["msg"]  = "success";
        body["data"] = data;
        return jsonResp(body, status);
    }

    // 构造成功响应（携带自定义 message）
    static drogon::HttpResponsePtr ok(const std::string& message,
                              const Json::Value& data = Json::Value::null)
    {
        Json::Value body;
        body["code"] = 0;
        body["msg"]  = message;
        body["data"] = data;
        return jsonResp(body, drogon::k200OK);
    }

    // 构造失败响应
    static drogon::HttpResponsePtr fail(ErrorCode code,
                                drogon::HttpStatusCode status = drogon::k400BadRequest)
    {
        Json::Value body;
        body["code"] = static_cast<int>(code);
        body["msg"]  = errorMessage(code);
        body["data"] = Json::Value::null;
        return jsonResp(body, status);
    }

    // 构造失败响应（携带自定义 message）
    static drogon::HttpResponsePtr fail(ErrorCode code,
                                const std::string& message,
                                drogon::HttpStatusCode status = drogon::k400BadRequest)
    {
        Json::Value body;
        body["code"] = static_cast<int>(code);
        body["msg"]  = message;
        body["data"] = Json::Value::null;
        return jsonResp(body, status);
    }

    // 401 未登录
    static drogon::HttpResponsePtr unauthorized(const std::string& message = "unauthorized")
    {
        return fail(ErrorCode::UNAUTHORIZED, message, drogon::k401Unauthorized);
    }

    // 403 权限不足
    static drogon::HttpResponsePtr forbidden(const std::string& message = "forbidden")
    {
        return fail(ErrorCode::FORBIDDEN, message, drogon::k403Forbidden);
    }

    // 404 资源不存在
    static drogon::HttpResponsePtr notFound(const std::string& message = "not found")
    {
        return fail(ErrorCode::NOT_FOUND, message, drogon::k404NotFound);
    }

private:
    static drogon::HttpResponsePtr jsonResp(const Json::Value& body, drogon::HttpStatusCode status)
    {
        auto resp = drogon::HttpResponse::newHttpJsonResponse(body);
        resp->setStatusCode(status);
        return resp;
    }
};

}  // namespace online_chat
