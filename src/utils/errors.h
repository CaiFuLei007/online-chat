#pragma once

#include <string>

// 统一错误码定义
//
// 所有 HTTP 接口与 WebSocket 事件的错误码在此集中管理，
// 前端根据 code 做统一错误处理，后端各模块只允许引用此枚举。
namespace online_chat {

enum class ErrorCode : int
{
    // 成功
    OK = 0,

    // 通用错误 (1xxx)
    INVALID_PARAMS       = 1001,   // 请求参数校验失败
    UNAUTHORIZED         = 1002,   // 未登录或 token 无效/过期
    FORBIDDEN            = 1003,   // 权限不足
    NOT_FOUND            = 1004,   // 资源不存在
    INTERNAL_ERROR       = 1005,   // 服务端内部错误
    TOO_MANY_REQUESTS    = 1006,   // 频率限制

    // 认证模块 (2xxx)
    VERIFY_CODE_EXPIRED  = 2001,   // 验证码已过期
    VERIFY_CODE_WRONG    = 2002,   // 验证码错误
    EMAIL_REGISTERED     = 2003,   // 邮箱已注册
    EMAIL_NOT_VERIFIED   = 2004,   // 邮箱未验证（未发送验证码）
    PASSWORD_WRONG       = 2005,   // 密码错误
    ACCOUNT_DISABLED     = 2006,   // 账号被禁用
    RESEND_TOO_FAST      = 2007,   // 验证码重发过快

    // 好友模块 (3xxx)
    ALREADY_FRIENDS      = 3001,   // 已是好友
    FRIEND_REQUEST_SENT  = 3002,   // 已发送过申请（待处理）
    FRIEND_NOT_FOUND     = 3003,   // 好友关系不存在
    CANNOT_ADD_SELF      = 3004,   // 不能加自己为好友

    // 群聊模块 (4xxx)
    GROUP_NOT_FOUND      = 4001,   // 群不存在
    ALREADY_IN_GROUP     = 4002,   // 已在群中
    NOT_IN_GROUP         = 4003,   // 不在群中
    NOT_GROUP_OWNER      = 4004,   // 不是群主
    GROUP_REQUEST_SENT   = 4005,   // 已发送过加群申请

    // 消息模块 (5xxx)
    CONV_NOT_FOUND       = 5001,   // 会话不存在（非好友/不在群中）
};

// 错误码 → 可读消息（仅用于日志和开发调试，前端不依赖此文本）
inline const char* errorMessage(ErrorCode code)
{
    switch (code) {
    case ErrorCode::OK:                  return "success";
    case ErrorCode::INVALID_PARAMS:      return "invalid parameters";
    case ErrorCode::UNAUTHORIZED:        return "unauthorized";
    case ErrorCode::FORBIDDEN:           return "forbidden";
    case ErrorCode::NOT_FOUND:           return "not found";
    case ErrorCode::INTERNAL_ERROR:      return "internal server error";
    case ErrorCode::TOO_MANY_REQUESTS:   return "too many requests";

    case ErrorCode::VERIFY_CODE_EXPIRED: return "verify code expired";
    case ErrorCode::VERIFY_CODE_WRONG:   return "verify code wrong";
    case ErrorCode::EMAIL_REGISTERED:    return "email already registered";
    case ErrorCode::EMAIL_NOT_VERIFIED:  return "email not verified";
    case ErrorCode::PASSWORD_WRONG:      return "password wrong";
    case ErrorCode::ACCOUNT_DISABLED:    return "account disabled";
    case ErrorCode::RESEND_TOO_FAST:     return "resend too fast";

    case ErrorCode::ALREADY_FRIENDS:     return "already friends";
    case ErrorCode::FRIEND_REQUEST_SENT: return "friend request already sent";
    case ErrorCode::FRIEND_NOT_FOUND:    return "friendship not found";
    case ErrorCode::CANNOT_ADD_SELF:     return "cannot add yourself";

    case ErrorCode::GROUP_NOT_FOUND:     return "group not found";
    case ErrorCode::ALREADY_IN_GROUP:    return "already in group";
    case ErrorCode::NOT_IN_GROUP:        return "not in group";
    case ErrorCode::NOT_GROUP_OWNER:     return "not group owner";
    case ErrorCode::GROUP_REQUEST_SENT:  return "group request already sent";

    case ErrorCode::CONV_NOT_FOUND:      return "conversation not found";
    }
    return "unknown error";
}

}  // namespace online_chat
