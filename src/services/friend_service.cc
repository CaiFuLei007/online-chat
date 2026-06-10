#include "services/friend_service.h"
#include "dao/db_client.h"
#include "dao/redis_client.h"
#include "utils/errors.h"
#include "utils/conversation.h"

#include <drogon/drogon.h>

namespace online_chat {

// convKey 已提取到 utils/conversation.h，此处直接使用 online_chat::convKey

// ---- 查找好友关系 ----
void FriendService::findFriendship(
    int64_t userA, int64_t userB,
    const std::function<void(int64_t id, int status)>& onFound,
    const std::function<void()>& onNotFound,
    const ErrorCallback& onError)
{
    if (userA > userB) std::swap(userA, userB);
    auto db = DbClient::get();
    db->execSqlAsync(
        [onFound, onNotFound](const drogon::orm::Result& result)
        {
            if (result.empty())
            {
                onNotFound();
                return;
            }
            onFound(result[0]["id"].as<int64_t>(),
                    result[0]["status"].as<int>());
        },
        [onError](const drogon::orm::DrogonDbException& e)
        {
            LOG_ERROR << "DB error in findFriendship: " << e.base().what();
            onError(ErrorCode::INTERNAL_ERROR, "数据库错误");
        },
        "SELECT id, status FROM friendships WHERE user_a=? AND user_b=?",
        userA, userB);
}

// ---- 发送加好友申请 ----
void FriendService::sendRequest(
    int64_t fromUser, int64_t toUser,
    const std::string& message,
    const JsonCallback& onSuccess,
    const ErrorCallback& onError)
{
    if (fromUser == toUser)
    {
        onError(ErrorCode::CANNOT_ADD_SELF, "不能加自己为好友");
        return;
    }

    // 检查是否已是好友
    findFriendship(fromUser, toUser,
        [onError](int64_t /*id*/, int status)
        {
            if (status == 1)
                onError(ErrorCode::ALREADY_FRIENDS, "已是好友");
            else
                onError(ErrorCode::ALREADY_FRIENDS, "好友关系已存在（已解除）");
        },
        [fromUser, toUser, message, onSuccess, onError]()
        {
            // 不是好友，检查是否有待处理申请
            auto db = DbClient::get();
            db->execSqlAsync(
                [fromUser, toUser, message, onSuccess, onError]
                (const drogon::orm::Result& result)
                {
                    if (!result.empty())
                    {
                        onError(ErrorCode::FRIEND_REQUEST_SENT, "已发送过申请，请等待处理");
                        return;
                    }

                    // 插入申请
                    auto db2 = DbClient::get();
                    db2->execSqlAsync(
                        [onSuccess](const drogon::orm::Result& r)
                        {
                            Json::Value data;
                            data["requestId"] = Json::Int64(r.insertId());
                            data["message"] = "申请已发送";
                            onSuccess(data);
                        },
                        [onError](const drogon::orm::DrogonDbException& e)
                        {
                            LOG_ERROR << "DB INSERT friend_request error: " << e.base().what();
                            onError(ErrorCode::INTERNAL_ERROR, "数据库错误");
                        },
                        "INSERT INTO friend_requests (from_user, to_user, message) VALUES (?, ?, ?)",
                        fromUser, toUser, message);
                },
                [onError](const drogon::orm::DrogonDbException& e)
                {
                    LOG_ERROR << "DB error checking request: " << e.base().what();
                    onError(ErrorCode::INTERNAL_ERROR, "数据库错误");
                },
                "SELECT id FROM friend_requests WHERE from_user=? AND to_user=? AND status=0",
                fromUser, toUser);
        },
        onError);
}

// ---- 获取待处理申请列表 ----
void FriendService::listPendingRequests(
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
                Json::Value req;
                req["id"]        = Json::Int64(row["id"].as<int64_t>());
                req["fromUser"]  = Json::Int64(row["from_user"].as<int64_t>());
                req["message"]   = row["message"].as<std::string>();
                req["createdAt"] = row["created_at"].as<std::string>();
                // 附带申请人昵称
                if (row["nickname"].isNull())
                    req["nickname"] = "";
                else
                    req["nickname"] = row["nickname"].as<std::string>();
                list.append(req);
            }
            Json::Value data;
            data["list"] = list;
            data["total"] = static_cast<int>(result.size());
            onSuccess(data);
        },
        [onError](const drogon::orm::DrogonDbException& e)
        {
            LOG_ERROR << "DB error in listPendingRequests: " << e.base().what();
            onError(ErrorCode::INTERNAL_ERROR, "数据库错误");
        },
        "SELECT fr.id, fr.from_user, fr.message, fr.created_at, u.nickname "
        "FROM friend_requests fr LEFT JOIN users u ON fr.from_user = u.id "
        "WHERE fr.to_user=? AND fr.status=0 ORDER BY fr.created_at DESC",
        userId);
}

// ---- 审批好友申请 ----
void FriendService::handleRequest(
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
            const auto& row = result[0];
            int64_t fromUser = row["from_user"].as<int64_t>();
            int64_t toUser   = row["to_user"].as<int64_t>();
            int status       = row["status"].as<int>();

            if (toUser != currentUserId)
            {
                onError(ErrorCode::FORBIDDEN, "无权处理此申请");
                return;
            }
            if (status != 0)
            {
                onError(ErrorCode::INVALID_PARAMS, "该申请已处理");
                return;
            }

            if (!accept)
            {
                // 拒绝：更新状态
                auto db2 = DbClient::get();
                db2->execSqlAsync(
                    [onSuccess](const drogon::orm::Result&)
                    {
                        Json::Value data;
                        data["message"] = "已拒绝";
                        onSuccess(data);
                    },
                    [onError](const drogon::orm::DrogonDbException& e)
                    {
                        LOG_ERROR << "DB error rejecting request: " << e.base().what();
                        onError(ErrorCode::INTERNAL_ERROR, "数据库错误");
                    },
                    "UPDATE friend_requests SET status=2 WHERE id=?",
                    requestId);
                return;
            }

            // 同意：更新申请状态 + 创建好友关系
            int64_t userA = std::min(fromUser, toUser);
            int64_t userB = std::max(fromUser, toUser);

            auto db2 = DbClient::get();
            // 先检查是否已有 friendships 记录（可能以前删过好友）
            db2->execSqlAsync(
                [requestId, userA, userB, onSuccess, onError]
                (const drogon::orm::Result& existResult)
                {
                    auto db3 = DbClient::get();
                    if (existResult.empty())
                    {
                        // 新建好友关系
                        db3->execSqlAsync(
                            [requestId, onSuccess](const drogon::orm::Result&)
                            {
                                // 更新申请状态
                                auto db4 = DbClient::get();
                                db4->execSqlAsync(
                                    [](const drogon::orm::Result&) {},
                                    [](const drogon::orm::DrogonDbException&) {},
                                    "UPDATE friend_requests SET status=1 WHERE id=?",
                                    requestId);
                                Json::Value data;
                                data["message"] = "已同意";
                                onSuccess(data);
                            },
                            [onError](const drogon::orm::DrogonDbException& e)
                            {
                                LOG_ERROR << "DB INSERT friendship error: " << e.base().what();
                                onError(ErrorCode::INTERNAL_ERROR, "数据库错误");
                            },
                            "INSERT INTO friendships (user_a, user_b, status) VALUES (?, ?, 1)",
                            userA, userB);
                    }
                    else
                    {
                        // 已有记录（之前删过），恢复 status=1
                        int64_t fid = existResult[0]["id"].as<int64_t>();
                        db3->execSqlAsync(
                            [requestId, onSuccess](const drogon::orm::Result&)
                            {
                                auto db4 = DbClient::get();
                                db4->execSqlAsync(
                                    [](const drogon::orm::Result&) {},
                                    [](const drogon::orm::DrogonDbException&) {},
                                    "UPDATE friend_requests SET status=1 WHERE id=?",
                                    requestId);
                                Json::Value data;
                                data["message"] = "已同意";
                                onSuccess(data);
                            },
                            [onError](const drogon::orm::DrogonDbException& e)
                            {
                                LOG_ERROR << "DB UPDATE friendship error: " << e.base().what();
                                onError(ErrorCode::INTERNAL_ERROR, "数据库错误");
                            },
                            "UPDATE friendships SET status=1, updated_at=NOW() WHERE id=?",
                            fid);
                    }
                },
                [onError](const drogon::orm::DrogonDbException& e)
                {
                    LOG_ERROR << "DB error checking friendship: " << e.base().what();
                    onError(ErrorCode::INTERNAL_ERROR, "数据库错误");
                },
                "SELECT id FROM friendships WHERE user_a=? AND user_b=?",
                userA, userB);
        },
        [onError](const drogon::orm::DrogonDbException& e)
        {
            LOG_ERROR << "DB error in handleRequest: " << e.base().what();
            onError(ErrorCode::INTERNAL_ERROR, "数据库错误");
        },
        "SELECT from_user, to_user, status FROM friend_requests WHERE id=?",
        requestId);
}

// ---- 好友列表（含在线状态） ----
void FriendService::listFriends(
    int64_t userId,
    const JsonCallback& onSuccess,
    const ErrorCallback& onError)
{
    auto db = DbClient::get();
    db->execSqlAsync(
        [userId, onSuccess, onError](const drogon::orm::Result& result)
        {
            Json::Value list(Json::arrayValue);
            for (const auto& row : result)
            {
                int64_t friendId = row["friend_id"].as<int64_t>();
                Json::Value f;
                f["id"]       = Json::Int64(friendId);
                f["nickname"] = row["nickname"].as<std::string>();
                f["role"]     = row["role"].as<int>();
                // 在线状态先默认 false，后面批量查 Redis
                f["online"]   = false;
                list.append(f);
            }

            // 批量查在线状态（简单方案：逐个查 Redis）
            // 优化方案可用 Redis pipeline，这里先用简单方案
            auto redis = RedisClient::get();
            if (list.size() == 0)
            {
                Json::Value data;
                data["list"] = list;
                onSuccess(data);
                return;
            }

            // 用计数器等待所有 Redis 查询完成
            auto count = std::make_shared<int>(0);
            int total = static_cast<int>(list.size());
            auto listPtr = std::make_shared<Json::Value>(list);

            for (int i = 0; i < total; ++i)
            {
                int64_t fid = (*listPtr)[i]["id"].asInt64();
                std::string key = "online:" + std::to_string(fid);
                redis->execCommandAsync(
                    [listPtr, i, count, total, onSuccess]
                    (const drogon::nosql::RedisResult& r)
                    {
                        if (r.type() == drogon::nosql::RedisResultType::kRedisString)
                            (*listPtr)[i]["online"] = true;
                        (*count)++;
                        if (*count == total)
                        {
                            Json::Value data;
                            data["list"] = *listPtr;
                            onSuccess(data);
                        }
                    },
                    [listPtr, i, count, total, onSuccess]
                    (const drogon::nosql::RedisException&)
                    {
                        (*count)++;
                        if (*count == total)
                        {
                            Json::Value data;
                            data["list"] = *listPtr;
                            onSuccess(data);
                        }
                    },
                    "GET %s", key.c_str());
            }
        },
        [onError](const drogon::orm::DrogonDbException& e)
        {
            LOG_ERROR << "DB error in listFriends: " << e.base().what();
            onError(ErrorCode::INTERNAL_ERROR, "数据库错误");
        },
        "SELECT f.user_a, f.user_b, "
        "CASE WHEN f.user_a = ? THEN f.user_b ELSE f.user_a END AS friend_id, "
        "u.nickname, u.role "
        "FROM friendships f "
        "JOIN users u ON u.id = CASE WHEN f.user_a = ? THEN f.user_b ELSE f.user_a END "
        "WHERE (f.user_a = ? OR f.user_b = ?) AND f.status = 1",
        userId, userId, userId, userId);
}

// ---- 删除好友（双向解除，保留消息） ----
void FriendService::removeFriend(
    int64_t userId, int64_t friendId,
    const JsonCallback& onSuccess,
    const ErrorCallback& onError)
{
    if (userId == friendId)
    {
        onError(ErrorCode::INVALID_PARAMS, "不能删除自己");
        return;
    }

    findFriendship(userId, friendId,
        [userId, friendId, onSuccess, onError](int64_t id, int status)
        {
            if (status == 0)
            {
                onError(ErrorCode::FRIEND_NOT_FOUND, "好友关系已解除");
                return;
            }
            // 软删除：status 设为 0
            auto db = DbClient::get();
            db->execSqlAsync(
                [onSuccess](const drogon::orm::Result&)
                {
                    Json::Value data;
                    data["message"] = "已删除好友";
                    onSuccess(data);
                },
                [onError](const drogon::orm::DrogonDbException& e)
                {
                    LOG_ERROR << "DB error in removeFriend: " << e.base().what();
                    onError(ErrorCode::INTERNAL_ERROR, "数据库错误");
                },
                "UPDATE friendships SET status=0, updated_at=NOW() WHERE id=?",
                id);
        },
        [onError]()
        {
            onError(ErrorCode::FRIEND_NOT_FOUND, "不是好友关系");
        },
        onError);
}

}  // namespace online_chat
