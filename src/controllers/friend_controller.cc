#include "controllers/friend_controller.h"
#include "services/friend_service.h"
#include "utils/response.h"

#include <drogon/drogon.h>
#include <json/json.h>

namespace online_chat {

// POST /api/friend/request
void FriendController::sendRequest(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
    int64_t fromUser = req->attributes()->get<int64_t>("userId");
    auto json = req->getJsonObject();
    if (!json || !json->isMember("userId"))
    {
        callback(Response::fail(ErrorCode::INVALID_PARAMS, "缺少 userId"));
        return;
    }
    int64_t toUser = (*json)["userId"].asInt64();
    std::string message = json->isMember("message") ? (*json)["message"].asString() : "";

    FriendService::sendRequest(fromUser, toUser, message,
        [callback](const Json::Value& data) { callback(Response::ok(data)); },
        [callback](ErrorCode code, const std::string& msg) { callback(Response::fail(code, msg)); });
}

// GET /api/friend/requests
void FriendController::listRequests(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
    int64_t userId = req->attributes()->get<int64_t>("userId");
    FriendService::listPendingRequests(userId,
        [callback](const Json::Value& data) { callback(Response::ok(data)); },
        [callback](ErrorCode code, const std::string& msg) { callback(Response::fail(code, msg)); });
}

// POST /api/friend/accept/{id}
void FriendController::acceptRequest(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    int64_t requestId)
{
    int64_t userId = req->attributes()->get<int64_t>("userId");
    FriendService::handleRequest(requestId, userId, true,
        [callback](const Json::Value& data) { callback(Response::ok(data)); },
        [callback](ErrorCode code, const std::string& msg) { callback(Response::fail(code, msg)); });
}

// POST /api/friend/reject/{id}
void FriendController::rejectRequest(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    int64_t requestId)
{
    int64_t userId = req->attributes()->get<int64_t>("userId");
    FriendService::handleRequest(requestId, userId, false,
        [callback](const Json::Value& data) { callback(Response::ok(data)); },
        [callback](ErrorCode code, const std::string& msg) { callback(Response::fail(code, msg)); });
}

// GET /api/friend/list
void FriendController::listFriends(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
    int64_t userId = req->attributes()->get<int64_t>("userId");
    FriendService::listFriends(userId,
        [callback](const Json::Value& data) { callback(Response::ok(data)); },
        [callback](ErrorCode code, const std::string& msg) { callback(Response::fail(code, msg)); });
}

// DELETE /api/friend/{id}
void FriendController::removeFriend(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    int64_t friendId)
{
    int64_t userId = req->attributes()->get<int64_t>("userId");
    FriendService::removeFriend(userId, friendId,
        [callback](const Json::Value& data) { callback(Response::ok(data)); },
        [callback](ErrorCode code, const std::string& msg) { callback(Response::fail(code, msg)); });
}

}  // namespace online_chat
