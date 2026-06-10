#include "controllers/group_controller.h"
#include "services/group_service.h"
#include "utils/response.h"
#include "utils/config_drogon.h"

#include <drogon/drogon.h>
#include <json/json.h>

namespace online_chat {

void GroupController::createGroup(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
    int64_t userId = req->attributes()->get<int64_t>("userId");
    auto json = req->getJsonObject();
    if (!json || !json->isMember("name"))
    {
        callback(Response::fail(ErrorCode::INVALID_PARAMS, "缺少群名"));
        return;
    }
    std::string name = (*json)["name"].asString();
    GroupService::createGroup(name, userId,
        [callback](const Json::Value& data) { callback(Response::ok(data)); },
        [callback](ErrorCode code, const std::string& msg) { callback(Response::fail(code, msg)); });
}

void GroupController::search(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
    std::string keyword = req->getParameter("keyword");
    int page = 1;
    int pageSize = ConfigDrogon::getInt("", "search.page_size", 20);
    try { page = std::stoi(req->getParameter("page")); } catch (...) {}
    if (page < 1) page = 1;

    GroupService::searchByName(keyword, page, pageSize,
        [callback](const Json::Value& data) { callback(Response::ok(data)); },
        [callback](ErrorCode code, const std::string& msg) { callback(Response::fail(code, msg)); });
}

void GroupController::joinRequest(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    int64_t groupId)
{
    int64_t userId = req->attributes()->get<int64_t>("userId");
    GroupService::sendRequest(groupId, userId,
        [callback](const Json::Value& data) { callback(Response::ok(data)); },
        [callback](ErrorCode code, const std::string& msg) { callback(Response::fail(code, msg)); });
}

void GroupController::listRequests(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    int64_t groupId)
{
    int64_t userId = req->attributes()->get<int64_t>("userId");
    GroupService::listPendingRequests(groupId, userId,
        [callback](const Json::Value& data) { callback(Response::ok(data)); },
        [callback](ErrorCode code, const std::string& msg) { callback(Response::fail(code, msg)); });
}

void GroupController::acceptRequest(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    int64_t requestId)
{
    int64_t userId = req->attributes()->get<int64_t>("userId");
    GroupService::handleRequest(requestId, userId, true,
        [callback](const Json::Value& data) { callback(Response::ok(data)); },
        [callback](ErrorCode code, const std::string& msg) { callback(Response::fail(code, msg)); });
}

void GroupController::rejectRequest(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    int64_t requestId)
{
    int64_t userId = req->attributes()->get<int64_t>("userId");
    GroupService::handleRequest(requestId, userId, false,
        [callback](const Json::Value& data) { callback(Response::ok(data)); },
        [callback](ErrorCode code, const std::string& msg) { callback(Response::fail(code, msg)); });
}

void GroupController::leaveGroup(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    int64_t groupId)
{
    int64_t userId = req->attributes()->get<int64_t>("userId");
    GroupService::leaveGroup(groupId, userId,
        [callback](const Json::Value& data) { callback(Response::ok(data)); },
        [callback](ErrorCode code, const std::string& msg) { callback(Response::fail(code, msg)); });
}

void GroupController::listMembers(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    int64_t groupId)
{
    GroupService::listMembers(groupId,
        [callback](const Json::Value& data) { callback(Response::ok(data)); },
        [callback](ErrorCode code, const std::string& msg) { callback(Response::fail(code, msg)); });
}

void GroupController::dissolve(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    int64_t groupId)
{
    int64_t userId = req->attributes()->get<int64_t>("userId");
    GroupService::dissolveGroup(groupId, userId,
        [callback](const Json::Value& data) { callback(Response::ok(data)); },
        [callback](ErrorCode code, const std::string& msg) { callback(Response::fail(code, msg)); });
}

void GroupController::myGroups(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
    int64_t userId = req->attributes()->get<int64_t>("userId");
    GroupService::myGroups(userId,
        [callback](const Json::Value& data) { callback(Response::ok(data)); },
        [callback](ErrorCode code, const std::string& msg) { callback(Response::fail(code, msg)); });
}

}  // namespace online_chat
