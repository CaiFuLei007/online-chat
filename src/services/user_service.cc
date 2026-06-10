#include "services/user_service.h"
#include "dao/db_client.h"
#include "utils/errors.h"

#include <drogon/drogon.h>

namespace online_chat {

// 获取用户资料
void UserService::getProfile(
    int64_t userId,
    const JsonCallback& onSuccess,
    const ErrorCallback& onError)
{
    auto db = DbClient::get();
    db->execSqlAsync(
        [onSuccess, onError](const drogon::orm::Result& result)
        {
            if (result.empty())
            {
                onError(ErrorCode::NOT_FOUND, "用户不存在");
                return;
            }
            const auto& row = result[0];
            Json::Value user;
            user["id"]       = Json::Int64(row["id"].as<int64_t>());
            user["email"]    = row["email"].as<std::string>();
            user["nickname"] = row["nickname"].as<std::string>();
            user["role"]     = row["role"].as<int>();
            user["createdAt"] = row["created_at"].as<std::string>();

            Json::Value data;
            data["user"] = user;
            onSuccess(data);
        },
        [onError](const drogon::orm::DrogonDbException& e)
        {
            LOG_ERROR << "DB error in getProfile: " << e.base().what();
            onError(ErrorCode::INTERNAL_ERROR, "数据库错误");
        },
        "SELECT id, email, nickname, role, created_at FROM users WHERE id=?",
        userId);
}

// 按昵称模糊搜索（分页）
void UserService::searchByNickname(
    const std::string& keyword,
    int page, int pageSize,
    int64_t currentUserId,
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

    // 先查总数
    db->execSqlAsync(
        [db, keyword, offset, pageSize, currentUserId, onSuccess, onError]
        (const drogon::orm::Result& countResult)
        {
            int total = countResult[0][0].as<int>();

            // 再查列表（排除自己）
            db->execSqlAsync(
                [total, page, pageSize, onSuccess]
                (const drogon::orm::Result& result)
                {
                    Json::Value list(Json::arrayValue);
                    for (const auto& row : result)
                    {
                        Json::Value user;
                        user["id"]       = Json::Int64(row["id"].as<int64_t>());
                        user["nickname"] = row["nickname"].as<std::string>();
                        user["role"]     = row["role"].as<int>();
                        list.append(user);
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
                    LOG_ERROR << "DB error in search: " << e.base().what();
                    onError(ErrorCode::INTERNAL_ERROR, "数据库错误");
                },
                "SELECT id, nickname, role FROM users "
                "WHERE nickname LIKE ? AND id != ? "
                "ORDER BY id LIMIT ? OFFSET ?",
                "%" + keyword + "%", currentUserId, pageSize, offset);
        },
        [onError](const drogon::orm::DrogonDbException& e)
        {
            LOG_ERROR << "DB error in search count: " << e.base().what();
            onError(ErrorCode::INTERNAL_ERROR, "数据库错误");
        },
        "SELECT COUNT(*) FROM users WHERE nickname LIKE ? AND id != ?",
        "%" + keyword + "%", currentUserId);
}

// 获取用户简要信息
void UserService::getUserBrief(
    int64_t userId,
    const std::function<void(const UserBrief&)>& onFound,
    const ErrorCallback& onError)
{
    auto db = DbClient::get();
    db->execSqlAsync(
        [onFound, onError, userId](const drogon::orm::Result& result)
        {
            if (result.empty())
            {
                onError(ErrorCode::NOT_FOUND, "用户不存在: " + std::to_string(userId));
                return;
            }
            const auto& row = result[0];
            UserBrief u;
            u.id       = row["id"].as<int64_t>();
            u.email    = row["email"].as<std::string>();
            u.nickname = row["nickname"].as<std::string>();
            u.role     = row["role"].as<int>();
            onFound(u);
        },
        [onError](const drogon::orm::DrogonDbException& e)
        {
            LOG_ERROR << "DB error in getUserBrief: " << e.base().what();
            onError(ErrorCode::INTERNAL_ERROR, "数据库错误");
        },
        "SELECT id, email, nickname, role FROM users WHERE id=?",
        userId);
}

}  // namespace online_chat
