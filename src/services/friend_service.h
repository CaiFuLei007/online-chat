#pragma once

#include "utils/errors.h"
#include <drogon/drogon.h>
#include <functional>
#include <string>

// 好友业务逻辑服务
//
// 职责：加好友申请、审批、好友列表、删除好友
namespace online_chat {

class FriendService
{
public:
    using JsonCallback = std::function<void(const Json::Value& data)>;
    using ErrorCallback = std::function<void(ErrorCode code, const std::string& msg)>;

    // 发送加好友申请
    static void sendRequest(int64_t fromUser, int64_t toUser,
                            const std::string& message,
                            const JsonCallback& onSuccess,
                            const ErrorCallback& onError);

    // 获取待处理的好友申请列表（我收到的）
    static void listPendingRequests(int64_t userId,
                                    const JsonCallback& onSuccess,
                                    const ErrorCallback& onError);

    // 审批好友申请（同意/拒绝）
    static void handleRequest(int64_t requestId, int64_t currentUserId,
                              bool accept,
                              const JsonCallback& onSuccess,
                              const ErrorCallback& onError);

    // 好友列表（含在线状态）
    static void listFriends(int64_t userId,
                            const JsonCallback& onSuccess,
                            const ErrorCallback& onError);

    // 删除好友（双向解除，保留消息）
    static void removeFriend(int64_t userId, int64_t friendId,
                             const JsonCallback& onSuccess,
                             const ErrorCallback& onError);

private:
    // 查找好友关系（返回 friendships 表 id，0 表示不存在）
    static void findFriendship(int64_t userA, int64_t userB,
                               const std::function<void(int64_t id, int status)>& onFound,
                               const std::function<void()>& onNotFound,
                               const ErrorCallback& onError);
};

}  // namespace online_chat
