#pragma once

#include <json/json.h>

#include <cstdlib>
#include <sstream>
#include <string>

// 配置读取工具（纯逻辑，无 Drogon 依赖，可独立测试）
//
// 策略：环境变量优先，缺省回退 JSON 对象中的点分路径。
//
// 使用：
//   auto secret = Config::get("JWT_SECRET", customJson, "jwt.secret", "");
//   // 先读 env JWT_SECRET；若为空，再按 "jwt.secret" 遍历 JSON；最后走默认值
namespace online_chat {

class Config
{
public:
    // 读取配置：环境变量优先，回退 JSON 对象的点分路径
    // envKey     : 环境变量名
    // root       : JSON 根节点（由调用方从 drogon::app().getCustomConfig() 传入）
    // configPath : 点分路径，如 "jwt.secret"
    // defaultVal : 兜底默认值
    static std::string get(const std::string& envKey,
                           const Json::Value& root,
                           const std::string& configPath,
                           const std::string& defaultVal = "")
    {
        // 1. 环境变量优先
        const char* envVal = std::getenv(envKey.c_str());
        if (envVal && envVal[0] != '\0')
            return envVal;

        // 2. 遍历 JSON 点分路径
        const Json::Value* node = &root;
        std::string part;
        std::istringstream ss(configPath);
        while (std::getline(ss, part, '.'))
        {
            if (!node->isObject() || !node->isMember(part))
            {
                node = nullptr;
                break;
            }
            node = &((*node)[part]);
        }
        if (node && node->isString())
            return node->asString();

        return defaultVal;
    }

    // int 版本
    static int getInt(const std::string& envKey,
                      const Json::Value& root,
                      const std::string& configPath,
                      int defaultVal = 0)
    {
        const char* envVal = std::getenv(envKey.c_str());
        if (envVal && envVal[0] != '\0')
        {
            try { return std::stoi(envVal); }
            catch (...) {}
        }

        const Json::Value* node = &root;
        std::string part;
        std::istringstream ss(configPath);
        while (std::getline(ss, part, '.'))
        {
            if (!node->isObject() || !node->isMember(part))
            {
                node = nullptr;
                break;
            }
            node = &((*node)[part]);
        }
        if (node && node->isInt())
            return node->asInt();

        return defaultVal;
    }
};

}  // namespace online_chat
