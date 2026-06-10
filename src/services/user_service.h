#pragma once

#include <drogon/drogon.h>
#include <functional>
#include <string>

// 用户业务逻辑服务
//
// 职责：用户资料查询、按昵称模糊搜索
namespace online_chat {

class UserService
{
public:
    using JsonCallback = std::function<void(const Json::Value& data)>;
    using ErrorCallback = std::function<void(ErrorCode code, const std::string& msg)>;

    // 获取用户资料（不含敏感字段）
    static void getProfile(int64_t userId,
                           const JsonCallback& onSuccess,
                           const ErrorCallback& onError);

    // 按昵称模糊搜索用户（分页）
    static void searchByNickname(const std::string& keyword,
                                 int page, int pageSize,
                                 int64_t currentUserId,
                                 const JsonCallback& onSuccess,
                                 const ErrorCallback& onError);

    // 获取用户简要信息（供内部调用）
    struct UserBrief
    {
        int64_t     id = 0;
        std::string email;
        std::string nickname;
        int         role = 0;
    };
    static void getUserBrief(int64_t userId,
                             const std::function<void(const UserBrief&)>& onFound,
                             const ErrorCallback& onError);
};

}  // namespace online_chat
