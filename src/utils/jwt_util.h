#pragma once

#include "utils/config_drogon.h"

#include <drogon/drogon.h>
#include <jwt-cpp/jwt.h>
#include <string>
#include <optional>
#include <chrono>

// JWT 签发/校验工具
//
// 使用 jwt-cpp（header-only）+ OpenSSL 实现 HS256 签名。
// 密钥从 config 的 jwt.secret 或环境变量 JWT_SECRET 读取。
namespace online_chat {

struct JwtPayload
{
    int64_t     userId;
    std::string email;
    int         role;       // 0=普通用户 1=超管
    std::string nickname;
};

class JwtUtil
{
public:
    // 签发 JWT
    static std::string sign(const JwtPayload& payload)
    {
        const std::string secret  = getSecret();
        const int expireSeconds   = ConfigDrogon::getInt("JWT_SECRET", "jwt.expire_seconds", 604800);
        const std::string issuer  = ConfigDrogon::get("JWT_SECRET", "jwt.issuer", "online-chat");

        auto now = std::chrono::system_clock::now();
        auto exp = now + std::chrono::seconds(expireSeconds);

        auto token = jwt::create()
            .set_issuer(issuer)
            .set_type("JWT")
            .set_issued_at(now)
            .set_expires_at(exp)
            .set_subject(std::to_string(payload.userId))
            .set_payload_claim("userId", jwt::claim(std::to_string(payload.userId)))
            .set_payload_claim("email",  jwt::claim(payload.email))
            .set_payload_claim("role",   jwt::claim(std::to_string(payload.role)))
            .set_payload_claim("nickname", jwt::claim(payload.nickname))
            .sign(jwt::algorithm::hs256{secret});

        return token;
    }

    // 校验 JWT，成功返回 payload，失败返回 nullopt
    static std::optional<JwtPayload> verify(const std::string& token)
    {
        const std::string secret = getSecret();

        try
        {
            auto decoded = jwt::decode(token);

            auto verifier = jwt::verify()
                .allow_algorithm(jwt::algorithm::hs256{secret})
                .with_issuer(ConfigDrogon::get("JWT_SECRET", "jwt.issuer", "online-chat"));

            verifier.verify(decoded);

            JwtPayload p;
            p.userId   = std::stoll(decoded.get_payload_claim("userId").as_string());
            p.email    = decoded.get_payload_claim("email").as_string();
            p.role     = std::stoi(decoded.get_payload_claim("role").as_string());
            p.nickname = decoded.get_payload_claim("nickname").as_string();
            return p;
        }
        catch (const std::exception& e)
        {
            LOG_DEBUG << "JWT verify failed: " << e.what();
            return std::nullopt;
        }
    }

private:
    static std::string getSecret()
    {
        // 优先读环境变量，回退 config
        const char* env = std::getenv("JWT_SECRET");
        if (env && env[0] != '\0')
            return env;
        return ConfigDrogon::get("JWT_SECRET", "jwt.secret", "");
    }
};

}  // namespace online_chat
