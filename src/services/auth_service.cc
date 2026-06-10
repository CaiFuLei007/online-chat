#include "services/auth_service.h"
#include "utils/config_drogon.h"
#include "utils/errors.h"
#include "utils/jwt_util.h"
#include "utils/password_util.h"
#include "utils/smtp_client.h"
#include "dao/db_client.h"
#include "dao/redis_client.h"

#include <drogon/drogon.h>
#include <random>
#include <sstream>

namespace online_chat {

// ---- 验证码生成 ----
std::string AuthService::generateCode(int length)
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 9);
    std::string code;
    code.reserve(length);
    for (int i = 0; i < length; ++i)
        code += static_cast<char>('0' + dis(gen));
    return code;
}

// ---- 查找用户 ----
void AuthService::findUserByEmail(
    const std::string& email,
    const std::function<void(const UserRow&)>& onFound,
    const std::function<void()>& onNotFound,
    const ErrorCallback& onError)
{
    auto db = DbClient::get();
    db->execSqlAsync(
        "SELECT id, email, password_hash, nickname, role, status "
        "FROM users WHERE email=?",
        [onFound, onNotFound](const drogon::orm::Result& result)
        {
            if (result.empty())
            {
                onNotFound();
                return;
            }
            const auto& row = result[0];
            UserRow u;
            u.id           = row["id"].as<int64_t>();
            u.email        = row["email"].as<std::string>();
            u.passwordHash = row["password_hash"].as<std::string>();
            u.nickname     = row["nickname"].as<std::string>();
            u.role         = row["role"].as<int>();
            u.status       = row["status"].as<int>();
            onFound(u);
        },
        [onError](const drogon::orm::DrogonDbException& e)
        {
            LOG_ERROR << "DB error in findUserByEmail: " << e.base().what();
            onError(ErrorCode::INTERNAL_ERROR, "database error");
        },
        email);
}

// ---- 1. 发送验证码 ----
void AuthService::sendVerifyCode(
    const std::string& email,
    const JsonCallback& onSuccess,
    const ErrorCallback& onError)
{
    const int codeLen = ConfigDrogon::getInt("JWT_SECRET", "verify_code.length", 6);
    const int ttl     = ConfigDrogon::getInt("JWT_SECRET", "verify_code.ttl_seconds", 300);
    const int limit   = ConfigDrogon::getInt("JWT_SECRET", "verify_code.resend_interval_seconds", 60);

    auto redis = RedisClient::get();

    // 1. 检查限频
    std::string limitKey = "verifycode_limit:" + email;
    redis->execCommandAsync(
        [email, codeLen, ttl, redis, onSuccess, onError, limitKey]
        (const drogon::nosql::RedisResult& result)
        {
            // 限频键存在 → 拒绝
            if (result.type() == drogon::nosql::RedisResultType::kRedisString)
            {
                onError(ErrorCode::RESEND_TOO_FAST, "请60秒后再试");
                return;
            }

            // 2. 生成验证码
            std::string code = generateCode(codeLen);
            std::string codeKey = "verifycode:" + email;

            // 3. 存验证码 + 设置限频（两条命令）
            redis->execCommandAsync(
                [email, code, onSuccess, onError, limitKey, limit]
                (const drogon::nosql::RedisResult&)
                {
                    // 验证码已存，再设限频
                    auto redis2 = RedisClient::get();
                    redis2->execCommandAsync(
                        [email, code, onSuccess]
                        (const drogon::nosql::RedisResult&)
                        {
                            // 4. 异步发邮件（丢到线程池）
                            drogon::app().getLoop()->queueInLoop(
                                [email, code]()
                                {
                                    std::string subject = "【online-chat】验证码";
                                    std::string body = "您的验证码是：" + code + "，5分钟内有效。";
                                    SmtpClient::send(email, subject, body);
                                });

                            Json::Value data;
                            data["message"] = "验证码已发送";
                            onSuccess(data);
                        },
                        [onError](const drogon::nosql::RedisException& e)
                        {
                            LOG_ERROR << "Redis SET limit error: " << e.what();
                            onError(ErrorCode::INTERNAL_ERROR, "redis error");
                        },
                        "SET %s 1 EX %d", limitKey.c_str(), limit);
                },
                [onError](const drogon::nosql::RedisException& e)
                {
                    LOG_ERROR << "Redis SET code error: " << e.what();
                    onError(ErrorCode::INTERNAL_ERROR, "redis error");
                },
                "SET %s %s EX %d", codeKey.c_str(), code.c_str(), ttl);
        },
        [onError](const drogon::nosql::RedisException& e)
        {
            LOG_ERROR << "Redis GET limit error: " << e.what();
            onError(ErrorCode::INTERNAL_ERROR, "redis error");
        },
        "GET %s", limitKey.c_str());
}

// ---- 2. 注册 ----
void AuthService::registerUser(
    const std::string& email,
    const std::string& code,
    const std::string& password,
    const std::string& nickname,
    const JsonCallback& onSuccess,
    const ErrorCallback& onError)
{
    // 参数校验
    if (email.empty() || code.empty() || password.empty())
    {
        onError(ErrorCode::INVALID_PARAMS, "邮箱、验证码、密码不能为空");
        return;
    }
    if (password.size() < 6)
    {
        onError(ErrorCode::INVALID_PARAMS, "密码长度不能少于6位");
        return;
    }

    auto redis = RedisClient::get();
    std::string codeKey = "verifycode:" + email;

    // 1. 从 Redis 取验证码
    redis->execCommandAsync(
        [email, code, password, nickname, onSuccess, onError, codeKey]
        (const drogon::nosql::RedisResult& result)
        {
            if (result.type() != drogon::nosql::RedisResultType::kRedisString)
            {
                onError(ErrorCode::VERIFY_CODE_EXPIRED, "验证码已过期或未发送");
                return;
            }

            std::string storedCode = result.getString();
            if (storedCode != code)
            {
                onError(ErrorCode::VERIFY_CODE_WRONG, "验证码错误");
                return;
            }

            // 2. 验证码正确，查重
            findUserByEmail(email,
                [onError](const UserRow&)
                {
                    onError(ErrorCode::EMAIL_REGISTERED, "该邮箱已注册");
                },
                [email, code, password, nickname, onSuccess, onError, codeKey]()
                {
                    // 3. 邮箱未注册，bcrypt 哈希密码
                    std::string hash = PasswordUtil::hash(password);
                    if (hash.empty())
                    {
                        onError(ErrorCode::INTERNAL_ERROR, "密码哈希失败");
                        return;
                    }

                    // 4. INSERT 用户
                    auto db = DbClient::get();
                    db->execSqlAsync(
                        [email, onSuccess, onError, codeKey]
                        (const drogon::orm::Result& result)
                        {
                            int64_t userId = result.insertId();

                            // 5. 删除已使用的验证码
                            auto redis = RedisClient::get();
                            redis->execCommandAsync(
                                [](const drogon::nosql::RedisResult&) {},
                                [](const drogon::nosql::RedisException&) {},
                                "DEL %s", codeKey.c_str());

                            Json::Value data;
                            data["userId"] = Json::Int64(userId);
                            data["message"] = "注册成功";
                            onSuccess(data);
                        },
                        [onError](const drogon::orm::DrogonDbException& e)
                        {
                            LOG_ERROR << "DB INSERT user error: " << e.base().what();
                            onError(ErrorCode::INTERNAL_ERROR, "数据库错误");
                        },
                        "INSERT INTO users (email, password_hash, nickname, role, status) "
                        "VALUES (?, ?, ?, 0, 0)",
                        email, hash, nickname);
                },
                onError);
        },
        [onError](const drogon::nosql::RedisException& e)
        {
            LOG_ERROR << "Redis GET code error: " << e.what();
            onError(ErrorCode::INTERNAL_ERROR, "redis error");
        },
        "GET %s", codeKey.c_str());
}

// ---- 3. 登录 ----
void AuthService::login(
    const std::string& email,
    const std::string& password,
    const JsonCallback& onSuccess,
    const ErrorCallback& onError)
{
    if (email.empty() || password.empty())
    {
        onError(ErrorCode::INVALID_PARAMS, "邮箱和密码不能为空");
        return;
    }

    findUserByEmail(email,
        [password, onSuccess, onError](const UserRow& user)
        {
            // 检查账号状态
            if (user.status == 1)
            {
                onError(ErrorCode::ACCOUNT_DISABLED, "账号已被禁用");
                return;
            }

            // bcrypt 校验
            if (!PasswordUtil::verify(password, user.passwordHash))
            {
                onError(ErrorCode::PASSWORD_WRONG, "邮箱或密码错误");
                return;
            }

            // 签发 JWT
            JwtPayload payload;
            payload.userId   = user.id;
            payload.email    = user.email;
            payload.role     = user.role;
            payload.nickname = user.nickname;
            std::string token = JwtUtil::sign(payload);

            // 存 Redis session（覆盖旧的 → 单点登录挤下线）
            auto redis = RedisClient::get();
            std::string sessionKey = "session:" + std::to_string(user.id);
            redis->execCommandAsync(
                [token, user, onSuccess]
                (const drogon::nosql::RedisResult&)
                {
                    Json::Value data;
                    data["token"]    = token;
                    data["userId"]   = Json::Int64(user.id);
                    data["nickname"] = user.nickname;
                    data["role"]     = user.role;
                    onSuccess(data);
                },
                [onError](const drogon::nosql::RedisException& e)
                {
                    LOG_ERROR << "Redis SET session error: " << e.what();
                    onError(ErrorCode::INTERNAL_ERROR, "redis error");
                },
                "SET %s %s EX %d", sessionKey.c_str(), token.c_str(), 604800);
        },
        [onError]()
        {
            onError(ErrorCode::PASSWORD_WRONG, "邮箱或密码错误");
        },
        onError);
}

// ---- 4. 踢下线 ----
void AuthService::kickUser(
    int64_t userId,
    const std::function<void()>& onDone)
{
    auto redis = RedisClient::get();
    std::string sessionKey = "session:" + std::to_string(userId);
    redis->execCommandAsync(
        [onDone](const drogon::nosql::RedisResult&) { onDone(); },
        [onDone](const drogon::nosql::RedisException&) { onDone(); },
        "DEL %s", sessionKey.c_str());
}

}  // namespace online_chat
