# 项目接口测试-curl
#
# 使用方法：
#   1. 启动服务：cd build && ./online_chat
#   2. 在另一个终端执行本文件中的 curl 命令
#   3. 按模块顺序执行（注册 → 登录 → 搜索 → 好友 → 群聊 → 消息 → 管理）
#
# 基础地址
BASE=http://localhost:8080

# ============================================================
# 一、认证模块（无需鉴权）
# ============================================================

# 1.1 发送邮箱验证码
curl -s $BASE/api/auth/send-code \
  -X POST \
  -H "Content-Type: application/json" \
  -d '{"email":"alice@example.com"}' | python3 -m json.tool

# 1.2 注册（先发送验证码，再用返回的验证码注册）
# 注意：验证码会发送到邮箱，本地开发时需要查看 SMTP 日志或 Redis 中的值
# 可以用 redis-cli 查看：redis-cli GET verifycode:alice@example.com
curl -s $BASE/api/auth/register \
  -X POST \
  -H "Content-Type: application/json" \
  -d '{"email":"alice@example.com","code":"123456","password":"alice123","nickname":"Alice"}' | python3 -m json.tool

# 1.3 登录（获取 JWT token）
curl -s $BASE/api/auth/login \
  -X POST \
  -H "Content-Type: application/json" \
  -d '{"email":"alice@example.com","password":"alice123"}' | python3 -m json.tool

# 登录成功后，将返回的 token 保存到变量，后续接口使用
# 请将下面的 TOKEN 替换为实际登录返回的 token
TOKEN="eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9..."

# ============================================================
# 二、用户模块（需 JwtFilter）
# ============================================================

# 2.1 获取当前用户资料
curl -s $BASE/api/user/profile \
  -H "Authorization: Bearer $TOKEN" | python3 -m json.tool

# 2.2 获取指定用户资料（id=1）
curl -s $BASE/api/user/profile/1 \
  -H "Authorization: Bearer $TOKEN" | python3 -m json.tool

# 2.3 按昵称搜索用户
curl -s "$BASE/api/user/search?keyword=Alice&page=1" \
  -H "Authorization: Bearer $TOKEN" | python3 -m json.tool

# ============================================================
# 三、好友模块（需 JwtFilter）
# ============================================================

# 先注册第二个用户 Bob（用于好友测试）
curl -s $BASE/api/auth/register \
  -X POST \
  -H "Content-Type: application/json" \
  -d '{"email":"bob@example.com","code":"654321","password":"bob123456","nickname":"Bob"}' | python3 -m json.tool

# Bob 登录
curl -s $BASE/api/auth/login \
  -X POST \
  -H "Content-Type: application/json" \
  -d '{"email":"bob@example.com","password":"bob123456"}' | python3 -m json.tool

# 请将下面的 TOKEN_BOB 替换为 Bob 登录返回的 token
TOKEN_BOB="eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9..."

# 3.1 Alice 向 Bob 发送好友申请
curl -s $BASE/api/friend/request \
  -X POST \
  -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{"userId":2,"message":"我是Alice，加个好友"}' | python3 -m json.tool

# 3.2 Bob 查看待处理的好友申请
curl -s $BASE/api/friend/requests \
  -H "Authorization: Bearer $TOKEN_BOB" | python3 -m json.tool

# 3.3 Bob 同意好友申请（假设申请 id=1）
curl -s $BASE/api/friend/accept/1 \
  -X POST \
  -H "Authorization: Bearer $TOKEN_BOB" | python3 -m json.tool

# 3.4 Alice 查看好友列表（含在线状态）
curl -s $BASE/api/friend/list \
  -H "Authorization: Bearer $TOKEN" | python3 -m json.tool

# 3.5 删除好友（Alice 删除 Bob，假设 Bob 的 userId=2）
curl -s $BASE/api/friend/2 \
  -X DELETE \
  -H "Authorization: Bearer $TOKEN" | python3 -m json.tool

# ============================================================
# 四、群聊模块（需 JwtFilter）
# ============================================================

# 4.1 创建群聊
curl -s $BASE/api/group/create \
  -X POST \
  -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{"name":"前端技术交流群"}' | python3 -m json.tool

# 4.2 按群名搜索群
curl -s "$BASE/api/group/search?keyword=前端&page=1" \
  -H "Authorization: Bearer $TOKEN" | python3 -m json.tool

# 4.3 Bob 申请加入群（假设群 id=1）
curl -s $BASE/api/group/join/1 \
  -X POST \
  -H "Authorization: Bearer $TOKEN_BOB" | python3 -m json.tool

# 4.4 Alice（群主）查看群的待处理申请
curl -s $BASE/api/group/requests/1 \
  -H "Authorization: Bearer $TOKEN" | python3 -m json.tool

# 4.5 Alice（群主）同意加群申请（假设申请 id=1）
curl -s $BASE/api/group/accept/1 \
  -X POST \
  -H "Authorization: Bearer $TOKEN" | python3 -m json.tool

# 4.6 查看群成员列表
curl -s $BASE/api/group/members/1 \
  -H "Authorization: Bearer $TOKEN" | python3 -m json.tool

# 4.7 Bob 退群（假设群 id=1）
curl -s $BASE/api/group/leave/1 \
  -X POST \
  -H "Authorization: Bearer $TOKEN_BOB" | python3 -m json.tool

# 4.8 查看我加入的群列表
curl -s $BASE/api/group/my \
  -H "Authorization: Bearer $TOKEN" | python3 -m json.tool

# 4.9 注销群（群主操作，假设群 id=1）
# 注意：注销后群、成员、消息全部删除，不可恢复
curl -s $BASE/api/group/1 \
  -X DELETE \
  -H "Authorization: Bearer $TOKEN" | python3 -m json.tool

# ============================================================
# 五、消息模块（需 JwtFilter）
# ============================================================

# WebSocket 消息收发需要使用 websocat 或浏览器，curl 无法测试
# 以下为 HTTP 接口测试

# 5.1 单聊历史消息（假设对方 userId=2）
curl -s "$BASE/api/message/single/history?peerId=2&beforeSeq=0&limit=30" \
  -H "Authorization: Bearer $TOKEN" | python3 -m json.tool

# 5.2 群聊历史消息（假设群 id=1）
curl -s "$BASE/api/message/group/history?groupId=1&beforeSeq=0&limit=30" \
  -H "Authorization: Bearer $TOKEN" | python3 -m json.tool

# 5.3 拉取离线消息
curl -s $BASE/api/message/offline \
  -H "Authorization: Bearer $TOKEN" | python3 -m json.tool

# ============================================================
# 六、管理模块（需超管 JwtFilter）
# ============================================================

# 使用预置超管账号登录
# 邮箱: admin@online-chat.local  密码: admin123
curl -s $BASE/api/auth/login \
  -X POST \
  -H "Content-Type: application/json" \
  -d '{"email":"admin@online-chat.local","password":"admin123"}' | python3 -m json.tool

# 请将下面的 TOKEN_ADMIN 替换为超管登录返回的 token
TOKEN_ADMIN="eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9..."

# 6.1 超管查看全部群列表
curl -s "$BASE/api/admin/groups?page=1" \
  -H "Authorization: Bearer $TOKEN_ADMIN" | python3 -m json.tool

# 6.2 超管注销任意群（假设群 id=2）
curl -s $BASE/api/admin/groups/2 \
  -X DELETE \
  -H "Authorization: Bearer $TOKEN_ADMIN" | python3 -m json.tool

# ============================================================
# 七、WebSocket 测试（需要 websocat 工具）
# ============================================================

# 安装 websocat：cargo install websocat
# 或下载二进制：https://github.com/nickel-org/websocat/releases

# 连接 WebSocket（替换 TOKEN）
# websocat "ws://localhost:8080/ws?token=$TOKEN"

# 连接成功后，发送心跳：
# {"type":"ping","seq":0,"data":{}}

# 发送单聊消息（to=接收方 userId）：
# {"type":"chat_single","seq":1,"data":{"to":2,"content":"你好Bob"}}

# 发送群聊消息（groupId=群 id）：
# {"type":"chat_group","seq":2,"data":{"groupId":1,"content":"大家好"}}

# ============================================================
# 八、快速完整测试流程（一键脚本）
# ============================================================

# 以下脚本自动完成：注册两个用户 → 登录 → 加好友 → 建群 → 加群 → 发消息
# 注意：需要先手动发送验证码或直接往 Redis 写入验证码

# --- 快速测试脚本开始 ---
# # 注册 Alice
# curl -s $BASE/api/auth/register -X POST -H "Content-Type: application/json" \
#   -d '{"email":"alice@test.com","code":"111111","password":"alice123","nickname":"Alice"}'
#
# # 注册 Bob
# curl -s $BASE/api/auth/register -X POST -H "Content-Type: application/json" \
#   -d '{"email":"bob@test.com","code":"222222","password":"bob123456","nickname":"Bob"}'
#
# # Alice 登录
# TOKEN=$(curl -s $BASE/api/auth/login -X POST -H "Content-Type: application/json" \
#   -d '{"email":"alice@test.com","password":"alice123"}' | python3 -c "import sys,json;print(json.load(sys.stdin)['data']['token'])")
# echo "Alice token: $TOKEN"
#
# # Bob 登录
# TOKEN_BOB=$(curl -s $BASE/api/auth/login -X POST -H "Content-Type: application/json" \
#   -d '{"email":"bob@test.com","password":"bob123456"}' | python3 -c "import sys,json;print(json.load(sys.stdin)['data']['token'])")
# echo "Bob token: $TOKEN_BOB"
#
# # Alice 加 Bob 好友
# curl -s $BASE/api/friend/request -X POST -H "Authorization: Bearer $TOKEN" \
#   -H "Content-Type: application/json" -d '{"userId":2}'
#
# # Bob 同意
# curl -s $BASE/api/friend/accept/1 -X POST -H "Authorization: Bearer $TOKEN_BOB"
#
# # Alice 建群
# curl -s $BASE/api/group/create -X POST -H "Authorization: Bearer $TOKEN" \
#   -H "Content-Type: application/json" -d '{"name":"测试群"}'
#
# # Bob 加群
# curl -s $BASE/api/group/join/1 -X POST -H "Authorization: Bearer $TOKEN_BOB"
#
# # Alice 审批
# curl -s $BASE/api/group/accept/1 -X POST -H "Authorization: Bearer $TOKEN"
#
# echo "=== 完成 ==="
# --- 快速测试脚本结束 ---
