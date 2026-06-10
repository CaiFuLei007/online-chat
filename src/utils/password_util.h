#pragma once

#include <string>
#include <random>
#include <crypt.h>

// 密码工具 — bcrypt 哈希与校验
//
// 使用系统 crypt() 函数实现 bcrypt（$2a$10$...）。
// 链接时需要 -lcrypt。
namespace online_chat {

class PasswordUtil
{
public:
    // 生成 bcrypt 哈希（cost=10）
    static std::string hash(const std::string& password)
    {
        // 生成 16 字节随机 salt
        std::string salt = generateSalt(16);
        // bcrypt salt 格式: $2a$10$<22字符base64>
        std::string bcryptSalt = "$2a$10$" + toBcrypt64(salt);

        const char* result = crypt(password.c_str(), bcryptSalt.c_str());
        if (!result)
            return "";
        return result;
    }

    // 校验密码是否匹配 bcrypt 哈希
    static bool verify(const std::string& password, const std::string& hash)
    {
        // 用已有的 hash 作为 salt 参数，crypt 会自动提取其中的 salt
        const char* result = crypt(password.c_str(), hash.c_str());
        if (!result)
            return false;
        return result == hash;
    }

private:
    // 生成随机字节串
    static std::string generateSalt(size_t length)
    {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 255);

        std::string salt;
        salt.reserve(length);
        for (size_t i = 0; i < length; ++i)
            salt += static_cast<char>(dis(gen));
        return salt;
    }

    // 将原始字节转为 bcrypt 使用的 base64 变体（非标准 base64）
    static std::string toBcrypt64(const std::string& raw)
    {
        static const char table[] =
            "./ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";

        std::string out;
        size_t i = 0;
        while (i < raw.size())
        {
            // 每 3 字节 → 4 字符
            uint32_t val = 0;
            int bytes = 0;
            for (int j = 0; j < 3 && i < raw.size(); ++j, ++i)
            {
                val = (val << 8) | static_cast<uint8_t>(raw[i]);
                ++bytes;
            }
            // 补齐到 3 字节
            val <<= (3 - bytes) * 8;

            out += table[(val >> 18) & 0x3F];
            out += table[(val >> 12) & 0x3F];
            if (bytes > 1)
                out += table[(val >> 6) & 0x3F];
            if (bytes > 2)
                out += table[val & 0x3F];
        }
        // bcrypt salt 需要 22 个字符
        while (out.size() < 22)
            out += '.';
        return out.substr(0, 22);
    }
};

}  // namespace online_chat
