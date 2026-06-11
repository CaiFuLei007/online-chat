#pragma once

#include <string>

// SQL 字符串字面量辅助
//
// 解决 Drogon execSqlAsync 模板推导中 const char[N] 无法隐式转为 std::string 的问题。
// 用法：db->execSqlAsync(SQL("SELECT ..."), successCb, errorCb, args...);
#define SQL(...) std::string(__VA_ARGS__)
