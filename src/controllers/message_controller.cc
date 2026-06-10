#include "controllers/message_controller.h"
#include "services/message_service.h"
#include "utils/response.h"

#include <drogon/drogon.h>

namespace online_chat {

// GET /api/message/single/history?peerId=xxx&beforeSeq=0&limit=30
void MessageController::singleHistory(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
    int64_t userId = req->attributes()->get<int64_t>("userId");
    int64_t peerId = 0;
    int64_t beforeSeq = 0;
    int limit = 30;

    try { peerId = std::stoll(req->getParameter("peerId")); } catch (...) {}
    try { beforeSeq = std::stoll(req->getParameter("beforeSeq")); } catch (...) {}
    try { limit = std::stoi(req->getParameter("limit")); } catch (...) {}

    if (peerId <= 0)
    {
        callback(Response::fail(ErrorCode::INVALID_PARAMS, "缺少 peerId"));
        return;
    }

    MessageService::getSingleHistory(userId, peerId, beforeSeq, limit,
        [callback](const Json::Value& data) { callback(Response::ok(data)); },
        [callback](ErrorCode code, const std::string& msg) { callback(Response::fail(code, msg)); });
}

// GET /api/message/group/history?groupId=xxx&beforeSeq=0&limit=30
void MessageController::groupHistory(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
    int64_t userId = req->attributes()->get<int64_t>("userId");
    int64_t groupId = 0;
    int64_t beforeSeq = 0;
    int limit = 30;

    try { groupId = std::stoll(req->getParameter("groupId")); } catch (...) {}
    try { beforeSeq = std::stoll(req->getParameter("beforeSeq")); } catch (...) {}
    try { limit = std::stoi(req->getParameter("limit")); } catch (...) {}

    if (groupId <= 0)
    {
        callback(Response::fail(ErrorCode::INVALID_PARAMS, "缺少 groupId"));
        return;
    }

    MessageService::getGroupHistory(userId, groupId, beforeSeq, limit,
        [callback](const Json::Value& data) { callback(Response::ok(data)); },
        [callback](ErrorCode code, const std::string& msg) { callback(Response::fail(code, msg)); });
}

// GET /api/message/offline
void MessageController::pullOffline(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
    int64_t userId = req->attributes()->get<int64_t>("userId");
    MessageService::pullOfflineMessages(userId,
        [callback](const Json::Value& data) { callback(Response::ok(data)); },
        [callback](ErrorCode code, const std::string& msg) { callback(Response::fail(code, msg)); });
}

}  // namespace online_chat
