#pragma once

#include <drogon/drogon.h>
#include <functional>
#include <string>

// 认证业务逻辑服务
//
// 职责：验证码生成/校验、注册、登录、JWT 签发、Redis session 管理
namespace online_chat {

class AuthService
{
public:
    // ---- 回调类型 ----
    using JsonCallback = std::function<void(const Json::Value& data)>;
    using ErrorCallback = std::function<void(ErrorCode code, const std::string& msg)>;

    // ---- 1. 发送验证码 ----
    // 检查限频 → 生成验证码 → 存 Redis → 异步发邮件
    static void sendVerifyCode(const std::string& email,
                               const JsonCallback& onSuccess,
                               const ErrorCallback& onError);

    // ---- 2. 注册 ----
    // 校验验证码 → 查重 → bcrypt 哈希 → INSERT → 删验证码
    static void registerUser(const std::string& email,
                             const std::string& code,
                             const std::string& password,
                             const std::string& nickname,
                             const JsonCallback& onSuccess,
                             const ErrorCallback& onError);

    // ---- 3. 登录 ----
    // 查用户 → bcrypt 校验 → 签发 JWT → 存 Redis session
    static void login(const std::string& email,
                      const std::string& password,
                      const JsonCallback& onSuccess,
                      const ErrorCallback& onError);

    // ---- 4. 将某用户踢下线（清除 Redis session） ----
    static void kickUser(int64_t userId,
                         const std::function<void()>& onDone);

private:
    // 生成 N 位随机数字验证码
    static std::string generateCode(int length);

    // 从 users 表查询用户（email）
    struct UserRow
    {
        int64_t     id = 0;
        std::string email;
        std::string passwordHash;
        std::string nickname;
        int         role = 0;
        int         status = 0;
    };
    static void findUserByEmail(const std::string& email,
                                const std::function<void(const UserRow&)>& onFound,
                                const std::function<void()>& onNotFound,
                                const ErrorCallback& onError);
};

}  // namespace online_chat
