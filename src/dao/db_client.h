#pragma once

#include <drogon/drogon.h>
#include <functional>
#include <string>
#include <vector>

// MySQL 数据库访问工具
//
// Drogon 通过 config.json 的 db_clients 配置管理 MySQL 连接池，
// 本工具仅封装获取客户端的便捷方法。
//
// 使用示例（异步）：
//   auto db = online_chat::DbClient::get();
//   db->execSqlAsync(
//       "SELECT id,nickname FROM users WHERE email=?",
//       [callback](const drogon::orm::Result& r) { ... },
//       [exception](const drogon::orm::DrogonDbException& e) { ... },
//       email);
//
// 使用示例（同步，仅用于初始化/迁移等低频场景，禁止在请求处理中使用）：
//   auto r = db->execSqlSync("SELECT 1");
namespace online_chat {

class DbClient
{
public:
    // 获取默认数据库客户端（连接池，线程安全）
    static drogon::orm::DbClientPtr get()
    {
        return drogon::app().getDbClient();
    }

    // 获取指定名称的数据库客户端（config 中 name 字段）
    static drogon::orm::DbClientPtr get(const std::string& name)
    {
        return drogon::app().getDbClient(name);
    }
};

}  // namespace online_chat
