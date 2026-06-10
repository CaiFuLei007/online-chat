// 测试 config.h — Config::get / Config::getInt（纯逻辑，无 Drogon 依赖）
// 直接构造 Json::Value 传入，不依赖 drogon::app()

#include <gtest/gtest.h>
#include "utils/config.h"

#include <json/json.h>
#include <cstdlib>

using namespace online_chat;

// 辅助：设置环境变量
static void setEnv(const char* key, const char* val)
{
    setenv(key, val, 1);
}

// 辅助：清除环境变量
static void unsetEnv(const char* key)
{
    unsetenv(key);
}

// 辅助：构造带嵌套的 JSON
static Json::Value makeConfig()
{
    Json::Value root;
    root["jwt"]["secret"] = "dev-secret";
    root["jwt"]["expire"] = 604800;
    root["smtp"]["host"]  = "smtp.example.com";
    root["smtp"]["port"]  = 465;
    return root;
}

class ConfigTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        unsetEnv("TEST_JWT_SECRET");
        unsetEnv("TEST_SMTP_HOST");
        unsetEnv("TEST_EXPIRE");
    }
};

// 环境变量优先于 JSON
TEST_F(ConfigTest, EnvOverridesJson)
{
    setEnv("TEST_JWT_SECRET", "env-secret");
    auto root = makeConfig();
    auto val = Config::get("TEST_JWT_SECRET", root, "jwt.secret", "fallback");
    EXPECT_EQ(val, "env-secret");
}

// 环境变量为空时回退 JSON
TEST_F(ConfigTest, EmptyEnvFallsBackToJson)
{
    setEnv("TEST_JWT_SECRET", "");
    auto root = makeConfig();
    auto val = Config::get("TEST_JWT_SECRET", root, "jwt.secret", "fallback");
    EXPECT_EQ(val, "dev-secret");
}

// 环境变量未设置时回退 JSON
TEST_F(ConfigTest, MissingEnvFallsBackToJson)
{
    auto root = makeConfig();
    auto val = Config::get("TEST_JWT_SECRET", root, "jwt.secret", "fallback");
    EXPECT_EQ(val, "dev-secret");
}

// JSON 路径不存在时返回默认值
TEST_F(ConfigTest, MissingJsonPathReturnsDefault)
{
    auto root = makeConfig();
    auto val = Config::get("NONEXIST", root, "nonexist.path", "my-default");
    EXPECT_EQ(val, "my-default");
}

// 空 JSON 对象 + 无环境变量 → 默认值
TEST_F(ConfigTest, EmptyJsonReturnsDefault)
{
    Json::Value empty;
    auto val = Config::get("NONEXIST", empty, "jwt.secret", "fallback");
    EXPECT_EQ(val, "fallback");
}

// 深层嵌套路径
TEST_F(ConfigTest, DeepNestedPath)
{
    Json::Value root;
    root["a"]["b"]["c"] = "deep-value";
    auto val = Config::get("NONEXIST", root, "a.b.c", "");
    EXPECT_EQ(val, "deep-value");
}

// getInt: 环境变量优先
TEST_F(ConfigTest, IntEnvOverridesJson)
{
    setEnv("TEST_EXPIRE", "999");
    auto root = makeConfig();
    auto val = Config::getInt("TEST_EXPIRE", root, "jwt.expire", 0);
    EXPECT_EQ(val, 999);
}

// getInt: 回退 JSON
TEST_F(ConfigTest, IntFallsBackToJson)
{
    auto root = makeConfig();
    auto val = Config::getInt("NONEXIST", root, "jwt.expire", 0);
    EXPECT_EQ(val, 604800);
}

// getInt: 环境变量非法时走默认值
TEST_F(ConfigTest, IntInvalidEnvFallsBack)
{
    setEnv("TEST_EXPIRE", "not-a-number");
    Json::Value empty;
    auto val = Config::getInt("TEST_EXPIRE", empty, "jwt.expire", 42);
    EXPECT_EQ(val, 42);
}

// getInt: 默认值
TEST_F(ConfigTest, IntDefault)
{
    Json::Value empty;
    auto val = Config::getInt("NONEXIST", empty, "nonexist", 123);
    EXPECT_EQ(val, 123);
}

// get: 路径中间节点不是对象时返回默认值
TEST_F(ConfigTest, PathThroughNonObjectReturnsDefault)
{
    Json::Value root;
    root["jwt"] = "not-an-object";
    auto val = Config::get("NONEXIST", root, "jwt.secret", "fallback");
    EXPECT_EQ(val, "fallback");
}
