#include <drogon/drogon.h>

#include <filesystem>
#include <string>

// online-chat 服务入口
//
// 启动流程：
//   1. 加载 config/config.json（Drogon 自动初始化 MySQL / Redis 连接池）
//   2. 启动 HTTP + WebSocket 服务
int main()
{
    const std::string configFile = "./config/config.json";

    if (std::filesystem::exists(configFile))
    {
        drogon::app().loadConfigFile(configFile);
    }
    else
    {
        drogon::app().addListener("0.0.0.0", 8080);
        LOG_WARN << "config file not found: " << configFile
                 << ", fall back to default listener 0.0.0.0:8080";
    }

    LOG_INFO << "online-chat server starting...";
    drogon::app().run();
    return 0;
}
