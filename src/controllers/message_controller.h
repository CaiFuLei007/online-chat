#pragma once

#include <drogon/HttpController.h>

// 消息控制器
//
// GET  /api/message/single/history?peerId=&beforeSeq=&limit=  单聊历史
// GET  /api/message/group/history?groupId=&beforeSeq=&limit=  群聊历史
// GET  /api/message/offline                                   拉取离线消息
namespace online_chat {

class MessageController : public drogon::HttpController<MessageController>
{
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(MessageController::singleHistory, "/api/message/single/history", Get, "JwtFilter");
    ADD_METHOD_TO(MessageController::groupHistory,  "/api/message/group/history",  Get, "JwtFilter");
    ADD_METHOD_TO(MessageController::pullOffline,   "/api/message/offline",        Get, "JwtFilter");
    METHOD_LIST_END

    void singleHistory(const drogon::HttpRequestPtr& req,
                       std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    void groupHistory(const drogon::HttpRequestPtr& req,
                      std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    void pullOffline(const drogon::HttpRequestPtr& req,
                     std::function<void(const drogon::HttpResponsePtr&)>&& callback);
};

}  // namespace online_chat
