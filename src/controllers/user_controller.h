#pragma once

#include <drogon/HttpController.h>

// 用户控制器
//
// GET  /api/user/profile        获取当前用户资料
// GET  /api/user/profile/{id}   获取指定用户资料
// GET  /api/user/search         按昵称模糊搜索
namespace online_chat {

class UserController : public drogon::HttpController<UserController>
{
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(UserController::myProfile,       "/api/user/profile",      Get, "JwtFilter");
    ADD_METHOD_TO(UserController::userProfile,     "/api/user/profile/{1}",  Get, "JwtFilter");
    ADD_METHOD_TO(UserController::search,          "/api/user/search",       Get, "JwtFilter");
    METHOD_LIST_END

    void myProfile(const drogon::HttpRequestPtr& req,
                   std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    void userProfile(const drogon::HttpRequestPtr& req,
                     std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                     int64_t userId);

    void search(const drogon::HttpRequestPtr& req,
                std::function<void(const drogon::HttpResponsePtr&)>&& callback);
};

}  // namespace online_chat
