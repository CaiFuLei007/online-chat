#include "controllers/admin_controller.h"
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

    // 查总数
    db->execSqlAsync(
        [db, offset, pageSize, page, onSuccess =
            [callback](const Json::Value& data) { callback(Response::ok(data)); },
            onError = [callback](ErrorCode code, const std::string& msg)
            { callback(Response::fail(code, msg)); }
        ](const drogon::orm::Result& countResult)
        {
            int total = countResult[0][0].as<int>();

            // 查列表（含群主昵称）
            db->execSqlAsync(
                [total, page, onSuccess](const drogon::orm::Result& result)
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
                    onSuccess(data);
                },
                [onError](const drogon::orm::DrogonDbException& e)
                {
                    LOG_ERROR << "DB error in admin listGroups: " << e.base().what();
                    onError(ErrorCode::INTERNAL_ERROR, "数据库错误");
                },
                "SELECT gc.id, gc.name, gc.owner_id, u.nickname AS owner_name, "
                "gc.member_count, gc.created_at "
                "FROM group_chats gc LEFT JOIN users u ON gc.owner_id = u.id "
                "ORDER BY gc.id DESC LIMIT ? OFFSET ?",
                pageSize, offset);
        },
        [callback](const drogon::orm::DrogonDbException& e)
        {
            LOG_ERROR << "DB error in admin listGroups count: " << e.base().what();
            callback(Response::fail(ErrorCode::INTERNAL_ERROR, "数据库错误"));
        },
        "SELECT COUNT(*) FROM group_chats");
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

    // 复用 GroupService 的注销逻辑（传 userId=0 表示超管操作，跳过群主校验）
    // 但 GroupService::dissolveGroup 会校验 ownerId == currentUserId
    // 所以这里直接调用 DB 硬删，与 GroupService 逻辑一致
    auto db = DbClient::get();
    db->execSqlAsync(
        [groupId, callback](const drogon::orm::Result& result)
        {
            if (result.empty())
            {
                callback(Response::notFound("群不存在"));
                return;
            }

            // 硬删：messages → members → group
            auto db2 = DbClient::get();
            db2->execSqlAsync(
                [groupId, callback](const drogon::orm::Result&)
                {
                    auto db3 = DbClient::get();
                    db3->execSqlAsync(
                        [groupId, callback](const drogon::orm::Result&)
                        {
                            auto db4 = DbClient::get();
                            db4->execSqlAsync(
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
                                "DELETE FROM group_chats WHERE id=?",
                                groupId);
                        },
                        [callback](const drogon::orm::DrogonDbException& e)
                        {
                            LOG_ERROR << "DB error: " << e.base().what();
                            callback(Response::fail(ErrorCode::INTERNAL_ERROR, "数据库错误"));
                        },
                        "DELETE FROM group_members WHERE group_id=?",
                        groupId);
                },
                [callback](const drogon::orm::DrogonDbException& e)
                {
                    LOG_ERROR << "DB error: " << e.base().what();
                    callback(Response::fail(ErrorCode::INTERNAL_ERROR, "数据库错误"));
                },
                "DELETE FROM group_messages WHERE group_id=?",
                groupId);
        },
        [callback](const drogon::orm::DrogonDbException& e)
        {
            LOG_ERROR << "DB error: " << e.base().what();
            callback(Response::fail(ErrorCode::INTERNAL_ERROR, "数据库错误"));
        },
        "SELECT id FROM group_chats WHERE id=?",
        groupId);
}

}  // namespace online_chat
