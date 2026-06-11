#include "controllers/admin_controller.h"
#include "utils/sql_util.h"
#include "services/group_service.h"
#include "dao/db_client.h"
#include "utils/response.h"
#include "utils/config_drogon.h"

#include <drogon/drogon.h>

namespace online_chat {

// 验证当前用户是否为超管
static bool isAdmin(const drogon::HttpRequestPtr& req)
{
    int role = req->attributes()->get<int>("role");
    return role == 1;
}

// GET /api/admin/groups?page=1
void AdminController::listGroups(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
    if (!isAdmin(req))
    {
        callback(Response::forbidden("仅超管可访问"));
        return;
    }

    int page = 1;
    int pageSize = ConfigDrogon::getInt("", "search.page_size", 20);
    try { page = std::stoi(req->getParameter("page")); } catch (...) {}
    if (page < 1) page = 1;

    int offset = (page - 1) * pageSize;
    auto db = DbClient::get();

    db->execSqlAsync(
        SQL("SELECT COUNT(*) FROM group_chats"),
        [db,
        offset,
        pageSize,
        page,
        callback]
        (const drogon::orm::Result& countResult)
        {
            int total = countResult[0][0].as<int>();

            db->execSqlAsync(
                SQL("SELECT gc.id, gc.name, gc.owner_id, u.nickname AS owner_name, "
                "gc.member_count, gc.created_at "
                "FROM group_chats gc LEFT JOIN users u ON gc.owner_id = u.id "
                "ORDER BY gc.id DESC LIMIT ? OFFSET ?"),
                [total,
                page,
                pageSize,
                callback]
                (const drogon::orm::Result& result)
                {
                    Json::Value list(Json::arrayValue);
                    for (const auto& row : result)
                    {
                        Json::Value g;
                        g["id"]          = Json::Int64(row["id"].as<int64_t>());
                        g["name"]        = row["name"].as<std::string>();
                        g["ownerId"]     = Json::Int64(row["owner_id"].as<int64_t>());
                        g["ownerName"]   = row["owner_name"].as<std::string>();
                        g["memberCount"] = row["member_count"].as<int>();
                        g["createdAt"]   = row["created_at"].as<std::string>();
                        list.append(g);
                    }
                    Json::Value data;
                    data["list"]     = list;
                    data["total"]    = total;
                    data["page"]     = page;
                    data["pageSize"] = pageSize;
                    callback(Response::ok(data));
                },
                [callback](const drogon::orm::DrogonDbException& e)
                {
                    LOG_ERROR << "DB error in admin listGroups: " << e.base().what();
                    callback(Response::fail(ErrorCode::INTERNAL_ERROR, "数据库错误"));
                },
                pageSize,
                offset);
        },
        [callback](const drogon::orm::DrogonDbException& e)
        {
            LOG_ERROR << "DB error in admin listGroups count: " << e.base().what();
            callback(Response::fail(ErrorCode::INTERNAL_ERROR, "数据库错误"));
        });
}

// DELETE /api/admin/groups/{id}
void AdminController::dissolve(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    int64_t groupId)
{
    if (!isAdmin(req))
    {
        callback(Response::forbidden("仅超管可访问"));
        return;
    }

    auto db = DbClient::get();
    db->execSqlAsync(
        SQL("SELECT id FROM group_chats WHERE id=?"),
        [groupId,
        callback](const drogon::orm::Result& result)
        {
            if (result.empty())
            {
                callback(Response::notFound("群不存在"));
                return;
            }

            // 硬删：messages → members → group
            auto db2 = DbClient::get();
            db2->execSqlAsync(
                SQL("DELETE FROM group_messages WHERE group_id=?"),
                [groupId,
                callback](const drogon::orm::Result&)
                {
                    auto db3 = DbClient::get();
                    db3->execSqlAsync(
                        SQL("DELETE FROM group_members WHERE group_id=?"),
                        [groupId,
                        callback](const drogon::orm::Result&)
                        {
                            auto db4 = DbClient::get();
                            db4->execSqlAsync(
                                SQL("DELETE FROM group_chats WHERE id=?"),
                                [callback](const drogon::orm::Result&)
                                {
                                    Json::Value data;
                                    data["message"] = "群已注销";
                                    callback(Response::ok(data));
                                },
                                [callback](const drogon::orm::DrogonDbException& e)
                                {
                                    LOG_ERROR << "DB error: " << e.base().what();
                                    callback(Response::fail(ErrorCode::INTERNAL_ERROR, "数据库错误"));
                                },
                                groupId);
                        },
                        [callback](const drogon::orm::DrogonDbException& e)
                        {
                            LOG_ERROR << "DB error: " << e.base().what();
                            callback(Response::fail(ErrorCode::INTERNAL_ERROR, "数据库错误"));
                        },
                        groupId);
                },
                [callback](const drogon::orm::DrogonDbException& e)
                {
                    LOG_ERROR << "DB error: " << e.base().what();
                    callback(Response::fail(ErrorCode::INTERNAL_ERROR, "数据库错误"));
                },
                groupId);
        },
        [callback](const drogon::orm::DrogonDbException& e)
        {
            LOG_ERROR << "DB error: " << e.base().what();
            callback(Response::fail(ErrorCode::INTERNAL_ERROR, "数据库错误"));
        },
        groupId);
}

}  // namespace online_chat
