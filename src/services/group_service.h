#pragma once

#include <drogon/drogon.h>
#include <functional>
#include <string>

// 群聊业务逻辑服务
//
// 职责：建群、搜索、加群申请/审批、退群、注销群、群成员列表、我的群列表
namespace online_chat {

class GroupService
{
public:
    using JsonCallback = std::function<void(const Json::Value& data)>;
    using ErrorCallback = std::function<void(ErrorCode code, const std::string& msg)>;

    // 创建群聊（创建者自动成为群主）
    static void createGroup(const std::string& name, int64_t ownerId,
                            const JsonCallback& onSuccess,
                            const ErrorCallback& onError);

    // 按群名模糊搜索（分页）
    static void searchByName(const std::string& keyword,
                             int page, int pageSize,
                             const JsonCallback& onSuccess,
                             const ErrorCallback& onError);

    // 发送加群申请
    static void sendRequest(int64_t groupId, int64_t fromUser,
                            const JsonCallback& onSuccess,
                            const ErrorCallback& onError);

    // 获取群的待处理加群申请（群主操作）
    static void listPendingRequests(int64_t groupId, int64_t currentUserId,
                                    const JsonCallback& onSuccess,
                                    const ErrorCallback& onError);

    // 审批加群申请（同意/拒绝，群主操作）
    static void handleRequest(int64_t requestId, int64_t currentUserId,
                              bool accept,
                              const JsonCallback& onSuccess,
                              const ErrorCallback& onError);

    // 退群
    static void leaveGroup(int64_t groupId, int64_t userId,
                           const JsonCallback& onSuccess,
                           const ErrorCallback& onError);

    // 群成员列表
    static void listMembers(int64_t groupId,
                            const JsonCallback& onSuccess,
                            const ErrorCallback& onError);

    // 注销群（群主权限，硬删群/成员/消息）
    static void dissolveGroup(int64_t groupId, int64_t currentUserId,
                              const JsonCallback& onSuccess,
                              const ErrorCallback& onError);

    // 我加入的群列表
    static void myGroups(int64_t userId,
                         const JsonCallback& onSuccess,
                         const ErrorCallback& onError);
};

}  // namespace online_chat
