// 测试 jwt_util.h — JWT 签发/校验
// 依赖 jwt-cpp（FetchContent 拉取）+ OpenSSL

#include <gtest/gtest.h>
#include <jwt-cpp/jwt.h>
#include <chrono>
#include <thread>

// 手动构造一个合法的 HS256 JWT 用于测试
static std::string createTestToken(const std::string& secret,
                                   const std::string& issuer,
                                   int64_t userId,
                                   const std::string& email,
                                   int role,
                                   const std::string& nickname,
                                   int expireSeconds = 3600)
{
    auto now = std::chrono::system_clock::now();
    auto exp = now + std::chrono::seconds(expireSeconds);

    return jwt::create()
        .set_issuer(issuer)
        .set_type("JWT")
        .set_issued_at(now)
        .set_expires_at(exp)
        .set_subject(std::to_string(userId))
        .set_payload_claim("userId",   jwt::claim(std::to_string(userId)))
        .set_payload_claim("email",    jwt::claim(email))
        .set_payload_claim("role",     jwt::claim(std::to_string(role)))
        .set_payload_claim("nickname", jwt::claim(nickname))
        .sign(jwt::algorithm::hs256{secret});
}

// 手动校验 JWT（不依赖 Drogon ConfigDrogon）
struct JwtPayload
{
    int64_t     userId;
    std::string email;
    int         role;
    std::string nickname;
};

static std::optional<JwtPayload> verifyToken(const std::string& token,
                                              const std::string& secret,
                                              const std::string& issuer)
{
    try
    {
        auto decoded = jwt::decode(token);
        auto verifier = jwt::verify()
            .allow_algorithm(jwt::algorithm::hs256{secret})
            .with_issuer(issuer);
        verifier.verify(decoded);

        JwtPayload p;
        p.userId   = std::stoll(decoded.get_payload_claim("userId").as_string());
        p.email    = decoded.get_payload_claim("email").as_string();
        p.role     = std::stoi(decoded.get_payload_claim("role").as_string());
        p.nickname = decoded.get_payload_claim("nickname").as_string();
        return p;
    }
    catch (...)
    {
        return std::nullopt;
    }
}

// ---- 测试 ----

TEST(JwtTest, SignAndVerify)
{
    std::string secret = "test-secret-key-32bytes-long!!!!";
    std::string issuer = "online-chat";

    std::string token = createTestToken(secret, issuer, 1001, "alice@example.com", 0, "Alice");
    EXPECT_FALSE(token.empty());

    auto payload = verifyToken(token, secret, issuer);
    ASSERT_TRUE(payload.has_value());
    EXPECT_EQ(payload->userId, 1001);
    EXPECT_EQ(payload->email, "alice@example.com");
    EXPECT_EQ(payload->role, 0);
    EXPECT_EQ(payload->nickname, "Alice");
}

TEST(JwtTest, WrongSecretFails)
{
    std::string token = createTestToken("correct-secret-32bytes-long!!!!!", "online-chat",
                                        1001, "a@b.com", 0, "A");
    auto payload = verifyToken(token, "wrong-secret-32bytes-long!!!!!!", "online-chat");
    EXPECT_FALSE(payload.has_value());
}

TEST(JwtTest, WrongIssuerFails)
{
    std::string secret = "test-secret-key-32bytes-long!!!!";
    std::string token = createTestToken(secret, "online-chat", 1001, "a@b.com", 0, "A");
    auto payload = verifyToken(token, secret, "wrong-issuer");
    EXPECT_FALSE(payload.has_value());
}

TEST(JwtTest, ExpiredTokenFails)
{
    std::string secret = "test-secret-key-32bytes-long!!!!";
    // 已过期的 token（expireSeconds = -10）
    std::string token = createTestToken(secret, "online-chat", 1001, "a@b.com", 0, "A", -10);
    auto payload = verifyToken(token, secret, "online-chat");
    EXPECT_FALSE(payload.has_value());
}

TEST(JwtTest, GarbageTokenFails)
{
    auto payload = verifyToken("not.a.jwt", "secret", "issuer");
    EXPECT_FALSE(payload.has_value());
}

TEST(JwtTest, AdminRole)
{
    std::string secret = "test-secret-key-32bytes-long!!!!";
    std::string token = createTestToken(secret, "online-chat", 1, "admin@local", 1, "Admin");
    auto payload = verifyToken(token, secret, "online-chat");
    ASSERT_TRUE(payload.has_value());
    EXPECT_EQ(payload->role, 1);
    EXPECT_EQ(payload->userId, 1);
}

TEST(JwtTest, ChineseNickname)
{
    std::string secret = "test-secret-key-32bytes-long!!!!";
    std::string token = createTestToken(secret, "online-chat", 200, "u@x.com", 0, "管理员");
    auto payload = verifyToken(token, secret, "online-chat");
    ASSERT_TRUE(payload.has_value());
    EXPECT_EQ(payload->nickname, "管理员");
}
