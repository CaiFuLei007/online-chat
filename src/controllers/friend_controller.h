#pragma once

#include <drogon/HttpController.h>
using drogon::Get;
using drogon::Post;
using drogon::Delete;
using drogon::Put;

// 好友控制器
//
// POST   /api/friend/request          发送加好友申请
// GET    /api/friend/requests         获取待处理申请列表
// POST   /api/friend/accept/{id}      同意申请
// POST   /api/friend/reject/{id}      拒绝申请
// GET    /api/friend/list             好友列表（含在线状态）
// DELETE /api/friend/{id}             删除好友
namespace online_chat {

class FriendController : public drogon::HttpController<FriendController>
{
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(FriendController::sendRequest,    "/api/friend/request",       Post,  "online_chat::JwtFilter");
    ADD_METHOD_TO(FriendController::listRequests,   "/api/friend/requests",      Get,   "online_chat::JwtFilter");
    ADD_METHOD_TO(FriendController::acceptRequest,  "/api/friend/accept/{1}",    Post,  "online_chat::JwtFilter");
    ADD_METHOD_TO(FriendController::rejectRequest,  "/api/friend/reject/{1}",    Post,  "online_chat::JwtFilter");
    ADD_METHOD_TO(FriendController::listFriends,    "/api/friend/list",          Get,   "online_chat::JwtFilter");
    ADD_METHOD_TO(FriendController::removeFriend,   "/api/friend/{1}",           Delete,"online_chat::JwtFilter");
    METHOD_LIST_END

    void sendRequest(const drogon::HttpRequestPtr& req,
                     std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    void listRequests(const drogon::HttpRequestPtr& req,
                      std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    void acceptRequest(const drogon::HttpRequestPtr& req,
                       std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                       int64_t requestId);

    void rejectRequest(const drogon::HttpRequestPtr& req,
                       std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                       int64_t requestId);

    void listFriends(const drogon::HttpRequestPtr& req,
                     std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    void removeFriend(const drogon::HttpRequestPtr& req,
                      std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                      int64_t friendId);
};

}  // namespace online_chat
