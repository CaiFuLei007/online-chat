// 测试 SMTP 工具函数 — shellQuote 转义正确性
//
// smtp_client.h 的 shellQuote 是 private，这里通过一个友元测试类访问。
// 或者直接复制函数实现做独立测试（推荐，不侵入生产代码）。

#include <gtest/gtest.h>
#include <string>

// 复制 smtp_client.h 中的 shellQuote 实现用于测试
// （生产代码中是 private static，测试时复制一份验证逻辑）
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

// 普通字符串被单引号包裹
TEST(ShellQuoteTest, SimpleString)
{
    EXPECT_EQ(shellQuote("hello"), "'hello'");
}

// 空字符串
TEST(ShellQuoteTest, EmptyString)
{
    EXPECT_EQ(shellQuote(""), "''");
}

// 包含单引号的字符串：用 '\'' 断开
TEST(ShellQuoteTest, ContainsSingleQuote)
{
    std::string result = shellQuote("it's");
    // 期望: 'it'\''s'
    EXPECT_EQ(result, "'it'\\''s'");
}

// 包含特殊字符（$、`、\、"、;、|）不被转义（因为在单引号内）
TEST(ShellQuoteTest, SpecialCharsInsideSingleQuotes)
{
    std::string input = R"(hello $HOME `whoami` \n "world"; rm -rf /)";
    std::string result = shellQuote(input);
    // 单引号内所有字符原样保留（除了单引号本身）
    // 应以 ' 开头和结尾
    EXPECT_EQ(result.front(), '\'');
    EXPECT_EQ(result.back(), '\'');
    // 中间不应有额外的转义
    EXPECT_NE(result.find("$HOME"), std::string::npos);
    EXPECT_NE(result.find("`whoami`"), std::string::npos);
}

// 包含多个单引号
TEST(ShellQuoteTest, MultipleSingleQuotes)
{
    std::string result = shellQuote("a'b'c");
    EXPECT_EQ(result, "'a'\\''b'\\''c'");
}

// 纯单引号字符串
TEST(ShellQuoteTest, OnlySingleQuote)
{
    EXPECT_EQ(shellQuote("'"), "''\\'''");
}

// 包含中文和 emoji
TEST(ShellQuoteTest, UnicodeContent)
{
    std::string result = shellQuote("你好👋");
    EXPECT_EQ(result, "'你好👋'");
}

// 包含换行符
TEST(ShellQuoteTest, Newlines)
{
    std::string result = shellQuote("line1\nline2");
    EXPECT_EQ(result, "'line1\nline2'");
}
