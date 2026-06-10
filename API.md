# API.md — online-chat 接口文档

> 版本：v1.4（对应 SPEC.md 阶段 0~3）
> 基础地址：`http://localhost:8080`
> 所有 HTTP 接口返回格式：`{ "code": 0, "msg": "success", "data": {...} }`
> `code=0` 表示成功，非 0 为错误码（见 SPEC.md 错误码定义）

---

## 目录

- [一、认证模块（无需鉴权）](#一认证模块无需鉴权)
- [二、用户模块（需 JwtFilter）](#二用户模块需-jwtfilter)
- [三、好友模块（需 JwtFilter）](#三好友模块需-jwtfilter)
- [四、WebSocket 网关](#四websocket-网关)
- [五、错误码参考](#五错误码参考)

---

## 一、认证模块（无需鉴权）

### 1.1 发送邮箱验证码

**`POST /api/auth/send-code`**

向指定邮箱发送 6 位数字验证码，有效期 5 分钟，60 秒内不可重复发送。

**请求头**：`Content-Type: application/json`

**请求体**：

| 字段 | 类型 | 必填 | 说明 |
|---|---|---|---|
| email | string | ✅ | 注册邮箱 |

```json
{
  "email": "user@example.com"
}
```

**成功响应**（200）：

```json
{
  "code": 0,
  "msg": "success",
  "data": {
    "message": "验证码已发送"
  }
}
```

**失败响应**：

| code | HTTP Status | 说明 |
|---|---|---|
| 1001 | 400 | email 为空 |
| 2007 | 429 | 60 秒内重复发送 |

---

### 1.2 注册

**`POST /api/auth/register`**

校验验证码后创建账号，密码使用 bcrypt 哈希存储。

**请求头**：`Content-Type: application/json`

**请求体**：

| 字段 | 类型 | 必填 | 说明 |
|---|---|---|---|
| email | string | ✅ | 注册邮箱 |
| code | string | ✅ | 6 位验证码 |
| password | string | ✅ | 密码（≥6 位） |
| nickname | string | ❌ | 昵称，默认取邮箱前缀 |

```json
{
  "email": "user@example.com",
  "code": "123456",
  "password": "mypassword",
  "nickname": "Alice"
}
```

**成功响应**（200）：

```json
{
  "code": 0,
  "msg": "注册成功",
  "data": {
    "userId": 1002,
    "message": "注册成功"
  }
}
```

**失败响应**：

| code | HTTP Status | 说明 |
|---|---|---|
| 1001 | 400 | 缺少必填字段 / 密码不足 6 位 |
| 2001 | 400 | 验证码已过期或未发送 |
| 2002 | 400 | 验证码错误 |
| 2003 | 400 | 该邮箱已注册 |

---

### 1.3 登录

**`POST /api/auth/login`**

校验邮箱+密码，成功返回 JWT token。新登录会使旧 token 失效（单点登录）。

**请求头**：`Content-Type: application/json`

**请求体**：

| 字段 | 类型 | 必填 | 说明 |
|---|---|---|---|
| email | string | ✅ | 登录邮箱 |
| password | string | ✅ | 密码 |

```json
{
  "email": "user@example.com",
  "password": "mypassword"
}
```

**成功响应**（200）：

```json
{
  "code": 0,
  "msg": "登录成功",
  "data": {
    "token": "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9...",
    "userId": 1002,
    "nickname": "Alice",
    "role": 0
  }
}
```

**失败响应**：

| code | HTTP Status | 说明 |
|---|---|---|
| 1001 | 400 | 缺少 email 或 password |
| 2005 | 401 | 邮箱或密码错误 |
| 2006 | 403 | 账号已被禁用 |

---

## 二、用户模块（需 JwtFilter）

> 以下所有接口需在请求头携带 JWT：
> `Authorization: Bearer <token>`

### 2.1 获取当前用户资料

**`GET /api/user/profile`**

**请求头**：

```
Authorization: Bearer eyJhbGciOi...
```

**成功响应**（200）：

```json
{
  "code": 0,
  "msg": "success",
  "data": {
    "user": {
      "id": 1002,
      "email": "user@example.com",
      "nickname": "Alice",
      "role": 0,
      "createdAt": "2026-06-10 12:00:00"
    }
  }
}
```

**失败响应**：

| code | HTTP Status | 说明 |
|---|---|---|
| 1002 | 401 | token 无效/过期/session 失效 |
| 1004 | 404 | 用户不存在 |

---

### 2.2 获取指定用户资料

**`GET /api/user/profile/{id}`**

**路径参数**：

| 参数 | 类型 | 说明 |
|---|---|---|
| id | int64 | 目标用户 ID |

**请求头**：`Authorization: Bearer <token>`

**成功响应**（200）：同 2.1

---

### 2.3 按昵称搜索用户

**`GET /api/user/search?keyword={keyword}&page={page}`**

**Query 参数**：

| 参数 | 类型 | 必填 | 说明 |
|---|---|---|---|
| keyword | string | ✅ | 搜索关键词（模糊匹配昵称） |
| page | int | ❌ | 页码，默认 1 |

**请求头**：`Authorization: Bearer <token>`

**成功响应**（200）：

```json
{
  "code": 0,
  "msg": "success",
  "data": {
    "list": [
      { "id": 1003, "nickname": "Alice2", "role": 0 },
      { "id": 1005, "nickname": "Alicia", "role": 0 }
    ],
    "total": 2,
    "page": 1,
    "pageSize": 20
  }
}
```

---

## 三、好友模块（需 JwtFilter）

> 以下所有接口需在请求头携带 JWT：
> `Authorization: Bearer <token>`

### 3.1 发送加好友申请

**`POST /api/friend/request`**

**请求头**：

```
Authorization: Bearer <token>
Content-Type: application/json
```

**请求体**：

| 字段 | 类型 | 必填 | 说明 |
|---|---|---|---|
| userId | int64 | ✅ | 目标用户 ID |
| message | string | ❌ | 验证留言 |

```json
{
  "userId": 1003,
  "message": "我是 Alice，加个好友"
}
```

**成功响应**（200）：

```json
{
  "code": 0,
  "msg": "success",
  "data": {
    "requestId": 1,
    "message": "申请已发送"
  }
}
```

**失败响应**：

| code | HTTP Status | 说明 |
|---|---|---|
| 1001 | 400 | 缺少 userId |
| 3004 | 400 | 不能加自己为好友 |
| 3001 | 400 | 已是好友 |
| 3002 | 400 | 已发送过申请（待处理） |

---

### 3.2 获取待处理的好友申请列表

**`GET /api/friend/requests`**

返回我收到的、状态为「待处理」的申请列表。

**请求头**：`Authorization: Bearer <token>`

**成功响应**（200）：

```json
{
  "code": 0,
  "msg": "success",
  "data": {
    "list": [
      {
        "id": 1,
        "fromUser": 1003,
        "nickname": "Bob",
        "message": "加个好友",
        "createdAt": "2026-06-10 14:00:00"
      }
    ],
    "total": 1
  }
}
```

---

### 3.3 同意好友申请

**`POST /api/friend/accept/{id}`**

**路径参数**：

| 参数 | 类型 | 说明 |
|---|---|---|
| id | int64 | 申请 ID |

**请求头**：`Authorization: Bearer <token>`

**成功响应**（200）：

```json
{
  "code": 0,
  "msg": "success",
  "data": {
    "message": "已同意"
  }
}
```

**失败响应**：

| code | HTTP Status | 说明 |
|---|---|---|
| 1004 | 404 | 申请不存在 |
| 1003 | 403 | 无权处理此申请 |
| 1001 | 400 | 该申请已处理 |

---

### 3.4 拒绝好友申请

**`POST /api/friend/reject/{id}`**

参数与响应格式同 3.3，成功时 `data.message` 为 `"已拒绝"`。

---

### 3.5 获取好友列表

**`GET /api/friend/list`**

返回当前用户的所有好友，包含在线状态（基于 Redis 实时查询）。

**请求头**：`Authorization: Bearer <token>`

**成功响应**（200）：

```json
{
  "code": 0,
  "msg": "success",
  "data": {
    "list": [
      { "id": 1003, "nickname": "Bob", "role": 0, "online": true },
      { "id": 1005, "nickname": "Carol", "role": 0, "online": false }
    ]
  }
}
```

---

### 3.6 删除好友

**`DELETE /api/friend/{id}`**

双向解除好友关系，历史消息保留。重新添加好友后可继续聊天。

**路径参数**：

| 参数 | 类型 | 说明 |
|---|---|---|
| id | int64 | 要删除的好友用户 ID |

**请求头**：`Authorization: Bearer <token>`

**成功响应**（200）：

```json
{
  "code": 0,
  "msg": "success",
  "data": {
    "message": "已删除好友"
  }
}
```

**失败响应**：

| code | HTTP Status | 说明 |
|---|---|---|
| 1001 | 400 | 不能删除自己 |
| 3003 | 404 | 不是好友关系 |

---

## 四、群聊模块（需 JwtFilter）

> 以下所有接口需在请求头携带 JWT：
> `Authorization: Bearer <token>`

### 4.1 创建群聊

**`POST /api/group/create`**

**请求头**：

```
Authorization: Bearer <token>
Content-Type: application/json
```

**请求体**：

| 字段 | 类型 | 必填 | 说明 |
|---|---|---|---|
| name | string | ✅ | 群名称 |

```json
{
  "name": "前端技术交流群"
}
```

**成功响应**（200）：

```json
{
  "code": 0,
  "msg": "success",
  "data": {
    "groupId": 1,
    "message": "群创建成功"
  }
}
```

---

### 4.2 按群名搜索群

**`GET /api/group/search?keyword={keyword}&page={page}`**

**Query 参数**：

| 参数 | 类型 | 必填 | 说明 |
|---|---|---|---|
| keyword | string | ✅ | 搜索关键词（模糊匹配群名） |
| page | int | ❌ | 页码，默认 1 |

**请求头**：`Authorization: Bearer <token>`

**成功响应**（200）：

```json
{
  "code": 0,
  "msg": "success",
  "data": {
    "list": [
      { "id": 1, "name": "前端技术交流群", "ownerId": 1001, "memberCount": 15 },
      { "id": 3, "name": "前端面试群", "ownerId": 1005, "memberCount": 8 }
    ],
    "total": 2,
    "page": 1
  }
}
```

---

### 4.3 发送加群申请

**`POST /api/group/join/{groupId}`**

**路径参数**：

| 参数 | 类型 | 说明 |
|---|---|---|
| groupId | int64 | 群 ID |

**请求头**：`Authorization: Bearer <token>`

**成功响应**（200）：

```json
{
  "code": 0,
  "msg": "success",
  "data": {
    "requestId": 1,
    "message": "加群申请已发送"
  }
}
```

**失败响应**：

| code | HTTP Status | 说明 |
|---|---|---|
| 4001 | 404 | 群不存在 |
| 4002 | 400 | 已在群中 |
| 4005 | 400 | 已发送过申请 |

---

### 4.4 获取群的待处理申请（群主）

**`GET /api/group/requests/{groupId}`**

**路径参数**：

| 参数 | 类型 | 说明 |
|---|---|---|
| groupId | int64 | 群 ID |

**请求头**：`Authorization: Bearer <token>`

**成功响应**（200）：

```json
{
  "code": 0,
  "msg": "success",
  "data": {
    "list": [
      { "id": 1, "fromUser": 1003, "nickname": "Bob", "createdAt": "2026-06-10 15:00:00" }
    ]
  }
}
```

**失败响应**：

| code | HTTP Status | 说明 |
|---|---|---|
| 4001 | 404 | 群不存在 |
| 4004 | 403 | 只有群主可以查看 |

---

### 4.5 同意加群申请（群主）

**`POST /api/group/accept/{requestId}`**

**路径参数**：

| 参数 | 类型 | 说明 |
|---|---|---|
| requestId | int64 | 申请 ID |

**请求头**：`Authorization: Bearer <token>`

**成功响应**（200）：

```json
{
  "code": 0,
  "msg": "success",
  "data": {
    "message": "已同意"
  }
}
```

**失败响应**：

| code | HTTP Status | 说明 |
|---|---|---|
| 1004 | 404 | 申请不存在 |
| 1001 | 400 | 该申请已处理 |
| 4004 | 403 | 不是群主 |

---

### 4.6 拒绝加群申请（群主）

**`POST /api/group/reject/{requestId}`**

参数与响应格式同 4.5，成功时 `data.message` 为 `"已拒绝"`。

---

### 4.7 退群

**`POST /api/group/leave/{groupId}`**

**路径参数**：

| 参数 | 类型 | 说明 |
|---|---|---|
| groupId | int64 | 群 ID |

**请求头**：`Authorization: Bearer <token>`

**成功响应**（200）：

```json
{
  "code": 0,
  "msg": "success",
  "data": {
    "message": "已退群"
  }
}
```

**失败响应**：

| code | HTTP Status | 说明 |
|---|---|---|
| 4001 | 404 | 群不存在 |
| 4003 | 400 | 不在群中 |
| 1003 | 403 | 群主不能退群 |

---

### 4.8 群成员列表

**`GET /api/group/members/{groupId}`**

**路径参数**：

| 参数 | 类型 | 说明 |
|---|---|---|
| groupId | int64 | 群 ID |

**请求头**：`Authorization: Bearer <token>`

**成功响应**（200）：

```json
{
  "code": 0,
  "msg": "success",
  "data": {
    "list": [
      { "id": 1001, "nickname": "Alice", "role": 1 },
      { "id": 1002, "nickname": "Bob", "role": 0 }
    ],
    "total": 2
  }
}
```

> `role=1` 为群主，`role=0` 为普通成员。列表按 role DESC 排序，群主在前。

---

### 4.9 注销群（群主）

**`DELETE /api/group/{groupId}`**

硬删除群、成员关系、群消息记录。不可恢复。

**路径参数**：

| 参数 | 类型 | 说明 |
|---|---|---|
| groupId | int64 | 群 ID |

**请求头**：`Authorization: Bearer <token>`

**成功响应**（200）：

```json
{
  "code": 0,
  "msg": "success",
  "data": {
    "message": "群已注销"
  }
}
```

**失败响应**：

| code | HTTP Status | 说明 |
|---|---|---|
| 4001 | 404 | 群不存在 |
| 4004 | 403 | 只有群主可以注销 |

---

### 4.10 我加入的群列表

**`GET /api/group/my`**

**请求头**：`Authorization: Bearer <token>`

**成功响应**（200）：

```json
{
  "code": 0,
  "msg": "success",
  "data": {
    "list": [
      { "id": 1, "name": "前端技术交流群", "memberCount": 15, "role": 1 },
      { "id": 2, "name": "项目讨论组", "memberCount": 5, "role": 0 }
    ]
  }
}
```

---

## 五、WebSocket 网关

### 4.1 连接

**`WS /ws?token={JWT}`**

通过 URL query 参数携带 JWT 建立 WebSocket 连接。连接建立后自动注册在线状态。

**连接参数**：

| 参数 | 类型 | 必填 | 说明 |
|---|---|---|---|
| token | string | ✅ | 登录接口返回的 JWT |

**连接成功后服务端推送**：

```json
{
  "type": "ack",
  "seq": 0,
  "data": {
    "message": "connected",
    "userId": 1002
  }
}
```

**连接失败**：token 无效或缺失时，服务端直接关闭连接（CloseCode: Violation）。

---

### 4.2 消息格式（统一信封）

所有 WebSocket 消息使用统一 JSON 信封格式：

```json
{
  "type": "消息类型",
  "seq": 12345,
  "data": { ... }
}
```

| 字段 | 类型 | 说明 |
|---|---|---|
| type | string | 消息类型（见下表） |
| seq | int64 | 序列号，用于请求-响应关联 |
| data | object | 消息数据，按 type 不同而异 |

---

### 4.3 消息类型

#### 客户端 → 服务端

| type | 说明 | data 字段 | 备注 |
|---|---|---|---|
| `ping` | 心跳 | `{}` | 服务端回复 `pong` |
| `chat_single` | 单聊消息 | `{ "to": userId, "content": "..." }` | 阶段 5 实现 |
| `chat_group` | 群聊消息 | `{ "groupId": id, "content": "..." }` | 阶段 5 实现 |

#### 服务端 → 客户端

| type | 说明 | data 字段 | 触发条件 |
|---|---|---|---|
| `ack` | 确认/欢迎 | `{ "message": "...", ... }` | 连接成功 / 消息已落库 |
| `pong` | 心跳回复 | `{}` | 收到 `ping` |
| `error` | 错误通知 | `{ "message": "..." }` | 格式错误 / 未知类型 |
| `kicked` | 被挤下线 | `{ "message": "..." }` | 同账号新登录 |
| `chat_single` | 收到单聊 | `{ "from": userId, "content": "...", "seq": ... }` | 阶段 5 实现 |
| `chat_group` | 收到群聊 | `{ "groupId": id, "from": userId, "content": "...", "seq": ... }` | 阶段 5 实现 |
| `notify_apply` | 收到申请 | 阶段 5 实现 | 加好友/加群申请 |
| `notify_apply_result` | 申请结果 | 阶段 5 实现 | 自己的申请被处理 |
| `presence` | 好友上下线 | `{ "userId": id, "online": true/false }` | 阶段 5 实现 |

---

### 4.4 心跳机制

- 服务端每 **30 秒**自动发送 ping 帧（Drogon 内置）
- 客户端收到 ping 后 Drogon 自动回复 pong
- 客户端也可主动发送 `{"type":"ping","seq":0,"data":{}}`，服务端回复 `pong`
- **60 秒**无心跳，Redis 在线状态自动过期

---

## 六、消息模块（需 JwtFilter）

> 以下所有接口需在请求头携带 JWT：
> `Authorization: Bearer <token>`

### 6.1 单聊历史消息

**`GET /api/message/single/history?peerId={peerId}&beforeSeq={beforeSeq}&limit={limit}`**

按游标分页加载单聊历史消息（最新消息在前）。

**Query 参数**：

| 参数 | 类型 | 必填 | 说明 |
|---|---|---|---|
| peerId | int64 | ✅ | 对方用户 ID |
| beforeSeq | int64 | ❌ | 游标序号，加载此 seq 之前的消息。首次传 0 或不传 |
| limit | int | ❌ | 每页条数，默认 30，最大 100 |

**请求头**：`Authorization: Bearer <token>`

**成功响应**（200）：

```json
{
  "code": 0,
  "msg": "success",
  "data": {
    "list": [
      {
        "id": 100,
        "from": 1001,
        "to": 1002,
        "content": "你好",
        "seq": 5,
        "createdAt": "2026-06-10 12:00:00"
      }
    ],
    "hasMore": true
  }
}
```

> `hasMore=true` 表示还有更早的消息，下次请求传 `beforeSeq=5` 继续加载。

---

### 6.2 群聊历史消息

**`GET /api/message/group/history?groupId={groupId}&beforeSeq={beforeSeq}&limit={limit}`**

按游标分页加载群聊历史消息。

**Query 参数**：

| 参数 | 类型 | 必填 | 说明 |
|---|---|---|---|
| groupId | int64 | ✅ | 群 ID |
| beforeSeq | int64 | ❌ | 游标序号。首次传 0 或不传 |
| limit | int | ❌ | 每页条数，默认 30，最大 100 |

**请求头**：`Authorization: Bearer <token>`

**成功响应**（200）：

```json
{
  "code": 0,
  "msg": "success",
  "data": {
    "list": [
      {
        "id": 200,
        "groupId": 42,
        "from": 1001,
        "content": "大家好",
        "seq": 3,
        "createdAt": "2026-06-10 12:00:00"
      }
    ],
    "hasMore": false
  }
}
```

---

### 6.3 拉取离线消息

**`GET /api/message/offline`**

用户上线后调用，拉取所有离线期间未投递的消息。拉取后自动删除离线索引。

**请求头**：`Authorization: Bearer <token>`

**成功响应**（200）：

```json
{
  "code": 0,
  "msg": "success",
  "data": {
    "messages": [
      {
        "type": "chat_single",
        "id": 100,
        "from": 1002,
        "content": "你不在的时候我发的消息",
        "seq": 5
      },
      {
        "type": "chat_group",
        "id": 200,
        "groupId": 42,
        "from": 1003,
        "content": "群消息",
        "seq": 3
      }
    ]
  }
}
```

---

### 6.4 WebSocket 消息收发

消息通过 WebSocket 实时收发，详见 [四、WebSocket 网关](#四websocket-网关)。

**发送单聊消息**（客户端 → 服务端）：

```json
{
  "type": "chat_single",
  "seq": 1,
  "data": {
    "to": 1002,
    "content": "你好"
  }
}
```

**服务端确认**（ack）：

```json
{
  "type": "ack",
  "seq": 1,
  "data": {
    "id": 100,
    "from": 1001,
    "to": 1002,
    "content": "你好",
    "seq": 5
  }
}
```

**收到单聊消息**（服务端 → 接收方）：

```json
{
  "type": "chat_single",
  "seq": 5,
  "data": {
    "id": 100,
    "from": 1001,
    "to": 1002,
    "content": "你好",
    "seq": 5
  }
}
```

**发送群聊消息**（客户端 → 服务端）：

```json
{
  "type": "chat_group",
  "seq": 2,
  "data": {
    "groupId": 42,
    "content": "大家好"
  }
}
```

**收到群聊消息**（服务端 → 群成员）：

```json
{
  "type": "chat_group",
  "seq": 3,
  "data": {
    "id": 200,
    "groupId": 42,
    "from": 1001,
    "content": "大家好",
    "seq": 3
  }
}
```

---

## 七、错误码参考

| code | 名称 | HTTP Status | 说明 |
|---|---|---|---|
| 0 | OK | 200 | 成功 |
| 1001 | INVALID_PARAMS | 400 | 请求参数校验失败 |
| 1002 | UNAUTHORIZED | 401 | 未登录或 token 无效/过期 |
| 1003 | FORBIDDEN | 403 | 权限不足 |
| 1004 | NOT_FOUND | 404 | 资源不存在 |
| 1005 | INTERNAL_ERROR | 500 | 服务端内部错误 |
| 1006 | TOO_MANY_REQUESTS | 429 | 频率限制 |
| 2001 | VERIFY_CODE_EXPIRED | 400 | 验证码已过期 |
| 2002 | VERIFY_CODE_WRONG | 400 | 验证码错误 |
| 2003 | EMAIL_REGISTERED | 400 | 邮箱已注册 |
| 2004 | EMAIL_NOT_VERIFIED | 400 | 邮箱未验证 |
| 2005 | PASSWORD_WRONG | 401 | 密码错误 |
| 2006 | ACCOUNT_DISABLED | 403 | 账号被禁用 |
| 2007 | RESEND_TOO_FAST | 429 | 验证码重发过快 |
| 3001 | ALREADY_FRIENDS | 400 | 已是好友 |
| 3002 | FRIEND_REQUEST_SENT | 400 | 已发送过申请 |
| 3003 | FRIEND_NOT_FOUND | 404 | 好友关系不存在 |
| 3004 | CANNOT_ADD_SELF | 400 | 不能加自己为好友 |
| 4001 | GROUP_NOT_FOUND | 404 | 群不存在 |
| 4002 | ALREADY_IN_GROUP | 400 | 已在群中 |
| 4003 | NOT_IN_GROUP | 400 | 不在群中 |
| 4004 | NOT_GROUP_OWNER | 403 | 不是群主 |
| 4005 | GROUP_REQUEST_SENT | 400 | 已发送过加群申请 |
| 5001 | CONV_NOT_FOUND | 404 | 会话不存在 |

---

## 八、鉴权说明

### HTTP 接口鉴权

在请求头中携带 JWT：

```
Authorization: Bearer eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9...
```

JWT 有效期 **7 天**，过期后需重新登录。

### WebSocket 鉴权

在连接 URL 的 query 参数中携带 token：

```
ws://localhost:8080/ws?token=eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9...
```

### 单点登录

同一账号只能一处在线。新登录会使旧 token 立即失效：
- HTTP 接口：旧 token 请求返回 `code=1002`
- WebSocket：旧连接收到 `kicked` 消息后被关闭
