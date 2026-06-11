#pragma once

#include <drogon/HttpController.h>

// 超管管理控制器
//
// GET    /api/admin/groups?page=      全部群列表（元数据 + 分页）
// DELETE /api/admin/groups/{id}       注销任意群
namespace online_chat {

class AdminController : public drogon::HttpController<AdminController>
{
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(AdminController::listGroups,  "/api/admin/groups",    Get,    "JwtFilter");
    ADD_METHOD_TO(AdminController::dissolve,    "/api/admin/groups/{1}",Delete, "JwtFilter");
    METHOD_LIST_END

    void listGroups(const drogon::HttpRequestPtr& req,
                    std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    void dissolve(const drogon::HttpRequestPtr& req,
                  std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                  int64_t groupId);
};

}  // namespace online_chat
