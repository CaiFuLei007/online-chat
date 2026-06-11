#include "services/auth_service.h"
#include "controllers/ws_gateway.h"
#include "utils/config_drogon.h"
#include "utils/jwt_util.h"
#include "utils/password_util.h"
#include "utils/smtp_client.h"
#include "dao/db_client.h"
#include "dao/redis_client.h"

#include <drogon/drogon.h>
#include <random>
#include <sstream>

namespace online_chat {

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

void AuthService::findUserByEmail(
    const std::string& email,
    const std::function<void(const UserRow&)>& onFound,
    const std::function<void()>& onNotFound,
    const ErrorCallback& onError)
{
    auto db = DbClient::get();
    db->execSqlAsync(
        std::string("SELECT id, email, password_hash, nickname, role, status "
        "FROM users WHERE email=?"),
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

void AuthService::sendVerifyCode(
    const std::string& email,
    const JsonCallback& onSuccess,
    const ErrorCallback& onError)
{
    const int codeLen = ConfigDrogon::getInt("VERIFY_CODE_LENGTH", "verify_code.length", 6);
    const int ttl     = ConfigDrogon::getInt("VERIFY_CODE_TTL_SECONDS", "verify_code.ttl_seconds", 300);
    const int limitSec = ConfigDrogon::getInt("VERIFY_CODE_RESEND_INTERVAL", "verify_code.resend_interval_seconds", 60);

    auto redis = RedisClient::get();
    std::string limitKey = "verifycode_limit:" + email;

    redis->execCommandAsync(
        [email, codeLen, ttl, redis, onSuccess, onError, limitKey, limitSec]
        (const drogon::nosql::RedisResult& result)
        {
            if (result.type() == drogon::nosql::RedisResultType::kString)
            {
                onError(ErrorCode::RESEND_TOO_FAST, "请60秒后再试");
                return;
            }

            std::string code = generateCode(codeLen);
            std::string codeKey = "verifycode:" + email;

            redis->execCommandAsync(
                [email, code, onSuccess, onError, limitKey, limitSec]
                (const drogon::nosql::RedisResult&)
                {
                    auto redis2 = RedisClient::get();
                    redis2->execCommandAsync(
                        [email, code, onSuccess, onError]
                        (const drogon::nosql::RedisResult&)
                        {
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
                        "SET %s 1 EX %d", limitKey.c_str(), limitSec);
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

void AuthService::registerUser(
    const std::string& email,
    const std::string& code,
    const std::string& password,
    const std::string& nickname,
    const JsonCallback& onSuccess,
    const ErrorCallback& onError)
{
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

    redis->execCommandAsync(
        [email, code, password, nickname, onSuccess, onError, codeKey]
        (const drogon::nosql::RedisResult& result)
        {
            if (result.type() != drogon::nosql::RedisResultType::kString)
            {
                onError(ErrorCode::VERIFY_CODE_EXPIRED, "验证码已过期或未发送");
                return;
            }

            std::string storedCode = result.asString();
            if (storedCode != code)
            {
                onError(ErrorCode::VERIFY_CODE_WRONG, "验证码错误");
                return;
            }

            findUserByEmail(email,
                [onError](const UserRow&)
                {
                    onError(ErrorCode::EMAIL_REGISTERED, "该邮箱已注册");
                },
                [email, password, nickname, onSuccess, onError, codeKey]()
                {
                    std::string hash = PasswordUtil::hash(password);
                    if (hash.empty())
                    {
                        onError(ErrorCode::INTERNAL_ERROR, "密码哈希失败");
                        return;
                    }

                    auto db = DbClient::get();
                    db->execSqlAsync(
                        std::string("INSERT INTO users (email, password_hash, nickname, role, status) "
                        "VALUES (?, ?, ?, 0, 0)"),
                        [email, onSuccess, codeKey](const drogon::orm::Result& result)
                        {
                            int64_t userId = result.insertId();
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
            if (user.status == 1)
            {
                onError(ErrorCode::ACCOUNT_DISABLED, "账号已被禁用");
                return;
            }

            if (!PasswordUtil::verify(password, user.passwordHash))
            {
                onError(ErrorCode::PASSWORD_WRONG, "邮箱或密码错误");
                return;
            }

            JwtPayload payload;
            payload.userId   = user.id;
            payload.email    = user.email;
            payload.role     = user.role;
            payload.nickname = user.nickname;
            std::string token = JwtUtil::sign(payload);

            auto redis = RedisClient::get();
            std::string sessionKey = "session:" + std::to_string(user.id);
            redis->execCommandAsync(
                [token, user, onSuccess]
                (const drogon::nosql::RedisResult&)
                {
                    // 单点登录：旧 WS 连接推送 kicked 后关闭
                    ConnectionManager::instance().kick(user.id, "账号在其他地方登录");

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
