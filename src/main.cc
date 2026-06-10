#include <drogon/drogon.h>

#include <filesystem>

// online-chat 服务入口
//
// 阶段 0.1：仅建立可启动的最小骨架。
// - 若存在 ./config/config.json（阶段 0.2 提供），按配置文件启动；
// - 否则使用兜底监听 0.0.0.0:8080，保证骨架在缺少配置时也能跑起来。
int main()
{
    const std::string configFile = "./config/config.json";

    if (std::filesystem::exists(configFile))
    {
        drogon::app().loadConfigFile(configFile);
    }
    else
    {
        // 兜底配置：阶段 0.2 引入完整 config.json 后，此分支不再走到
        drogon::app().addListener("0.0.0.0", 8080);
        LOG_WARN << "config file not found: " << configFile
                 << ", fall back to default listener 0.0.0.0:8080";
    }

    LOG_INFO << "online-chat server starting...";
    drogon::app().run();
    return 0;
}
