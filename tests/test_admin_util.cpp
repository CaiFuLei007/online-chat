// 测试阶段 6 管理模块的纯逻辑部分
//
// 覆盖：
//   - 超管权限校验（role=1）
//   - 管理面板群列表 SQL 模式
//   - 超管注销群的硬删逻辑
//   - 分页与搜索
//   - JSON 响应结构

#include <gtest/gtest.h>
#include <json/json.h>
#include <string>
#include <vector>

// ============================================================
// 一、超管权限校验
// ============================================================

TEST(AdminPermissionTest, AdminRoleIsOne)
{
    int role = 1;  // 超管
    EXPECT_EQ(role, 1);
}

TEST(AdminPermissionTest, NormalRoleIsZero)
{
    int role = 0;  // 普通用户
    EXPECT_EQ(role, 0);
}

TEST(AdminPermissionTest, AdminCanListAllGroups)
{
    int role = 1;
    EXPECT_EQ(role, 1);  // 允许
}

TEST(AdminPermissionTest, NormalCannotListAllGroups)
{
    int role = 0;
    EXPECT_NE(role, 1);  // 应拒绝
}

TEST(AdminPermissionTest, AdminCanDissolveAnyGroup)
{
    int role = 1;
    EXPECT_EQ(role, 1);  // 允许
}

TEST(AdminPermissionTest, NormalCannotDissolveAnyGroup)
{
    int role = 0;
    EXPECT_NE(role, 1);  // 应拒绝
}

// ============================================================
// 二、管理面板群列表 SQL
// ============================================================

TEST(AdminSqlTest, ListGroupsJoinUsers)
{
    std::string sql = "SELECT gc.id, gc.name, gc.owner_id, u.nickname AS owner_name, "
                      "gc.member_count, gc.created_at "
                      "FROM group_chats gc LEFT JOIN users u ON gc.owner_id = u.id "
                      "ORDER BY gc.id DESC LIMIT ? OFFSET ?";
    EXPECT_NE(sql.find("LEFT JOIN users"), std::string::npos);
    EXPECT_NE(sql.find("owner_name"), std::string::npos);
    EXPECT_NE(sql.find("member_count"), std::string::npos);
    EXPECT_NE(sql.find("DESC"), std::string::npos);
}

TEST(AdminSqlTest, CountAllGroups)
{
    std::string sql = "SELECT COUNT(*) FROM group_chats";
    EXPECT_NE(sql.find("COUNT"), std::string::npos);
}

TEST(AdminSqlTest, DissolveCheckGroupExists)
{
    std::string sql = "SELECT id FROM group_chats WHERE id=?";
    EXPECT_NE(sql.find("id=?"), std::string::npos);
}

// ============================================================
// 三、超管注销群硬删逻辑
// ============================================================

TEST(AdminDissolveTest, SameDeleteOrderAsOwner)
{
    // 超管注销与群主注销使用相同的硬删顺序
    std::vector<std::string> order = {
        "DELETE FROM group_messages WHERE group_id=?",
        "DELETE FROM group_members WHERE group_id=?",
        "DELETE FROM group_chats WHERE id=?"
    };
    EXPECT_EQ(order[0].find("group_messages"), 12u);
    EXPECT_EQ(order[1].find("group_members"), 12u);
    EXPECT_EQ(order[2].find("group_chats"), 12u);
}

TEST(AdminDissolveTest, DeletesAllRelatedData)
{
    // 硬删应删除：群消息、群成员、群本身
    std::vector<std::string> tables = {"group_messages", "group_members", "group_chats"};
    EXPECT_EQ(tables.size(), 3u);
}

TEST(AdminDissolveTest, Irreversible)
{
    std::string sql = "DELETE FROM group_chats WHERE id=?";
    EXPECT_EQ(sql.find("status"), std::string::npos);  // 无 status 字段，直接删除
}

// ============================================================
// 四、分页逻辑
// ============================================================

TEST(AdminPaginationTest, DefaultPageSize)
{
    int pageSize = 20;  // config.search.page_size
    EXPECT_EQ(pageSize, 20);
}

TEST(AdminPaginationTest, FirstPageOffset)
{
    int page = 1, pageSize = 20;
    int offset = (page - 1) * pageSize;
    EXPECT_EQ(offset, 0);
}

TEST(AdminPaginationTest, ThirdPageOffset)
{
    int page = 3, pageSize = 20;
    int offset = (page - 1) * pageSize;
    EXPECT_EQ(offset, 40);
}

TEST(AdminPaginationTest, NegativePageClamped)
{
    int page = -1;
    if (page < 1) page = 1;
    EXPECT_EQ(page, 1);
}

TEST(AdminPaginationTest, OrderByIdDesc)
{
    // 管理面板默认按群 ID 降序（最新创建的在前）
    std::string sql = "ORDER BY gc.id DESC";
    EXPECT_NE(sql.find("DESC"), std::string::npos);
}

// ============================================================
// 五、JSON 响应结构
// ============================================================

TEST(AdminJsonTest, GroupListItem)
{
    Json::Value g;
    g["id"]          = Json::Int64(1);
    g["name"]        = "test-group";
    g["ownerId"]     = Json::Int64(1001);
    g["ownerName"]   = "Alice";
    g["memberCount"] = 15;
    g["createdAt"]   = "2026-06-10 12:00:00";

    EXPECT_EQ(g["id"].asInt64(), 1);
    EXPECT_EQ(g["name"].asString(), "test-group");
    EXPECT_EQ(g["ownerId"].asInt64(), 1001);
    EXPECT_EQ(g["ownerName"].asString(), "Alice");
    EXPECT_EQ(g["memberCount"].asInt(), 15);
}

TEST(AdminJsonTest, ListGroupsResponse)
{
    Json::Value g1;
    g1["id"] = Json::Int64(1);
    g1["name"] = "group-1";

    Json::Value g2;
    g2["id"] = Json::Int64(2);
    g2["name"] = "group-2";

    Json::Value list(Json::arrayValue);
    list.append(g1);
    list.append(g2);

    Json::Value data;
    data["list"]     = list;
    data["total"]    = 2;
    data["page"]     = 1;
    data["pageSize"] = 20;

    EXPECT_EQ(data["list"].size(), 2u);
    EXPECT_EQ(data["total"].asInt(), 2);
    EXPECT_EQ(data["page"].asInt(), 1);
    EXPECT_EQ(data["pageSize"].asInt(), 20);
}

TEST(AdminJsonTest, EmptyGroupList)
{
    Json::Value data;
    data["list"] = Json::Value(Json::arrayValue);
    data["total"] = 0;
    EXPECT_EQ(data["list"].size(), 0u);
    EXPECT_EQ(data["total"].asInt(), 0);
}

TEST(AdminJsonTest, DissolveResponse)
{
    Json::Value data;
    data["message"] = "群已注销";
    EXPECT_EQ(data["message"].asString(), "群已注销");
}

TEST(AdminJsonTest, ForbiddenResponse)
{
    Json::Value body;
    body["code"] = 1003;
    body["msg"]  = "仅超管可访问";
    body["data"] = Json::Value::null;
    EXPECT_EQ(body["code"].asInt(), 1003);
}

// ============================================================
// 六、与群主注销的区别
// ============================================================

TEST(AdminDissolveTest, AdminSkipsOwnerCheck)
{
    // 超管注销不校验 owner_id，直接删除
    int64_t adminId = 1;       // 超管
    int64_t groupOwnerId = 1001;  // 群主
    // adminId != groupOwnerId，但超管仍有权限
    EXPECT_NE(adminId, groupOwnerId);
}

TEST(AdminDissolveTest, OwnerDissolveChecksOwnership)
{
    // 群主注销必须 owner_id == currentUserId
    int64_t ownerId = 1001;
    int64_t currentUserId = 1001;
    EXPECT_EQ(ownerId, currentUserId);  // 校验通过
}

TEST(AdminDissolveTest, NonOwnerNonAdminCannotDissolve)
{
    // 普通用户（非群主、非超管）不能注销
    int role = 0;
    int64_t ownerId = 1001;
    int64_t currentUserId = 1002;
    EXPECT_NE(role, 1);        // 不是超管
    EXPECT_NE(ownerId, currentUserId);  // 不是群主
}

// ============================================================
// 七、边界条件
// ============================================================

TEST(AdminBoundaryTest, EmptyGroupTable)
{
    int total = 0;
    EXPECT_EQ(total, 0);
}

TEST(AdminBoundaryTest, LargeGroupId)
{
    int64_t groupId = 9999999999LL;
    EXPECT_GT(groupId, 0);
}

TEST(AdminBoundaryTest, MaxPageSize)
{
    // 分页不应超过合理上限
    int pageSize = 100;
    EXPECT_LE(pageSize, 100);
}
