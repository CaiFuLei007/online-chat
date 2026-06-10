#pragma once

#include <drogon/drogon.h>
#include <cstdlib>
#include <string>

// 配置读取工具
//
// 策略：环境变量优先，缺省回退 config.json 中 custom_config 的同名键。
// 用于保护敏感项（JWT 密钥、SMTP 口令等）不进版本库。
//
// 使用：
//   auto secret = Config::get("JWT_SECRET", "jwt.secret");
//   // 先读 env JWT_SECRET；若为空，再读 custom_config.jwt.secret
namespace online_chat {

class Config
{
public:
    // 读取配置：环境变量优先，回退 custom_config 中的 JSON 路径
    // envKey       : 环境变量名，如 "JWT_SECRET"
    // configPath   : custom_config 下的点分路径，如 "jwt.secret"
    // defaultVal   : 都找不到时的默认值
    static std::string get(const std::string& envKey,
                           const std::string& configPath,
                           const std::string& defaultVal = "")
    {
        // 1. 尝试环境变量
        const char* envVal = std::getenv(envKey.c_str());
        if (envVal && envVal[0] != '\0')
            return envVal;

        // 2. 尝试 custom_config
        try
        {
            const auto& custom = drogon::app().getCustomConfig();
            // 按 "." 分割路径逐级下钻
            const Json::Value* node = &custom;
            std::string part;
            std::istringstream ss(configPath);
            while (std::getline(ss, part, '.'))
            {
                if (!node->isMember(part))
                {
                    node = nullptr;
                    break;
                }
                node = &((*node)[part]);
            }
            if (node && node->isString())
                return node->asString();
        }
        catch (...)
        {
            // getCustomConfig 在未加载 config 时可能抛异常，静默处理
        }

        return defaultVal;
    }

    // 读取 int 类型配置（同样环境变量优先）
    static int getInt(const std::string& envKey,
                      const std::string& configPath,
                      int defaultVal = 0)
    {
        const char* envVal = std::getenv(envKey.c_str());
        if (envVal && envVal[0] != '\0')
        {
            try { return std::stoi(envVal); }
            catch (...) {}
        }

        try
        {
            const auto& custom = drogon::app().getCustomConfig();
            const Json::Value* node = &custom;
            std::string part;
            std::istringstream ss(configPath);
            while (std::getline(ss, part, '.'))
            {
                if (!node->isMember(part))
                {
                    node = nullptr;
                    break;
                }
                node = &((*node)[part]);
            }
            if (node && node->isInt())
                return node->asInt();
        }
        catch (...) {}

        return defaultVal;
    }
};

}  // namespace online_chat
