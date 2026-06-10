// 测试 errors.h — ErrorCode 枚举与 errorMessage 映射
// 零外部依赖，纯标准库

#include <gtest/gtest.h>
#include "utils/errors.h"

using namespace online_chat;

// 验证 OK 码为 0
TEST(ErrorsTest, OkIsZero)
{
    EXPECT_EQ(static_cast<int>(ErrorCode::OK), 0);
}

// 验证各域错误码范围
TEST(ErrorsTest, AuthCodesInRange)
{
    EXPECT_GE(static_cast<int>(ErrorCode::VERIFY_CODE_EXPIRED), 2000);
    EXPECT_LE(static_cast<int>(ErrorCode::RESEND_TOO_FAST), 2999);
}

TEST(ErrorsTest, FriendCodesInRange)
{
    EXPECT_GE(static_cast<int>(ErrorCode::ALREADY_FRIENDS), 3000);
    EXPECT_LE(static_cast<int>(ErrorCode::CANNOT_ADD_SELF), 3999);
}

TEST(ErrorsTest, GroupCodesInRange)
{
    EXPECT_GE(static_cast<int>(ErrorCode::GROUP_NOT_FOUND), 4000);
    EXPECT_LE(static_cast<int>(ErrorCode::GROUP_REQUEST_SENT), 4999);
}

TEST(ErrorsTest, MessageCodesInRange)
{
    EXPECT_GE(static_cast<int>(ErrorCode::CONV_NOT_FOUND), 5000);
    EXPECT_LE(static_cast<int>(ErrorCode::CONV_NOT_FOUND), 5999);
}

// 验证每个错误码都有非空消息
TEST(ErrorsTest, AllCodesHaveMessage)
{
    std::vector<ErrorCode> codes = {
        ErrorCode::OK,
        ErrorCode::INVALID_PARAMS, ErrorCode::UNAUTHORIZED, ErrorCode::FORBIDDEN,
        ErrorCode::NOT_FOUND, ErrorCode::INTERNAL_ERROR, ErrorCode::TOO_MANY_REQUESTS,
        ErrorCode::VERIFY_CODE_EXPIRED, ErrorCode::VERIFY_CODE_WRONG,
        ErrorCode::EMAIL_REGISTERED, ErrorCode::EMAIL_NOT_VERIFIED,
        ErrorCode::PASSWORD_WRONG, ErrorCode::ACCOUNT_DISABLED, ErrorCode::RESEND_TOO_FAST,
        ErrorCode::ALREADY_FRIENDS, ErrorCode::FRIEND_REQUEST_SENT,
        ErrorCode::FRIEND_NOT_FOUND, ErrorCode::CANNOT_ADD_SELF,
        ErrorCode::GROUP_NOT_FOUND, ErrorCode::ALREADY_IN_GROUP,
        ErrorCode::NOT_IN_GROUP, ErrorCode::NOT_GROUP_OWNER, ErrorCode::GROUP_REQUEST_SENT,
        ErrorCode::CONV_NOT_FOUND,
    };
    for (auto code : codes)
    {
        const char* msg = errorMessage(code);
        EXPECT_NE(msg, nullptr);
        EXPECT_GT(strlen(msg), 0u) << "code=" << static_cast<int>(code);
    }
}

// 验证具体消息文本
TEST(ErrorsTest, SpecificMessages)
{
    EXPECT_STREQ(errorMessage(ErrorCode::OK), "success");
    EXPECT_STREQ(errorMessage(ErrorCode::UNAUTHORIZED), "unauthorized");
    EXPECT_STREQ(errorMessage(ErrorCode::PASSWORD_WRONG), "password wrong");
    EXPECT_STREQ(errorMessage(ErrorCode::NOT_GROUP_OWNER), "not group owner");
}
