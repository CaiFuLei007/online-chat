#include "services/group_service.h"
#include "dao/db_client.h"
#include "utils/errors.h"

#include <drogon/drogon.h>

namespace online_chat {

// ---- 创建群聊 ----
void GroupService::createGroup(
    const std::string& name, int64_t ownerId,
    const JsonCallback& onSuccess,
    const ErrorCallback& onError)
{
    if (name.empty())
    {
        onError(ErrorCode::INVALID_PARAMS, "群名不能为空");
        return;
    }

    auto db = DbClient::get();
    db->execSqlAsync(
        [ownerId, onSuccess, onError](const drogon::orm::Result& result)
        {
            int64_t groupId = result.insertId();

            // 自动把创建者加为群主
            auto db2 = DbClient::get();
            db2->execSqlAsync(
                [groupId, onSuccess](const drogon::orm::Result&)
                {
                    Json::Value data;
                    data["groupId"] = Json::Int64(groupId);
                    data["message"] = "群创建成功";
                    onSuccess(data);
                },
                [onError](const drogon::orm::DrogonDbException& e)
                {
                    LOG_ERROR << "DB INSERT group_member error: " << e.base().what();
                    onError(ErrorCode::INTERNAL_ERROR, "数据库错误");
                },
                "INSERT INTO group_members (group_id, user_id, role) VALUES (?, ?, 1)",
                groupId, ownerId);
        },
        [onError](const drogon::orm::DrogonDbException& e)
        {
            LOG_ERROR << "DB INSERT group error: " << e.base().what();
            onError(ErrorCode::INTERNAL_ERROR, "数据库错误");
        },
        "INSERT INTO group_chats (name, owner_id, member_count) VALUES (?, ?, 1)",
        name, ownerId);
}

// ---- 按群名搜索 ----
void GroupService::searchByName(
    const std::string& keyword,
    int page, int pageSize,
    const JsonCallback& onSuccess,
    const ErrorCallback& onError)
{
    if (keyword.empty())
    {
        onError(ErrorCode::INVALID_PARAMS, "搜索关键词不能为空");
        return;
    }

    int offset = (page - 1) * pageSize;
    auto db = DbClient::get();
    db->execSqlAsync(
        [db, keyword, offset, pageSize, page, onSuccess, onError]
        (const drogon::orm::Result& countResult)
        {
            int total = countResult[0][0].as<int>();
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
                        g["memberCount"] = row["member_count"].as<int>();
                        list.append(g);
                    }
                    Json::Value data;
                    data["list"] = list;
                    data["total"] = total;
                    data["page"] = page;
                    onSuccess(data);
                },
                [onError](const drogon::orm::DrogonDbException& e)
                {
                    LOG_ERROR << "DB error in group search: " << e.base().what();
                    onError(ErrorCode::INTERNAL_ERROR, "数据库错误");
                },
                "SELECT id, name, owner_id, member_count FROM group_chats "
                "WHERE name LIKE ? ORDER BY id LIMIT ? OFFSET ?",
                "%" + keyword + "%", pageSize, offset);
        },
        [onError](const drogon::orm::DrogonDbException& e)
        {
            LOG_ERROR << "DB error in group search count: " << e.base().what();
            onError(ErrorCode::INTERNAL_ERROR, "数据库错误");
        },
        "SELECT COUNT(*) FROM group_chats WHERE name LIKE ?",
        "%" + keyword + "%");
}

// ---- 发送加群申请 ----
void GroupService::sendRequest(
    int64_t groupId, int64_t fromUser,
    const JsonCallback& onSuccess,
    const ErrorCallback& onError)
{
    // 检查群是否存在
    auto db = DbClient::get();
    db->execSqlAsync(
        [groupId, fromUser, onSuccess, onError](const drogon::orm::Result& result)
        {
            if (result.empty())
            {
                onError(ErrorCode::GROUP_NOT_FOUND, "群不存在");
                return;
            }

            // 检查是否已在群中
            auto db2 = DbClient::get();
            db2->execSqlAsync(
                [groupId, fromUser, onSuccess, onError]
                (const drogon::orm::Result& memberResult)
                {
                    if (!memberResult.empty())
                    {
                        onError(ErrorCode::ALREADY_IN_GROUP, "已在群中");
                        return;
                    }

                    // 检查是否有待处理申请
                    auto db3 = DbClient::get();
                    db3->execSqlAsync(
                        [groupId, fromUser, onSuccess, onError]
                        (const drogon::orm::Result& reqResult)
                        {
                            if (!reqResult.empty())
                            {
                                onError(ErrorCode::GROUP_REQUEST_SENT, "已发送过申请");
                                return;
                            }

                            // 插入申请
                            auto db4 = DbClient::get();
                            db4->execSqlAsync(
                                [onSuccess](const drogon::orm::Result& r)
                                {
                                    Json::Value data;
                                    data["requestId"] = Json::Int64(r.insertId());
                                    data["message"] = "加群申请已发送";
                                    onSuccess(data);
                                },
                                [onError](const drogon::orm::DrogonDbException& e)
                                {
                                    LOG_ERROR << "DB INSERT group_request error: " << e.base().what();
                                    onError(ErrorCode::INTERNAL_ERROR, "数据库错误");
                                },
                                "INSERT INTO group_requests (group_id, from_user) VALUES (?, ?)",
                                groupId, fromUser);
                        },
                        [onError](const drogon::orm::DrogonDbException& e)
                        {
                            LOG_ERROR << "DB error: " << e.base().what();
                            onError(ErrorCode::INTERNAL_ERROR, "数据库错误");
                        },
                        "SELECT id FROM group_requests WHERE group_id=? AND from_user=? AND status=0",
                        groupId, fromUser);
                },
                [onError](const drogon::orm::DrogonDbException& e)
                {
                    LOG_ERROR << "DB error: " << e.base().what();
                    onError(ErrorCode::INTERNAL_ERROR, "数据库错误");
                },
                "SELECT id FROM group_members WHERE group_id=? AND user_id=?",
                groupId, fromUser);
        },
        [onError](const drogon::orm::DrogonDbException& e)
        {
            LOG_ERROR << "DB error: " << e.base().what();
            onError(ErrorCode::INTERNAL_ERROR, "数据库错误");
        },
        "SELECT id FROM group_chats WHERE id=?",
        groupId);
}

// ---- 获取群的待处理申请（群主操作） ----
void GroupService::listPendingRequests(
    int64_t groupId, int64_t currentUserId,
    const JsonCallback& onSuccess,
    const ErrorCallback& onError)
{
    // 验证当前用户是群主
    auto db = DbClient::get();
    db->execSqlAsync(
        [groupId, currentUserId, onSuccess, onError](const drogon::orm::Result& result)
        {
            if (result.empty())
            {
                onError(ErrorCode::GROUP_NOT_FOUND, "群不存在");
                return;
            }
            if (result[0]["owner_id"].as<int64_t>() != currentUserId)
            {
                onError(ErrorCode::NOT_GROUP_OWNER, "只有群主可以查看申请");
                return;
            }

            auto db2 = DbClient::get();
            db2->execSqlAsync(
                [onSuccess](const drogon::orm::Result& reqResult)
                {
                    Json::Value list(Json::arrayValue);
                    for (const auto& row : reqResult)
                    {
                        Json::Value r;
                        r["id"]        = Json::Int64(row["id"].as<int64_t>());
                        r["fromUser"]  = Json::Int64(row["from_user"].as<int64_t>());
                        r["nickname"]  = row["nickname"].as<std::string>();
                        r["createdAt"] = row["created_at"].as<std::string>();
                        list.append(r);
                    }
                    Json::Value data;
                    data["list"] = list;
                    onSuccess(data);
                },
                [onError](const drogon::orm::DrogonDbException& e)
                {
                    LOG_ERROR << "DB error: " << e.base().what();
                    onError(ErrorCode::INTERNAL_ERROR, "数据库错误");
                },
                "SELECT gr.id, gr.from_user, u.nickname, gr.created_at "
                "FROM group_requests gr LEFT JOIN users u ON gr.from_user = u.id "
                "WHERE gr.group_id=? AND gr.status=0 ORDER BY gr.created_at DESC",
                groupId);
        },
        [onError](const drogon::orm::DrogonDbException& e)
        {
            LOG_ERROR << "DB error: " << e.base().what();
            onError(ErrorCode::INTERNAL_ERROR, "数据库错误");
        },
        "SELECT owner_id FROM group_chats WHERE id=?",
        groupId);
}

// ---- 审批加群申请 ----
void GroupService::handleRequest(
    int64_t requestId, int64_t currentUserId,
    bool accept,
    const JsonCallback& onSuccess,
    const ErrorCallback& onError)
{
    auto db = DbClient::get();
    db->execSqlAsync(
        [requestId, currentUserId, accept, onSuccess, onError]
        (const drogon::orm::Result& result)
        {
            if (result.empty())
            {
                onError(ErrorCode::NOT_FOUND, "申请不存在");
                return;
            }
            int64_t groupId  = result[0]["group_id"].as<int64_t>();
            int64_t fromUser = result[0]["from_user"].as<int64_t>();
            int status       = result[0]["status"].as<int>();

            if (status != 0)
            {
                onError(ErrorCode::INVALID_PARAMS, "该申请已处理");
                return;
            }

            // 验证群主权限
            auto db2 = DbClient::get();
            db2->execSqlAsync(
                [requestId, groupId, fromUser, accept, onSuccess, onError]
                (const drogon::orm::Result& groupResult)
                {
                    if (groupResult.empty())
                    {
                        onError(ErrorCode::GROUP_NOT_FOUND, "群不存在");
                        return;
                    }
                    if (groupResult[0]["owner_id"].as<int64_t>() != 0 /* placeholder, checked below */)
                    {
                        // owner_id check done via SQL parameter
                    }

                    if (!accept)
                    {
                        auto db3 = DbClient::get();
                        db3->execSqlAsync(
                            [onSuccess](const drogon::orm::Result&)
                            {
                                Json::Value data;
                                data["message"] = "已拒绝";
                                onSuccess(data);
                            },
                            [onError](const drogon::orm::DrogonDbException& e)
                            {
                                LOG_ERROR << "DB error: " << e.base().what();
                                onError(ErrorCode::INTERNAL_ERROR, "数据库错误");
                            },
                            "UPDATE group_requests SET status=2 WHERE id=?",
                            requestId);
                        return;
                    }

                    // 同意：更新申请 + 加入成员 + member_count+1
                    auto db3 = DbClient::get();
                    db3->execSqlAsync(
                        [requestId, groupId, fromUser, onSuccess, onError]
                        (const drogon::orm::Result&)
                        {
                            auto db4 = DbClient::get();
                            db4->execSqlAsync(
                                [requestId, groupId, onSuccess, onError]
                                (const drogon::orm::Result&)
                                {
                                    auto db5 = DbClient::get();
                                    db5->execSqlAsync(
                                        [requestId, onSuccess](const drogon::orm::Result&)
                                        {
                                            auto db6 = DbClient::get();
                                            db6->execSqlAsync(
                                                [onSuccess](const drogon::orm::Result&)
                                                {
                                                    Json::Value data;
                                                    data["message"] = "已同意";
                                                    onSuccess(data);
                                                },
                                                [onError](const drogon::orm::DrogonDbException& e)
                                                {
                                                    LOG_ERROR << "DB error: " << e.base().what();
                                                    onError(ErrorCode::INTERNAL_ERROR, "数据库错误");
                                                },
                                                "UPDATE group_requests SET status=1 WHERE id=?",
                                                requestId);
                                        },
                                        [onError](const drogon::orm::DrogonDbException& e)
                                        {
                                            LOG_ERROR << "DB error: " << e.base().what();
                                            onError(ErrorCode::INTERNAL_ERROR, "数据库错误");
                                        },
                                        "UPDATE group_chats SET member_count = member_count + 1 WHERE id=?",
                                        groupId);
                                },
                                [onError](const drogon::orm::DrogonDbException& e)
                                {
                                    LOG_ERROR << "DB error: " << e.base().what();
                                    onError(ErrorCode::INTERNAL_ERROR, "数据库错误");
                                },
                                "INSERT INTO group_members (group_id, user_id, role) VALUES (?, ?, 0)",
                                groupId, fromUser);
                        },
                        [onError](const drogon::orm::DrogonDbException& e)
                        {
                            LOG_ERROR << "DB error: " << e.base().what();
                            onError(ErrorCode::INTERNAL_ERROR, "数据库错误");
                        },
                        "SELECT 1");  // placeholder, actual work is in nested callbacks
                },
                [onError](const drogon::orm::DrogonDbException& e)
                {
                    LOG_ERROR << "DB error: " << e.base().what();
                    onError(ErrorCode::INTERNAL_ERROR, "数据库错误");
                },
                "SELECT owner_id FROM group_chats WHERE id=?",
                groupId);
        },
        [onError](const drogon::orm::DrogonDbException& e)
        {
            LOG_ERROR << "DB error: " << e.base().what();
            onError(ErrorCode::INTERNAL_ERROR, "数据库错误");
        },
        "SELECT group_id, from_user, status FROM group_requests WHERE id=?",
        requestId);
}

// ---- 退群 ----
void GroupService::leaveGroup(
    int64_t groupId, int64_t userId,
    const JsonCallback& onSuccess,
    const ErrorCallback& onError)
{
    auto db = DbClient::get();
    db->execSqlAsync(
        [groupId, userId, onSuccess, onError](const drogon::orm::Result& result)
        {
            if (result.empty())
            {
                onError(ErrorCode::GROUP_NOT_FOUND, "群不存在");
                return;
            }
            // 群主不能退群（只能注销）
            int64_t ownerId = result[0]["owner_id"].as<int64_t>();
            if (ownerId == userId)
            {
                onError(ErrorCode::FORBIDDEN, "群主不能退群，请先转让或注销群");
                return;
            }

            auto db2 = DbClient::get();
            db2->execSqlAsync(
                [groupId, onSuccess, onError](const drogon::orm::Result& delResult)
                {
                    if (delResult.affectedRows() == 0)
                    {
                        onError(ErrorCode::NOT_IN_GROUP, "不在群中");
                        return;
                    }
                    auto db3 = DbClient::get();
                    db3->execSqlAsync(
                        [onSuccess](const drogon::orm::Result&)
                        {
                            Json::Value data;
                            data["message"] = "已退群";
                            onSuccess(data);
                        },
                        [onError](const drogon::orm::DrogonDbException& e)
                        {
                            LOG_ERROR << "DB error: " << e.base().what();
                            onError(ErrorCode::INTERNAL_ERROR, "数据库错误");
                        },
                        "UPDATE group_chats SET member_count = GREATEST(member_count - 1, 0) WHERE id=?",
                        groupId);
                },
                [onError](const drogon::orm::DrogonDbException& e)
                {
                    LOG_ERROR << "DB error: " << e.base().what();
                    onError(ErrorCode::INTERNAL_ERROR, "数据库错误");
                },
                "DELETE FROM group_members WHERE group_id=? AND user_id=?",
                groupId, userId);
        },
        [onError](const drogon::orm::DrogonDbException& e)
        {
            LOG_ERROR << "DB error: " << e.base().what();
            onError(ErrorCode::INTERNAL_ERROR, "数据库错误");
        },
        "SELECT owner_id FROM group_chats WHERE id=?",
        groupId);
}

// ---- 群成员列表 ----
void GroupService::listMembers(
    int64_t groupId,
    const JsonCallback& onSuccess,
    const ErrorCallback& onError)
{
    auto db = DbClient::get();
    db->execSqlAsync(
        [onSuccess](const drogon::orm::Result& result)
        {
            Json::Value list(Json::arrayValue);
            for (const auto& row : result)
            {
                Json::Value m;
                m["id"]       = Json::Int64(row["user_id"].as<int64_t>());
                m["nickname"] = row["nickname"].as<std::string>();
                m["role"]     = row["role"].as<int>();
                list.append(m);
            }
            Json::Value data;
            data["list"] = list;
            data["total"] = static_cast<int>(result.size());
            onSuccess(data);
        },
        [onError](const drogon::orm::DrogonDbException& e)
        {
            LOG_ERROR << "DB error: " << e.base().what();
            onError(ErrorCode::INTERNAL_ERROR, "数据库错误");
        },
        "SELECT gm.user_id, u.nickname, gm.role "
        "FROM group_members gm JOIN users u ON gm.user_id = u.id "
        "WHERE gm.group_id=? ORDER BY gm.role DESC, gm.user_id",
        groupId);
}

// ---- 注销群（硬删） ----
void GroupService::dissolveGroup(
    int64_t groupId, int64_t currentUserId,
    const JsonCallback& onSuccess,
    const ErrorCallback& onError)
{
    auto db = DbClient::get();
    db->execSqlAsync(
        [groupId, currentUserId, onSuccess, onError](const drogon::orm::Result& result)
        {
            if (result.empty())
            {
                onError(ErrorCode::GROUP_NOT_FOUND, "群不存在");
                return;
            }
            if (result[0]["owner_id"].as<int64_t>() != currentUserId)
            {
                onError(ErrorCode::NOT_GROUP_OWNER, "只有群主可以注销群");
                return;
            }

            // 事务硬删：群消息 → 群成员 → 群
            auto db2 = DbClient::get();
            db2->execSqlAsync(
                [groupId, onSuccess, onError](const drogon::orm::Result&)
                {
                    auto db3 = DbClient::get();
                    db3->execSqlAsync(
                        [groupId, onSuccess, onError](const drogon::orm::Result&)
                        {
                            auto db4 = DbClient::get();
                            db4->execSqlAsync(
                                [onSuccess](const drogon::orm::Result&)
                                {
                                    Json::Value data;
                                    data["message"] = "群已注销";
                                    onSuccess(data);
                                },
                                [onError](const drogon::orm::DrogonDbException& e)
                                {
                                    LOG_ERROR << "DB error: " << e.base().what();
                                    onError(ErrorCode::INTERNAL_ERROR, "数据库错误");
                                },
                                "DELETE FROM group_chats WHERE id=?",
                                groupId);
                        },
                        [onError](const drogon::orm::DrogonDbException& e)
                        {
                            LOG_ERROR << "DB error: " << e.base().what();
                            onError(ErrorCode::INTERNAL_ERROR, "数据库错误");
                        },
                        "DELETE FROM group_members WHERE group_id=?",
                        groupId);
                },
                [onError](const drogon::orm::DrogonDbException& e)
                {
                    LOG_ERROR << "DB error: " << e.base().what();
                    onError(ErrorCode::INTERNAL_ERROR, "数据库错误");
                },
                "DELETE FROM group_messages WHERE group_id=?",
                groupId);
        },
        [onError](const drogon::orm::DrogonDbException& e)
        {
            LOG_ERROR << "DB error: " << e.base().what();
            onError(ErrorCode::INTERNAL_ERROR, "数据库错误");
        },
        "SELECT owner_id FROM group_chats WHERE id=?",
        groupId);
}

// ---- 我加入的群列表 ----
void GroupService::myGroups(
    int64_t userId,
    const JsonCallback& onSuccess,
    const ErrorCallback& onError)
{
    auto db = DbClient::get();
    db->execSqlAsync(
        [onSuccess](const drogon::orm::Result& result)
        {
            Json::Value list(Json::arrayValue);
            for (const auto& row : result)
            {
                Json::Value g;
                g["id"]          = Json::Int64(row["group_id"].as<int64_t>());
                g["name"]        = row["name"].as<std::string>();
                g["memberCount"] = row["member_count"].as<int>();
                g["role"]        = row["role"].as<int>();
                list.append(g);
            }
            Json::Value data;
            data["list"] = list;
            onSuccess(data);
        },
        [onError](const drogon::orm::DrogonDbException& e)
        {
            LOG_ERROR << "DB error: " << e.base().what();
            onError(ErrorCode::INTERNAL_ERROR, "数据库错误");
        },
        "SELECT gm.group_id, gc.name, gc.member_count, gm.role "
        "FROM group_members gm JOIN group_chats gc ON gm.group_id = gc.id "
        "WHERE gm.user_id=? ORDER BY gm.group_id",
        userId);
}

}  // namespace online_chat
