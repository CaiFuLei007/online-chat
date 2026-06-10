#include "controllers/user_controller.h"
#include "services/user_service.h"
#include "utils/response.h"
#include "utils/config_drogon.h"

#include <drogon/drogon.h>

namespace online_chat {

// GET /api/user/profile — 获取当前用户资料
void UserController::myProfile(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
    int64_t userId = req->attributes()->get<int64_t>("userId");
    UserService::getProfile(userId,
        [callback](const Json::Value& data) { callback(Response::ok(data)); },
        [callback](ErrorCode code, const std::string& msg) { callback(Response::fail(code, msg)); });
}

// GET /api/user/profile/{id} — 获取指定用户资料
void UserController::userProfile(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    int64_t userId)
{
    UserService::getProfile(userId,
        [callback](const Json::Value& data) { callback(Response::ok(data)); },
        [callback](ErrorCode code, const std::string& msg) { callback(Response::fail(code, msg)); });
}

// GET /api/user/search?keyword=xxx&page=1 — 按昵称搜索
void UserController::search(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
    int64_t currentUserId = req->attributes()->get<int64_t>("userId");
    std::string keyword = req->getParameter("keyword");
    int page = 1;
    int pageSize = ConfigDrogon::getInt("", "search.page_size", 20);
    try { page = std::stoi(req->getParameter("page")); } catch (...) {}
    if (page < 1) page = 1;

    UserService::searchByNickname(keyword, page, pageSize, currentUserId,
        [callback](const Json::Value& data) { callback(Response::ok(data)); },
        [callback](ErrorCode code, const std::string& msg) { callback(Response::fail(code, msg)); });
}

}  // namespace online_chat
