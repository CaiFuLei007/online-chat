#pragma once

#include "utils/config_drogon.h"

#include <drogon/drogon.h>
#include <string>
#include <sstream>
#include <fstream>
#include <cstdio>
#include <unistd.h>

// SMTP 邮件发送工具
//
// 通过 curl 发送 SMTP 邮件（容器中预装 curl）。
// 所有用户可控参数均做壳转义，防止命令注入。
namespace online_chat {

class SmtpClient
{
public:
    // 发送邮件（同步，应在线程池中调用）
    static bool send(const std::string& toEmail,
                     const std::string& subject,
                     const std::string& body)
    {
        const std::string host     = ConfigDrogon::get("SMTP_HOST",     "smtp.host",     "smtp.example.com");
        const std::string port     = ConfigDrogon::get("SMTP_PORT",     "smtp.port",     "465");
        const std::string username = ConfigDrogon::get("SMTP_USERNAME", "smtp.username", "");
        const std::string password = ConfigDrogon::get("SMTP_PASSWORD", "smtp.password", "");
        const std::string fromName = ConfigDrogon::get("SMTP_FROM_NAME","smtp.from_name","online-chat");
        const std::string fromAddr = ConfigDrogon::get("SMTP_FROM_EMAIL","smtp.from_email","noreply@example.com");
        const bool useSsl          = ConfigDrogon::get("SMTP_USE_SSL",  "smtp.use_ssl",  "true") == "true";

        if (username.empty() || password.empty())
        {
            LOG_ERROR << "SMTP credentials not configured, cannot send email";
            return false;
        }

        // 构造完整的 RFC 2822 邮件内容（头 + 空行 + 正文）
        std::ostringstream mailData;
        mailData << "From: " << fromName << " <" << fromAddr << ">\r\n"
                 << "To: <" << toEmail << ">\r\n"
                 << "Subject: " << subject << "\r\n"
                 << "Content-Type: text/plain; charset=UTF-8\r\n"
                 << "\r\n"
                 << body;

        // 写入临时文件
        const std::string tmpFile = "/tmp/smtp_mail_" + std::to_string(getpid()) + ".eml";
        {
            std::ofstream ofs(tmpFile, std::ios::binary);
            if (!ofs)
            {
                LOG_ERROR << "Failed to create temp file: " << tmpFile;
                return false;
            }
            ofs << mailData.str();
        }

        // 构造 curl 命令
        std::ostringstream cmd;
        cmd << "curl -sS --max-time 30"
            << " --url "       << shellQuote((useSsl ? "smtps://" : "smtp://") + host + ":" + port)
            << " --mail-from " << shellQuote(fromAddr)
            << " --mail-rcpt " << shellQuote(toEmail)
            << " --user "      << shellQuote(username + ":" + password)
            << " --upload-file " << shellQuote(tmpFile)
            << " 2>&1";

        LOG_DEBUG << "SMTP sending to " << toEmail;
        int ret = std::system(cmd.str().c_str());

        // 清理临时文件
        std::remove(tmpFile.c_str());

        if (ret != 0)
        {
            LOG_ERROR << "SMTP send failed to " << toEmail << ", curl exit code: " << ret;
            return false;
        }

        LOG_INFO << "SMTP sent to " << toEmail;
        return true;
    }

private:
    // 将字符串用单引号包裹，安全用于 POSIX shell
    // 单引号内的内容原样保留；单引号本身用 '\'' 断开再续
    static std::string shellQuote(const std::string& s)
    {
        std::string out = "'";
        for (char c : s)
        {
            if (c == '\'')
                out += "'\\''";
            else
                out += c;
        }
        out += "'";
        return out;
    }
};

}  // namespace online_chat
