#pragma once

#include <drogon/HttpController.h>
using drogon::Get;
using drogon::Post;
using drogon::Delete;
using drogon::Put;

// 群聊控制器
//
// POST   /api/group/create              创建群
// GET    /api/group/search?keyword=      按群名搜索
// POST   /api/group/join/{id}            发送加群申请
// GET    /api/group/requests/{id}        待处理申请（群主）
// POST   /api/group/accept/{id}          同意申请（群主）
// POST   /api/group/reject/{id}          拒绝申请（群主）
// POST   /api/group/leave/{id}           退群
// GET    /api/group/members/{id}         群成员列表
// DELETE /api/group/{id}                 注销群（群主）
// GET    /api/group/my                   我加入的群
namespace online_chat {

class GroupController : public drogon::HttpController<GroupController>
{
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(GroupController::createGroup,   "/api/group/create",        Post,   "online_chat::JwtFilter");
    ADD_METHOD_TO(GroupController::search,        "/api/group/search",        Get,    "online_chat::JwtFilter");
    ADD_METHOD_TO(GroupController::joinRequest,   "/api/group/join/{1}",      Post,   "online_chat::JwtFilter");
    ADD_METHOD_TO(GroupController::listRequests,  "/api/group/requests/{1}",  Get,    "online_chat::JwtFilter");
    ADD_METHOD_TO(GroupController::acceptRequest, "/api/group/accept/{1}",    Post,   "online_chat::JwtFilter");
    ADD_METHOD_TO(GroupController::rejectRequest, "/api/group/reject/{1}",    Post,   "online_chat::JwtFilter");
    ADD_METHOD_TO(GroupController::leaveGroup,    "/api/group/leave/{1}",     Post,   "online_chat::JwtFilter");
    ADD_METHOD_TO(GroupController::listMembers,   "/api/group/members/{1}",   Get,    "online_chat::JwtFilter");
    ADD_METHOD_TO(GroupController::dissolve,      "/api/group/{1}",           Delete, "online_chat::JwtFilter");
    ADD_METHOD_TO(GroupController::myGroups,      "/api/group/my",            Get,    "online_chat::JwtFilter");
    METHOD_LIST_END

    void createGroup(const drogon::HttpRequestPtr& req,
                     std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    void search(const drogon::HttpRequestPtr& req,
                std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    void joinRequest(const drogon::HttpRequestPtr& req,
                     std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                     int64_t groupId);

    void listRequests(const drogon::HttpRequestPtr& req,
                      std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                      int64_t groupId);

    void acceptRequest(const drogon::HttpRequestPtr& req,
                       std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                       int64_t requestId);

    void rejectRequest(const drogon::HttpRequestPtr& req,
                       std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                       int64_t requestId);

    void leaveGroup(const drogon::HttpRequestPtr& req,
                    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                    int64_t groupId);

    void listMembers(const drogon::HttpRequestPtr& req,
                     std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                     int64_t groupId);

    void dissolve(const drogon::HttpRequestPtr& req,
                  std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                  int64_t groupId);

    void myGroups(const drogon::HttpRequestPtr& req,
                  std::function<void(const drogon::HttpResponsePtr&)>&& callback);
};

}  // namespace online_chat
