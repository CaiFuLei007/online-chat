#pragma once

#include "utils/config.h"

#include <drogon/drogon.h>
#include <functional>
#include <string>
#include <sstream>
#include <array>
#include <memory>

// SMTP 邮件发送工具
//
// 用于发送注册验证码邮件。
// 实现方式：通过 popen 调用系统 curl 发送 SMTP 邮件（容器中预装 curl）。
// 不引入额外第三方 SMTP 库，保持依赖精简。
//
// config 中需要配置（custom_config.smtp）：
//   host / port / use_ssl / username / password / from_name / from_email
//
// 环境变量覆盖：
//   SMTP_HOST / SMTP_PORT / SMTP_USERNAME / SMTP_PASSWORD
namespace online_chat {

class SmtpClient
{
public:
    // 发送邮件（同步，应在线程池中调用，不要阻塞主循环）
    // 返回 true 表示 curl 命令执行成功（退出码 0），不代表邮件一定送达
    static bool send(const std::string& toEmail,
                     const std::string& subject,
                     const std::string& body)
    {
        const std::string host     = Config::get("SMTP_HOST",     "smtp.host",     "smtp.example.com");
        const std::string port     = Config::get("SMTP_PORT",     "smtp.port",     "465");
        const std::string username = Config::get("SMTP_USERNAME", "smtp.username", "");
        const std::string password = Config::get("SMTP_PASSWORD", "smtp.password", "");
        const std::string fromName = Config::get("SMTP_FROM_NAME","smtp.from_name","online-chat");
        const std::string fromAddr = Config::get("SMTP_FROM_EMAIL","smtp.from_email","noreply@example.com");
        const bool useSsl          = Config::get("SMTP_USE_SSL",  "smtp.use_ssl",  "true") == "true";

        if (username.empty() || password.empty())
        {
            LOG_ERROR << "SMTP credentials not configured, cannot send email";
            return false;
        }

        // 构造 curl 命令
        // --url            : SMTP 服务器地址（smtps:// 表示 SSL）
        // --mail-from      : 发件人
        // --mail-rcpt      : 收件人
        // --user           : 认证
        // -H "Subject: ..." : 邮件头
        // --upload-file -  : 从 stdin 读邮件正文
        std::ostringstream cmd;
        cmd << "curl -sS --max-time 30"
            << " --url " << (useSsl ? "smtps://" : "smtp://") << host << ":" << port
            << " --mail-from " << fromAddr
            << " --mail-rcpt " << toEmail
            << " --user " << username << ":" << password
            << " -H 'From: " << fromName << " <" << fromAddr << ">'"
            << " -H 'To: <" << toEmail << ">'"
            << " -H 'Subject: " << subject << "'"
            << " -H 'Content-Type: text/plain; charset=UTF-8'"
            << " --upload-file -"
            << " 2>&1";

        // 邮件正文通过 stdin 传入（curl --upload-file - 读 stdin）
        const std::string fullCmd = "echo '" + escapeShell(body) + "' | " + cmd.str();

        LOG_DEBUG << "SMTP sending to " << toEmail;
        int ret = std::system(fullCmd.c_str());
        if (ret != 0)
        {
            LOG_ERROR << "SMTP send failed to " << toEmail << ", curl exit code: " << ret;
            return false;
        }

        LOG_INFO << "SMTP sent to " << toEmail << " subject=" << subject;
        return true;
    }

private:
    // 对 shell 特殊字符做转义（防止邮件正文破坏命令行）
    static std::string escapeShell(const std::string& s)
    {
        std::string out;
        out.reserve(s.size());
        for (char c : s)
        {
            switch (c)
            {
            case '\'': out += "'\\''"; break;
            case '\\': out += "\\\\"; break;
            default:   out += c;       break;
            }
        }
        return out;
    }
};

}  // namespace online_chat
