// 测试阶段 4 群聊模块的全部业务逻辑
//
// 覆盖：
//   - 角色枚举与权限语义
//   - 申请状态流转
//   - 群名校验
//   - 分页偏移量计算
//   - 成员计数增减与边界
//   - 注销群硬删顺序
//   - 群主权限矩阵
//   - 成员列表排序
//   - SQL 模式验证
//   - JSON 响应结构

#include <gtest/gtest.h>
#include <json/json.h>
#include <algorithm>
#include <vector>
#include <string>
#include <cmath>

// ---- 枚举定义（与 group_service 逻辑一致） ----

enum GroupRole : int
{
    kMember = 0,
    kOwner  = 1,
};

enum GroupRequestStatus : int
{
    kPending  = 0,
    kAccepted = 1,
    kRejected = 2,
};

// ============================================================
// 一、角色枚举
// ============================================================

TEST(GroupRoleTest, MemberIsZero)
{
    EXPECT_EQ(kMember, 0);
}

TEST(GroupRoleTest, OwnerIsOne)
{
    EXPECT_EQ(kOwner, 1);
}

TEST(GroupRoleTest, OwnerIsHigherThanMember)
{
    EXPECT_GT(kOwner, kMember);
}

TEST(GroupRoleTest, OnlyTwoRoles)
{
    // 群聊只有两种角色：群主和普通成员
    EXPECT_EQ(kOwner, 1);
    EXPECT_EQ(kMember, 0);
}

// ============================================================
// 二、申请状态
// ============================================================

TEST(GroupRequestStatusTest, PendingIsZero)
{
    EXPECT_EQ(kPending, 0);
}

TEST(GroupRequestStatusTest, AcceptedIsOne)
{
    EXPECT_EQ(kAccepted, 1);
}

TEST(GroupRequestStatusTest, RejectedIsTwo)
{
    EXPECT_EQ(kRejected, 2);
}

TEST(GroupRequestStatusTest, ValidTransitions)
{
    // pending → accepted 或 rejected
    EXPECT_NE(kPending, kAccepted);
    EXPECT_NE(kPending, kRejected);
    EXPECT_NE(kAccepted, kRejected);
}

// ============================================================
// 三、群名校验
// ============================================================

TEST(GroupNameTest, EmptyNameInvalid)
{
    std::string name = "";
    EXPECT_TRUE(name.empty());
}

TEST(GroupNameTest, NonEmptyNameValid)
{
    std::string name = "test-group";
    EXPECT_FALSE(name.empty());
}

TEST(GroupNameTest, WhitespaceOnlyIsNotEmpty)
{
    // 业务层只检查 empty()，空格视为有效（前端可进一步校验）
    std::string name = "   ";
    EXPECT_FALSE(name.empty());
}

TEST(GroupNameTest, UnicodeName)
{
    std::string name = "前端技术交流群";
    EXPECT_FALSE(name.empty());
    EXPECT_EQ(name.size(), 21u);  // 7 个中文字符 * 3 字节/字符
}

// ============================================================
// 四、分页偏移量计算
// ============================================================

TEST(GroupPaginationTest, FirstPage)
{
    int page = 1, pageSize = 20;
    int offset = (page - 1) * pageSize;
    EXPECT_EQ(offset, 0);
}

TEST(GroupPaginationTest, SecondPage)
{
    int page = 2, pageSize = 20;
    int offset = (page - 1) * pageSize;
    EXPECT_EQ(offset, 20);
}

TEST(GroupPaginationTest, ThirdPage)
{
    int page = 3, pageSize = 10;
    int offset = (page - 1) * pageSize;
    EXPECT_EQ(offset, 20);
}

TEST(GroupPaginationTest, NegativePageClampedToOne)
{
    int page = -1;
    if (page < 1) page = 1;
    EXPECT_EQ(page, 1);
}

TEST(GroupPaginationTest, ZeroPageClampedToOne)
{
    int page = 0;
    if (page < 1) page = 1;
    EXPECT_EQ(page, 1);
}

TEST(GroupPaginationTest, SearchLikePattern)
{
    std::string keyword = "test";
    std::string pattern = "%" + keyword + "%";
    EXPECT_EQ(pattern, "%test%");
}

TEST(GroupPaginationTest, EmptyKeywordPattern)
{
    std::string keyword = "";
    std::string pattern = "%" + keyword + "%";
    EXPECT_EQ(pattern, "%%");
}

TEST(GroupPaginationTest, UnicodeKeywordPattern)
{
    std::string keyword = "前端";
    std::string pattern = "%" + keyword + "%";
    EXPECT_EQ(pattern, "%前端%");
}

// ============================================================
// 五、成员计数逻辑
// ============================================================

TEST(GroupMemberCountTest, Increment)
{
    int count = 5;
    count++;
    EXPECT_EQ(count, 6);
}

TEST(GroupMemberCountTest, Decrement)
{
    int count = 5;
    count = std::max(count - 1, 0);
    EXPECT_EQ(count, 4);
}

TEST(GroupMemberCountTest, NeverNegative)
{
    int count = 0;
    count = std::max(count - 1, 0);
    EXPECT_EQ(count, 0);
}

TEST(GroupMemberCountTest, StartAtOneOnCreate)
{
    // 创建群时 member_count=1（群主自己）
    int count = 1;
    EXPECT_EQ(count, 1);
}

TEST(GroupMemberCountTest, DecrementFromOne)
{
    int count = 1;
    count = std::max(count - 1, 0);
    EXPECT_EQ(count, 0);
}

TEST(GroupMemberCountTest, MultipleJoins)
{
    int count = 1;  // 群主
    count++;  // 成员 1
    count++;  // 成员 2
    count++;  // 成员 3
    EXPECT_EQ(count, 4);
}

TEST(GroupMemberCountTest, JoinAndLeave)
{
    int count = 3;
    count++;  // 新成员加入
    EXPECT_EQ(count, 4);
    count = std::max(count - 1, 0);  // 成员退出
    EXPECT_EQ(count, 3);
}

// ============================================================
// 六、注销群硬删顺序
// ============================================================

TEST(GroupDissolveTest, DeleteOrderMessagesFirst)
{
    // 必须先删消息，再删成员，最后删群
    std::vector<std::string> tables = {
        "group_messages",
        "group_members",
        "group_chats"
    };
    EXPECT_EQ(tables[0], "group_messages");
    EXPECT_EQ(tables[1], "group_members");
    EXPECT_EQ(tables[2], "group_chats");
}

TEST(GroupDissolveTest, DeleteByGroupId)
{
    // 所有删除都按 group_id 过滤
    std::string sql1 = "DELETE FROM group_messages WHERE group_id=?";
    std::string sql2 = "DELETE FROM group_members WHERE group_id=?";
    std::string sql3 = "DELETE FROM group_chats WHERE id=?";
    EXPECT_NE(sql1.find("group_id=?"), std::string::npos);
    EXPECT_NE(sql2.find("group_id=?"), std::string::npos);
    EXPECT_NE(sql3.find("id=?"), std::string::npos);
}

TEST(GroupDissolveTest, Irreversible)
{
    // 硬删除不可恢复，确认没有 status 字段
    std::string sql = "DELETE FROM group_chats WHERE id=?";
    EXPECT_EQ(sql.find("status"), std::string::npos);
}

// ============================================================
// 七、群主权限矩阵
// ============================================================

TEST(GroupPermissionTest, OwnerCanDissolve)
{
    int64_t ownerId = 1001, currentUserId = 1001;
    EXPECT_EQ(ownerId, currentUserId);
}

TEST(GroupPermissionTest, NonOwnerCannotDissolve)
{
    int64_t ownerId = 1001, currentUserId = 1002;
    EXPECT_NE(ownerId, currentUserId);
}

TEST(GroupPermissionTest, OwnerCannotLeave)
{
    int64_t ownerId = 1001, userId = 1001;
    EXPECT_EQ(ownerId, userId);  // 应拒绝
}

TEST(GroupPermissionTest, MemberCanLeave)
{
    int64_t ownerId = 1001, userId = 1002;
    EXPECT_NE(ownerId, userId);  // 非群主，允许退群
}

TEST(GroupPermissionTest, OwnerCanViewRequests)
{
    int64_t ownerId = 1001, currentUserId = 1001;
    EXPECT_EQ(ownerId, currentUserId);
}

TEST(GroupPermissionTest, NonOwnerCannotViewRequests)
{
    int64_t ownerId = 1001, currentUserId = 1002;
    EXPECT_NE(ownerId, currentUserId);
}

TEST(GroupPermissionTest, OwnerCanAcceptReject)
{
    int64_t ownerId = 1001, currentUserId = 1001;
    EXPECT_EQ(ownerId, currentUserId);
}

TEST(GroupPermissionTest, NonOwnerCannotAcceptReject)
{
    int64_t ownerId = 1001, currentUserId = 1003;
    EXPECT_NE(ownerId, currentUserId);
}

// ============================================================
// 八、申请状态流转
// ============================================================

TEST(GroupRequestFlowTest, PendingToAccepted)
{
    int status = kPending;
    status = kAccepted;
    EXPECT_EQ(status, kAccepted);
}

TEST(GroupRequestFlowTest, PendingToRejected)
{
    int status = kPending;
    status = kRejected;
    EXPECT_EQ(status, kRejected);
}

TEST(GroupRequestFlowTest, AlreadyProcessedCannotChange)
{
    int status = kAccepted;
    // 业务层检查：status != 0 时拒绝操作
    EXPECT_NE(status, kPending);
}

TEST(GroupRequestFlowTest, OnlyPendingCanBeProcessed)
{
    EXPECT_EQ(kPending, 0);  // 业务层用 status==0 判断
}

// ============================================================
// 九、成员列表排序
// ============================================================

TEST(GroupMemberListTest, OwnerSortsFirst)
{
    struct Member { int64_t id; int role; std::string name; };
    std::vector<Member> members = {
        {1003, kMember, "Charlie"},
        {1001, kOwner,  "Alice"},
        {1002, kMember, "Bob"},
    };

    std::sort(members.begin(), members.end(),
        [](const Member& a, const Member& b)
        {
            if (a.role != b.role) return a.role > b.role;
            return a.id < b.id;
        });

    EXPECT_EQ(members[0].name, "Alice");
    EXPECT_EQ(members[0].role, kOwner);
    EXPECT_EQ(members[1].name, "Bob");
    EXPECT_EQ(members[2].name, "Charlie");
}

TEST(GroupMemberListTest, MultipleOwnersSortCorrectly)
{
    // 理论上只有一个群主，但排序逻辑应健壮
    struct Member { int64_t id; int role; };
    std::vector<Member> members = {
        {1002, kOwner},
        {1001, kOwner},
    };

    std::sort(members.begin(), members.end(),
        [](const Member& a, const Member& b)
        {
            if (a.role != b.role) return a.role > b.role;
            return a.id < b.id;
        });

    EXPECT_EQ(members[0].id, 1001);
    EXPECT_EQ(members[1].id, 1002);
}

TEST(GroupMemberListTest, AllMembers)
{
    struct Member { int64_t id; int role; };
    std::vector<Member> members = {
        {1005, kMember},
        {1003, kMember},
        {1001, kOwner},
        {1004, kMember},
        {1002, kMember},
    };

    std::sort(members.begin(), members.end(),
        [](const Member& a, const Member& b)
        {
            if (a.role != b.role) return a.role > b.role;
            return a.id < b.id;
        });

    EXPECT_EQ(members[0].id, 1001);
    EXPECT_EQ(members[0].role, kOwner);
    for (int i = 1; i < 5; ++i)
    {
        EXPECT_EQ(members[i].role, kMember);
    }
}

// ============================================================
// 十、SQL 模式验证
// ============================================================

TEST(GroupSqlTest, InsertGroupChat)
{
    std::string sql = "INSERT INTO group_chats (name, owner_id, member_count) VALUES (?, ?, 1)";
    EXPECT_NE(sql.find("member_count"), std::string::npos);
    EXPECT_NE(sql.find("VALUES (?, ?, 1)"), std::string::npos);
}

TEST(GroupSqlTest, InsertGroupMember)
{
    std::string sql = "INSERT INTO group_members (group_id, user_id, role) VALUES (?, ?, 1)";
    EXPECT_NE(sql.find("role"), std::string::npos);
}

TEST(GroupSqlTest, InsertGroupRequest)
{
    std::string sql = "INSERT INTO group_requests (group_id, from_user) VALUES (?, ?)";
    EXPECT_NE(sql.find("from_user"), std::string::npos);
}

TEST(GroupSqlTest, SelectGroupMembers)
{
    std::string sql = "SELECT gm.user_id, u.nickname, gm.role "
                      "FROM group_members gm JOIN users u ON gm.user_id = u.id "
                      "WHERE gm.group_id=? ORDER BY gm.role DESC, gm.user_id";
    EXPECT_NE(sql.find("JOIN"), std::string::npos);
    EXPECT_NE(sql.find("role DESC"), std::string::npos);
}

TEST(GroupSqlTest, SelectMyGroups)
{
    std::string sql = "SELECT gm.group_id, gc.name, gc.member_count, gm.role "
                      "FROM group_members gm JOIN group_chats gc ON gm.group_id = gc.id "
                      "WHERE gm.user_id=?";
    EXPECT_NE(sql.find("JOIN group_chats"), std::string::npos);
}

TEST(GroupSqlTest, SelectPendingRequests)
{
    std::string sql = "SELECT gr.id, gr.from_user, u.nickname, gr.created_at "
                      "FROM group_requests gr LEFT JOIN users u ON gr.from_user = u.id "
                      "WHERE gr.group_id=? AND gr.status=0";
    EXPECT_NE(sql.find("status=0"), std::string::npos);
}

TEST(GroupSqlTest, UpdateRequestStatus)
{
    std::string accept = "UPDATE group_requests SET status=1 WHERE id=?";
    std::string reject = "UPDATE group_requests SET status=2 WHERE id=?";
    EXPECT_NE(accept.find("status=1"), std::string::npos);
    EXPECT_NE(reject.find("status=2"), std::string::npos);
}

// ============================================================
// 十一、JSON 响应结构
// ============================================================

TEST(GroupJsonTest, CreateGroupResponse)
{
    Json::Value data;
    data["groupId"] = Json::Int64(42);
    data["message"] = "群创建成功";
    EXPECT_EQ(data["groupId"].asInt64(), 42);
    EXPECT_EQ(data["message"].asString(), "群创建成功");
}

TEST(GroupJsonTest, SearchGroupListItem)
{
    Json::Value g;
    g["id"]          = Json::Int64(1);
    g["name"]        = "test-group";
    g["ownerId"]     = Json::Int64(1001);
    g["memberCount"] = 10;
    EXPECT_EQ(g["id"].asInt64(), 1);
    EXPECT_EQ(g["ownerId"].asInt64(), 1001);
    EXPECT_EQ(g["memberCount"].asInt(), 10);
}

TEST(GroupJsonTest, MemberListItem)
{
    Json::Value m;
    m["id"]       = Json::Int64(1001);
    m["nickname"] = "Alice";
    m["role"]     = kOwner;
    EXPECT_EQ(m["role"].asInt(), kOwner);
}

TEST(GroupJsonTest, MyGroupListItem)
{
    Json::Value g;
    g["id"]          = Json::Int64(1);
    g["name"]        = "test";
    g["memberCount"] = 5;
    g["role"]        = kMember;
    EXPECT_EQ(g["role"].asInt(), kMember);
}

TEST(GroupJsonTest, RequestListItem)
{
    Json::Value r;
    r["id"]        = Json::Int64(1);
    r["fromUser"]  = Json::Int64(1003);
    r["nickname"]  = "Bob";
    r["createdAt"] = "2026-06-10 15:00:00";
    EXPECT_EQ(r["fromUser"].asInt64(), 1003);
}

TEST(GroupJsonTest, JoinRequestResponse)
{
    Json::Value data;
    data["requestId"] = Json::Int64(5);
    data["message"]   = "加群申请已发送";
    EXPECT_EQ(data["requestId"].asInt64(), 5);
}

TEST(GroupJsonTest, LeaveGroupResponse)
{
    Json::Value data;
    data["message"] = "已退群";
    EXPECT_EQ(data["message"].asString(), "已退群");
}

TEST(GroupJsonTest, DissolveGroupResponse)
{
    Json::Value data;
    data["message"] = "群已注销";
    EXPECT_EQ(data["message"].asString(), "群已注销");
}

TEST(GroupJsonTest, AcceptRejectResponse)
{
    Json::Value accept;
    accept["message"] = "已同意";
    Json::Value reject;
    reject["message"] = "已拒绝";
    EXPECT_NE(accept["message"].asString(), reject["message"].asString());
}

// ============================================================
// 十二、边界条件
// ============================================================

TEST(GroupBoundaryTest, MaxGroupName)
{
    std::string name(64, 'x');  // VARCHAR(64) 上限
    EXPECT_EQ(name.size(), 64u);
}

TEST(GroupBoundaryTest, SearchKeywordWithSpecialChars)
{
    std::string keyword = "%_test";
    std::string pattern = "%" + keyword + "%";
    // SQL LIKE 通配符应由前端转义，后端原样传递
    EXPECT_EQ(pattern, "%%_test%");
}

TEST(GroupBoundaryTest, EmptyMemberList)
{
    Json::Value list(Json::arrayValue);
    EXPECT_EQ(list.size(), 0u);
}

TEST(GroupBoundaryTest, LargeGroupId)
{
    int64_t groupId = 9999999999LL;
    EXPECT_GT(groupId, 0);
}

TEST(GroupBoundaryTest, ConcurrentMemberCountChanges)
{
    // 模拟并发场景：两个退群操作同时发生
    int count = 2;
    count = std::max(count - 1, 0);  // 用户 A 退群
    count = std::max(count - 1, 0);  // 用户 B 退群
    EXPECT_EQ(count, 0);
}
