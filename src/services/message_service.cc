#include "services/message_service.h"
#include "controllers/ws_gateway.h"
#include "utils/conversation.h"
#include "utils/ws_protocol.h"
#include "utils/errors.h"
#include "dao/db_client.h"
#include "dao/redis_client.h"

#include <drogon/drogon.h>
#include <json/json.h>

namespace online_chat {

// ---- 序号生成（Redis INCR） ----
void MessageService::nextSeq(
    const std::string& seqKey,
    const std::function<void(int64_t)>& onSeq,
    const ErrorCallback& onError)
{
    auto redis = RedisClient::get();
    redis->execCommandAsync(
        [onSeq](const drogon::nosql::RedisResult& r)
        {
            onSeq(r.asInteger());
        },
        [onError, seqKey](const drogon::nosql::RedisException& e)
        {
            LOG_ERROR << "Redis INCR " << seqKey << " error: " << e.what();
            onError(ErrorCode::INTERNAL_ERROR, "序号生成失败");
        },
        "INCR %s", seqKey.c_str());
}

// ---- 写离线索引 ----
void MessageService::saveOfflineIndex(int64_t userId, int msgType, int64_t msgId)
{
    auto db = DbClient::get();
    db->execSqlAsync(
        [](const drogon::orm::Result&) {},
        [userId](const drogon::orm::DrogonDbException& e)
        {
            LOG_ERROR << "DB error saving offline index for userId=" << userId
                      << ": " << e.base().what();
        },
        "INSERT INTO offline_messages (user_id, msg_type, msg_id) VALUES (?, ?, ?)",
        userId, msgType, msgId);
}

// ============================================================
// 单聊消息
// ============================================================
void MessageService::sendSingleMessage(
    int64_t fromUser, int64_t toUser,
    const std::string& content,
    const JsonCallback& onAck,
    const ErrorCallback& onError)
{
    if (content.empty())
    {
        onError(ErrorCode::INVALID_PARAMS, "消息内容不能为空");
        return;
    }
    if (fromUser == toUser)
    {
        onError(ErrorCode::INVALID_PARAMS, "不能给自己发消息");
        return;
    }

    std::string ck = convKey(fromUser, toUser);
    std::string seqKey = "msgseq:single:" + ck;

    // 1. 生成序号
    nextSeq(seqKey,
        [fromUser, toUser, content, ck, onAck, onError](int64_t seq)
        {
            // 2. 落库
            auto db = DbClient::get();
            db->execSqlAsync(
                [fromUser, toUser, content, seq, onAck, onError]
                (const drogon::orm::Result& result)
                {
                    int64_t msgId = result.insertId();

                    // 3. 构造消息 JSON
                    Json::Value msgData;
                    msgData["id"]      = Json::Int64(msgId);
                    msgData["from"]    = Json::Int64(fromUser);
                    msgData["to"]      = Json::Int64(toUser);
                    msgData["content"] = content;
                    msgData["seq"]     = Json::Int64(seq);

                    // 4. ack 给发送方
                    onAck(msgData);

                    // 5. 尝试在线投递
                    bool delivered = ConnectionManager::instance().sendTo(
                        toUser, ws::makeEnvelope(ws::CHAT_SINGLE, seq, msgData));

                    // 6. 离线暂存
                    if (!delivered)
                    {
                        saveOfflineIndex(toUser, 1, msgId);
                    }
                },
                [onError](const drogon::orm::DrogonDbException& e)
                {
                    LOG_ERROR << "DB INSERT single_message error: " << e.base().what();
                    onError(ErrorCode::INTERNAL_ERROR, "消息落库失败");
                },
                "INSERT INTO single_messages (conv_key, from_user, to_user, content, seq) "
                "VALUES (?, ?, ?, ?, ?)",
                ck, fromUser, toUser, content, seq);
        },
        onError);
}

// ============================================================
// 群聊消息
// ============================================================
void MessageService::sendGroupMessage(
    int64_t fromUser, int64_t groupId,
    const std::string& content,
    const JsonCallback& onAck,
    const ErrorCallback& onError)
{
    if (content.empty())
    {
        onError(ErrorCode::INVALID_PARAMS, "消息内容不能为空");
        return;
    }

    // 1. 验证发送者在群中 + 获取成员列表
    auto db = DbClient::get();
    db->execSqlAsync(
        [fromUser, groupId, content, onAck, onError]
        (const drogon::orm::Result& memberResult)
        {
            // 检查发送者是否在群中
            bool inGroup = false;
            for (const auto& row : memberResult)
            {
                if (row["user_id"].as<int64_t>() == fromUser)
                {
                    inGroup = true;
                    break;
                }
            }
            if (!inGroup)
            {
                onError(ErrorCode::NOT_IN_GROUP, "不在群中，无法发消息");
                return;
            }

            // 2. 生成序号
            std::string seqKey = "msgseq:group:" + std::to_string(groupId);
            nextSeq(seqKey,
                [fromUser, groupId, content, memberResult, onAck, onError]
                (int64_t seq)
                {
                    // 3. 落库
                    auto db2 = DbClient::get();
                    db2->execSqlAsync(
                        [fromUser, groupId, content, seq, memberResult, onAck, onError]
                        (const drogon::orm::Result& result)
                        {
                            int64_t msgId = result.insertId();

                            // 4. 构造消息 JSON
                            Json::Value msgData;
                            msgData["id"]       = Json::Int64(msgId);
                            msgData["groupId"]  = Json::Int64(groupId);
                            msgData["from"]     = Json::Int64(fromUser);
                            msgData["content"]  = content;
                            msgData["seq"]      = Json::Int64(seq);

                            // 5. ack 给发送方
                            onAck(msgData);

                            // 6. 广播给群内在线成员（排除发送者）
                            std::string envelope = ws::makeEnvelope(ws::CHAT_GROUP, seq, msgData);
                            for (const auto& row : memberResult)
                            {
                                int64_t memberId = row["user_id"].as<int64_t>();
                                if (memberId == fromUser) continue;

                                bool delivered = ConnectionManager::instance().sendTo(
                                    memberId, envelope);
                                if (!delivered)
                                {
                                    saveOfflineIndex(memberId, 2, msgId);
                                }
                            }
                        },
                        [onError](const drogon::orm::DrogonDbException& e)
                        {
                            LOG_ERROR << "DB INSERT group_message error: " << e.base().what();
                            onError(ErrorCode::INTERNAL_ERROR, "消息落库失败");
                        },
                        "INSERT INTO group_messages (group_id, from_user, content, seq) "
                        "VALUES (?, ?, ?, ?)",
                        groupId, fromUser, content, seq);
                },
                onError);
        },
        [onError](const drogon::orm::DrogonDbException& e)
        {
            LOG_ERROR << "DB error checking group membership: " << e.base().what();
            onError(ErrorCode::INTERNAL_ERROR, "数据库错误");
        },
        "SELECT user_id FROM group_members WHERE group_id=?",
        groupId);
}

// ============================================================
// 拉取离线消息
// ============================================================
void MessageService::pullOfflineMessages(
    int64_t userId,
    const JsonCallback& onSuccess,
    const ErrorCallback& onError)
{
    auto db = DbClient::get();
    db->execSqlAsync(
        [userId, onSuccess, onError](const drogon::orm::Result& result)
        {
            if (result.empty())
            {
                Json::Value data;
                data["messages"] = Json::Value(Json::arrayValue);
                onSuccess(data);
                return;
            }

            // 收集所有 msg_id 和类型
            struct OfflineRef { int64_t id; int64_t msgId; int msgType; };
            std::vector<OfflineRef> refs;
            for (const auto& row : result)
            {
                refs.push_back({
                    row["id"].as<int64_t>(),
                    row["msg_id"].as<int64_t>(),
                    row["msg_type"].as<int>()
                });
            }

            auto messages = std::make_shared<Json::Value>(Json::arrayValue);
            auto count = std::make_shared<int>(0);
            int total = static_cast<int>(refs.size());
            auto refPtr = std::make_shared<std::vector<OfflineRef>>(refs);

            for (int i = 0; i < total; ++i)
            {
                const auto& ref = (*refPtr)[i];
                if (ref.msgType == 1)
                {
                    // 单聊消息
                    auto db2 = DbClient::get();
                    db2->execSqlAsync(
                        [messages, count, total, userId, ref, onSuccess]
                        (const drogon::orm::Result& msgResult)
                        {
                            if (!msgResult.empty())
                            {
                                const auto& r = msgResult[0];
                                Json::Value m;
                                m["type"]    = "chat_single";
                                m["id"]      = Json::Int64(r["id"].as<int64_t>());
                                m["from"]    = Json::Int64(r["from_user"].as<int64_t>());
                                m["content"] = r["content"].as<std::string>();
                                m["seq"]     = Json::Int64(r["seq"].as<int64_t>());
                                messages->append(m);
                            }
                            (*count)++;
                            // 删除已投递的离线索引
                            auto db3 = DbClient::get();
                            db3->execSqlAsync(
                                [](const drogon::orm::Result&) {},
                                [](const drogon::orm::DrogonDbException&) {},
                                "DELETE FROM offline_messages WHERE id=?",
                                ref.id);

                            if (*count == total)
                            {
                                Json::Value data;
                                data["messages"] = *messages;
                                onSuccess(data);
                            }
                        },
                        [count, total, ref, onSuccess, messages]
                        (const drogon::orm::DrogonDbException&)
                        {
                            (*count)++;
                            if (*count == total)
                            {
                                Json::Value data;
                                data["messages"] = *messages;
                                onSuccess(data);
                            }
                        },
                        "SELECT id, from_user, content, seq FROM single_messages WHERE id=?",
                        ref.msgId);
                }
                else
                {
                    // 群聊消息
                    auto db2 = DbClient::get();
                    db2->execSqlAsync(
                        [messages, count, total, userId, ref, onSuccess]
                        (const drogon::orm::Result& msgResult)
                        {
                            if (!msgResult.empty())
                            {
                                const auto& r = msgResult[0];
                                Json::Value m;
                                m["type"]    = "chat_group";
                                m["id"]      = Json::Int64(r["id"].as<int64_t>());
                                m["groupId"] = Json::Int64(r["group_id"].as<int64_t>());
                                m["from"]    = Json::Int64(r["from_user"].as<int64_t>());
                                m["content"] = r["content"].as<std::string>();
                                m["seq"]     = Json::Int64(r["seq"].as<int64_t>());
                                messages->append(m);
                            }
                            (*count)++;
                            auto db3 = DbClient::get();
                            db3->execSqlAsync(
                                [](const drogon::orm::Result&) {},
                                [](const drogon::orm::DrogonDbException&) {},
                                "DELETE FROM offline_messages WHERE id=?",
                                ref.id);

                            if (*count == total)
                            {
                                Json::Value data;
                                data["messages"] = *messages;
                                onSuccess(data);
                            }
                        },
                        [count, total, ref, onSuccess, messages]
                        (const drogon::orm::DrogonDbException&)
                        {
                            (*count)++;
                            if (*count == total)
                            {
                                Json::Value data;
                                data["messages"] = *messages;
                                onSuccess(data);
                            }
                        },
                        "SELECT id, group_id, from_user, content, seq FROM group_messages WHERE id=?",
                        ref.msgId);
                }
            }
        },
        [onError](const drogon::orm::DrogonDbException& e)
        {
            LOG_ERROR << "DB error pulling offline messages: " << e.base().what();
            onError(ErrorCode::INTERNAL_ERROR, "数据库错误");
        },
        "SELECT id, msg_type, msg_id FROM offline_messages WHERE user_id=? ORDER BY id",
        userId);
}

// ============================================================
// 单聊历史分页
// ============================================================
void MessageService::getSingleHistory(
    int64_t userId, int64_t peerId,
    int64_t beforeSeq, int limit,
    const JsonCallback& onSuccess,
    const ErrorCallback& onError)
{
    if (limit <= 0 || limit > 100) limit = 30;
    std::string ck = convKey(userId, peerId);

    auto db = DbClient::get();
    std::string sql;
    if (beforeSeq > 0)
    {
        sql = "SELECT id, from_user, to_user, content, seq, created_at "
              "FROM single_messages WHERE conv_key=? AND seq < ? "
              "ORDER BY seq DESC LIMIT ?";
        db->execSqlAsync(
            [onSuccess](const drogon::orm::Result& result)
            {
                Json::Value list(Json::arrayValue);
                for (const auto& row : result)
                {
                    Json::Value m;
                    m["id"]        = Json::Int64(row["id"].as<int64_t>());
                    m["from"]      = Json::Int64(row["from_user"].as<int64_t>());
                    m["to"]        = Json::Int64(row["to_user"].as<int64_t>());
                    m["content"]   = row["content"].as<std::string>();
                    m["seq"]       = Json::Int64(row["seq"].as<int64_t>());
                    m["createdAt"] = row["created_at"].as<std::string>();
                    list.append(m);
                }
                Json::Value data;
                data["list"] = list;
                data["hasMore"] = (result.size() == static_cast<size_t>(30));
                onSuccess(data);
            },
            [onError](const drogon::orm::DrogonDbException& e)
            {
                LOG_ERROR << "DB error: " << e.base().what();
                onError(ErrorCode::INTERNAL_ERROR, "数据库错误");
            },
            sql, ck, beforeSeq, limit);
    }
    else
    {
        sql = "SELECT id, from_user, to_user, content, seq, created_at "
              "FROM single_messages WHERE conv_key=? "
              "ORDER BY seq DESC LIMIT ?";
        db->execSqlAsync(
            [onSuccess, limit](const drogon::orm::Result& result)
            {
                Json::Value list(Json::arrayValue);
                for (const auto& row : result)
                {
                    Json::Value m;
                    m["id"]        = Json::Int64(row["id"].as<int64_t>());
                    m["from"]      = Json::Int64(row["from_user"].as<int64_t>());
                    m["to"]        = Json::Int64(row["to_user"].as<int64_t>());
                    m["content"]   = row["content"].as<std::string>();
                    m["seq"]       = Json::Int64(row["seq"].as<int64_t>());
                    m["createdAt"] = row["created_at"].as<std::string>();
                    list.append(m);
                }
                Json::Value data;
                data["list"] = list;
                data["hasMore"] = (result.size() == static_cast<size_t>(limit));
                onSuccess(data);
            },
            [onError](const drogon::orm::DrogonDbException& e)
            {
                LOG_ERROR << "DB error: " << e.base().what();
                onError(ErrorCode::INTERNAL_ERROR, "数据库错误");
            },
            sql, ck, limit);
    }
}

// ============================================================
// 群聊历史分页
// ============================================================
void MessageService::getGroupHistory(
    int64_t userId, int64_t groupId,
    int64_t beforeSeq, int limit,
    const JsonCallback& onSuccess,
    const ErrorCallback& onError)
{
    if (limit <= 0 || limit > 100) limit = 30;

    auto db = DbClient::get();
    std::string sql;
    if (beforeSeq > 0)
    {
        sql = "SELECT id, group_id, from_user, content, seq, created_at "
              "FROM group_messages WHERE group_id=? AND seq < ? "
              "ORDER BY seq DESC LIMIT ?";
        db->execSqlAsync(
            [onSuccess](const drogon::orm::Result& result)
            {
                Json::Value list(Json::arrayValue);
                for (const auto& row : result)
                {
                    Json::Value m;
                    m["id"]        = Json::Int64(row["id"].as<int64_t>());
                    m["groupId"]   = Json::Int64(row["group_id"].as<int64_t>());
                    m["from"]      = Json::Int64(row["from_user"].as<int64_t>());
                    m["content"]   = row["content"].as<std::string>();
                    m["seq"]       = Json::Int64(row["seq"].as<int64_t>());
                    m["createdAt"] = row["created_at"].as<std::string>();
                    list.append(m);
                }
                Json::Value data;
                data["list"] = list;
                data["hasMore"] = (result.size() == static_cast<size_t>(30));
                onSuccess(data);
            },
            [onError](const drogon::orm::DrogonDbException& e)
            {
                LOG_ERROR << "DB error: " << e.base().what();
                onError(ErrorCode::INTERNAL_ERROR, "数据库错误");
            },
            sql, groupId, beforeSeq, limit);
    }
    else
    {
        sql = "SELECT id, group_id, from_user, content, seq, created_at "
              "FROM group_messages WHERE group_id=? "
              "ORDER BY seq DESC LIMIT ?";
        db->execSqlAsync(
            [onSuccess, limit](const drogon::orm::Result& result)
            {
                Json::Value list(Json::arrayValue);
                for (const auto& row : result)
                {
                    Json::Value m;
                    m["id"]        = Json::Int64(row["id"].as<int64_t>());
                    m["groupId"]   = Json::Int64(row["group_id"].as<int64_t>());
                    m["from"]      = Json::Int64(row["from_user"].as<int64_t>());
                    m["content"]   = row["content"].as<std::string>();
                    m["seq"]       = Json::Int64(row["seq"].as<int64_t>());
                    m["createdAt"] = row["created_at"].as<std::string>();
                    list.append(m);
                }
                Json::Value data;
                data["list"] = list;
                data["hasMore"] = (result.size() == static_cast<size_t>(limit));
                onSuccess(data);
            },
            [onError](const drogon::orm::DrogonDbException& e)
            {
                LOG_ERROR << "DB error: " << e.base().what();
                onError(ErrorCode::INTERNAL_ERROR, "数据库错误");
            },
            sql, groupId, limit);
    }
}

}  // namespace online_chat
