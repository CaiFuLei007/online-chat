#pragma once

#include "utils/config.h"

#include <drogon/drogon.h>
#include <string>

// Drogon 运行时配置读取包装
//
// 封装 drogon::app().getCustomConfig() 调用，对外提供与 Config 相同的接口。
// 生产代码用这个；测试代码直接用 Config（传入自定义 JSON）。
namespace online_chat {

class ConfigDrogon
{
public:
    static std::string get(const std::string& envKey,
                           const std::string& configPath,
                           const std::string& defaultVal = "")
    {
        try
        {
            return Config::get(envKey, drogon::app().getCustomConfig(),
                               configPath, defaultVal);
        }
        catch (...)
        {
            // getCustomConfig 在未加载 config 时可能抛异常
            const char* envVal = std::getenv(envKey.c_str());
            if (envVal && envVal[0] != '\0')
                return envVal;
            return defaultVal;
        }
    }

    static int getInt(const std::string& envKey,
                      const std::string& configPath,
                      int defaultVal = 0)
    {
        try
        {
            return Config::getInt(envKey, drogon::app().getCustomConfig(),
                                  configPath, defaultVal);
        }
        catch (...)
        {
            const char* envVal = std::getenv(envKey.c_str());
            if (envVal && envVal[0] != '\0')
            {
                try { return std::stoi(envVal); }
                catch (...) {}
            }
            return defaultVal;
        }
    }
};

}  // namespace online_chat
