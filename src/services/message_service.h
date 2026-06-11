#pragma once

#include "utils/errors.h"
#include <drogon/drogon.h>
#include <functional>
#include <string>
#include <json/json.h>

// 消息业务逻辑服务
//
// 职责：单聊/群聊消息落库、序号生成、在线投递、离线暂存、历史分页
namespace online_chat {

class MessageService
{
public:
    using JsonCallback = std::function<void(const Json::Value& data)>;
    using ErrorCallback = std::function<void(ErrorCode code, const std::string& msg)>;

    // ---- 单聊消息 ----
    // 落库 → 生成序号 → 在线投递/离线暂存 → ack
    static void sendSingleMessage(int64_t fromUser, int64_t toUser,
                                  const std::string& content,
                                  const JsonCallback& onAck,
                                  const ErrorCallback& onError);

    // ---- 群聊消息 ----
    // 落库 → 生成序号 → 广播在群在线成员 → 离线暂存 → ack
    static void sendGroupMessage(int64_t fromUser, int64_t groupId,
                                 const std::string& content,
                                 const JsonCallback& onAck,
                                 const ErrorCallback& onError);

    // ---- 拉取离线消息 ----
    // 用户上线后调用：查 offline_messages → 投递 → 删除已投递索引
    static void pullOfflineMessages(int64_t userId,
                                    const JsonCallback& onSuccess,
                                    const ErrorCallback& onError);

    // ---- 历史消息分页查询 ----
    // 单聊历史：按 conv_key + seq 游标翻页
    static void getSingleHistory(int64_t userId, int64_t peerId,
                                 int64_t beforeSeq, int limit,
                                 const JsonCallback& onSuccess,
                                 const ErrorCallback& onError);

    // 群聊历史：按 group_id + seq 游标翻页
    static void getGroupHistory(int64_t userId, int64_t groupId,
                                int64_t beforeSeq, int limit,
                                const JsonCallback& onSuccess,
                                const ErrorCallback& onError);

private:
    // 生成会话内单调序号（Redis INCR）
    static void nextSeq(const std::string& seqKey,
                        const std::function<void(int64_t)>& onSeq,
                        const ErrorCallback& onError);

    // 写离线索引
    static void saveOfflineIndex(int64_t userId, int msgType, int64_t msgId);
};

}  // namespace online_chat
