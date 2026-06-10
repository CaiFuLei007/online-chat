#pragma once

#include <string>
#include <algorithm>

// 会话工具
//
// conv_key: 单聊会话键，约定格式 "min(uid)-max(uid)"
// 用于 single_messages 表的 conv_key 字段和消息序号 Redis 键
namespace online_chat {

inline std::string convKey(int64_t userIdA, int64_t userIdB)
{
    int64_t lo = std::min(userIdA, userIdB);
    int64_t hi = std::max(userIdA, userIdB);
    return std::to_string(lo) + "-" + std::to_string(hi);
}

// 从 conv_key 字符串解析出两个 userId
// 返回 false 表示格式错误
inline bool parseConvKey(const std::string& key, int64_t& userIdA, int64_t& userIdB)
{
    auto pos = key.find('-');
    if (pos == std::string::npos || pos == 0 || pos == key.size() - 1)
        return false;
    try
    {
        size_t parsedA = 0, parsedB = 0;
        userIdA = std::stoll(key.substr(0, pos), &parsedA);
        userIdB = std::stoll(key.substr(pos + 1), &parsedB);
        // 确保两个数完全消费了各自的子串
        if (parsedA != pos || parsedB != key.size() - pos - 1)
            return false;
        return true;
    }
    catch (...)
    {
        return false;
    }
}

}  // namespace online_chat
