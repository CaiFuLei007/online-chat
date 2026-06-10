# SPEC.md — 网页版聊天工具（类微信）技术规格说明书

> 版本：v1.4
> 日期：2026-06-10
> 状态：阶段 0~3 已完成，下一阶段：群聊模块
> 后端语言：C++（Drogon 框架） | 前端：原生 HTML/CSS/JS | 存储：MySQL + Redis

---

## 一、项目整体需求

### 1.1 项目定位
小型生产级 Web 聊天工具，单机或主备部署，目标支持数百到数千并发在线用户。核心追求：功能完整、代码逻辑清晰、易于维护，不为高并发过度设计。

### 1.2 技术基线（已确认）

| 维度 | 结论 |
|---|---|
| 后端框架 | C++ / Drogon（自带 HTTP + WebSocket + ORM + 线程池） |
| 关系存储 | MySQL 8.x |
| 缓存/状态 | Redis 7.x（在线表、会话/挤下线、验证码、消息序号） |
| 前端 | 原生 HTML/CSS/JS + 原生 WebSocket API |
| 认证 | 邮箱验证码激活注册（SMTP）+ JWT 鉴权（配合 Redis 做单点登录） |
| 部署 | CMake 构建 + Docker（含 MySQL/Redis）+ 建表 SQL + API/WS 协议文档 |

### 1.3 功能需求清单

**1) 注册与登录**
- 邮箱注册：填邮箱 → 服务端发送 6 位验证码（存 Redis，有效期 5 分钟，限频）→ 校验通过后设置密码 → 完成注册。
- 邮箱兼作登录账号：后续用「邮箱 + 密码」登录。
- 密码使用 bcrypt 加盐哈希存储，绝不明文。
- 登录成功签发 JWT（含 userId、role、exp=7 天）。

**2) 搜索与添加**
- 按「用户名称」（可重复昵称）模糊搜索用户；按「群名称」模糊搜索群。结果分页（默认每页 20）。
- 可对搜索到的用户发起「加好友申请」，对群发起「加群申请」。

**3) 群聊管理**
- 任意用户可创建群聊，创建者即群主。
- 群主：审批（同意/拒绝）加群申请、注销（解散）群聊。
- 成员：加入群、主动退群、在群内发消息（群内所有在群成员可见）。
- 不做踢人、不做转让群主、不做禁言。
- 注销群=硬删除：群、成员关系、群消息记录一并从库删除，不可恢复。

**4) 好友聊天**
- 互为好友后可一对一聊天，消息仅双方可见。
- 删除好友：双向解除（双方都不再是好友、互不可见、不能发消息），但历史消息保留在库；重新加为好友后可再次看到历史并继续聊天。

**5) 用户权限分类**
- 普通用户（role=normal）：仅能看到自己的好友与已加入的群，与好友/群成员聊天。
- 超级用户（role=admin，数据库预置）：在管理面板查看「全部群聊列表」（群名/群主/人数等元数据），可注销任意群；**不进入查看群内聊天内容**。

**6) 消息**
- 仅文本消息（含 emoji 字符），不支持图片/文件。
- 持久化 + 离线消息：所有消息入库；接收方离线时暂存，上线后拉取离线消息。
- 历史消息分页滚动加载（打开会话默认拉最近 N 条，向上滚动按游标拉更早）。
- **不做已读/未读机制**（不存回执、不显示已读状态）。

**7) 在线状态**
- Redis 维护在线表，好友列表展示在线/离线，状态变化实时推送给在线好友。

**8) 通知**
- 加好友/加群申请、审批结果：对方在线时 WebSocket 实时推送；离线时入库，上线后拉取未处理申请列表。

**9) 会话策略**
- 单点登录：一个账号同时只能一处在线，新登录挤掉旧连接（旧连接收到「被挤下线」通知并断开）。

---

## 二、项目依赖清单与安装方式

> 目标系统：Ubuntu 22.04 / 24.04 LTS（Docker 镜像与宿主机均适用）。
> 其他 Debian 系发行版包名基本一致；RHEL/CentOS 需替换为 `dnf` 对应包名。

### 2.1 构建依赖（编译期，builder 阶段安装）

| 依赖 | 最低版本 | 用途 | Ubuntu/Debian 包名 |
|---|---|---|---|
| CMake | ≥ 3.16 | 构建系统 | `cmake` |
| GCC / G++ | ≥ 11（C++17） | C++ 编译器 | `build-essential` |
| Git | 任意 | 拉取 Drogon 源码 | `git` |
| ca-certificates | — | HTTPS 证书（git clone / curl） | `ca-certificates` |
| jsoncpp 开发库 | ≥ 1.7 | JSON 解析（Drogon 依赖） | `libjsoncpp-dev` |
| UUID 开发库 | — | UUID 生成（Drogon 依赖） | `uuid-dev` |
| OpenSSL 开发库 | ≥ 1.1 | TLS / bcrypt（Drogon 依赖） | `libssl-dev` |
| zlib 开发库 | — | 压缩（Drogon 依赖） | `zlib1g-dev` |
| MySQL 客户端开发库 | ≥ 8.0 | Drogon ORM 连接 MySQL | `libmysqlclient-dev` |
| hiredis 开发库 | ≥ 1.0 | Drogon 连接 Redis | `libhiredis-dev` |
| curl | — | SMTP 发送验证码邮件 | `curl` |

**一键安装（Ubuntu/Debian）：**

```bash
sudo apt-get update && sudo apt-get install -y --no-install-recommends \
    build-essential cmake git ca-certificates \
    libjsoncpp-dev uuid-dev libssl-dev zlib1g-dev \
    libmysqlclient-dev libhiredis-dev curl
```

### 2.2 Drogon 框架（从源码编译安装）

| 依赖 | 版本 | 说明 |
|---|---|---|
| Drogon | v1.9.13 | C++ HTTP/WebSocket 框架，含 ORM、线程池、JSON、Redis 客户端 |
| Trantor | 随 Drogon 附带 | Drogon 的底层网络库（git submodule，无需单独安装） |

**安装方式（编译安装，Dockerfile 中已自动化）：**

```bash
git clone --depth 1 --branch v1.9.13 \
    https://github.com/drogonframework/drogon.git /tmp/drogon
cd /tmp/drogon
git submodule update --init --recursive
cmake -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_EXAMPLES=OFF -DBUILD_CTL=OFF
cmake --build build -j$(nproc)
sudo cmake --install build   # 安装到 /usr/local
rm -rf /tmp/drogon
```

> 安装后 `DrogonConfig.cmake` 位于 `/usr/local/lib/cmake/Drogon/`，
> 项目的 `find_package(Drogon CONFIG REQUIRED)` 即可找到。

### 2.3 运行时依赖（生产/运行阶段安装）

| 依赖 | 版本 | 用途 | Ubuntu/Debian 包名 |
|---|---|---|---|
| MySQL Server | 8.0 | 关系型数据存储 | `mysql-server`（或 Docker 镜像 `mysql:8.0`） |
| Redis Server | ≥ 7.0 | 缓存 / 在线表 / 验证码 / 消息序号 | `redis-server`（或 Docker 镜像 `redis:7-alpine`） |
| libjsoncpp | ≥ 1.7 | JSON 解析运行时 | `libjsoncpp25` |
| OpenSSL | ≥ 1.1 | TLS 运行时 | `libssl3` |
| zlib | — | 压缩运行时 | `zlib1g` |
| MySQL 客户端运行时 | ≥ 8.0 | MySQL 连接 | `libmysqlclient21` |
| hiredis 运行时 | ≥ 1.0 | Redis 连接 | `libhiredis0.14` |
| curl | — | SMTP 发送邮件 | `curl` |
| ca-certificates | — | HTTPS 证书验证 | `ca-certificates` |

**一键安装（Ubuntu/Debian 运行时）：**

```bash
sudo apt-get update && sudo apt-get install -y --no-install-recommends \
    mysql-server redis-server \
    libjsoncpp25 libssl3 zlib1g libmysqlclient21 libhiredis0.14 \
    curl ca-certificates
```

> ⚠️ 如果使用 Docker 部署（推荐），MySQL 和 Redis 由 `docker-compose.yml` 自动拉取镜像，
> 宿主机无需安装 MySQL/Redis，只需安装 Docker（见 2.5）。

### 2.4 测试依赖

| 依赖 | 版本 | 用途 | Ubuntu/Debian 包名 |
|---|---|---|---|
| Google Test | ≥ 1.12 | C++ 单元测试框架 | `libgtest-dev` |
| jsoncpp 开发库 | — | 测试中解析 JSON 响应 | `libjsoncpp-dev`（同构建依赖） |

**一键安装：**

```bash
sudo apt-get install -y --no-install-recommends libgtest-dev libjsoncpp-dev
```

> 注：`libgtest-dev` 在 Ubuntu 22.04+ 已包含预编译库，CMake 中
> `find_package(GTest REQUIRED)` 即可找到，无需手动编译 gtest。

### 2.5 Docker 部署依赖

| 依赖 | 最低版本 | 用途 |
|---|---|---|
| Docker Engine | ≥ 20.10 | 容器运行时 |
| Docker Compose | ≥ 2.0（V2 插件） | 多容器编排 |

**安装 Docker Engine + Compose（Ubuntu）：**

```bash
# 卸载旧版本（如有）
sudo apt-get remove -y docker docker-engine docker.io containerd runc 2>/dev/null

# 安装依赖
sudo apt-get update
sudo apt-get install -y ca-certificates curl gnupg

# 添加 Docker 官方 GPG 密钥
sudo install -m 0755 -d /etc/apt/keyrings
curl -fsSL https://download.docker.com/linux/ubuntu/gpg | \
    sudo gpg --dearmor -o /etc/apt/keyrings/docker.gpg
sudo chmod a+r /etc/apt/keyrings/docker.gpg

# 添加 Docker 源
echo "deb [arch=$(dpkg --print-architecture) signed-by=/etc/apt/keyrings/docker.gpg] \
https://download.docker.com/linux/ubuntu $(. /etc/os-release && echo "$VERSION_CODENAME") stable" | \
    sudo tee /etc/apt/sources.list.d/docker.list > /dev/null

# 安装
sudo apt-get update
sudo apt-get install -y docker-ce docker-ce-cli containerd.io docker-compose-plugin

# 验证
docker --version
docker compose version

# （可选）免 sudo 运行 docker
sudo usermod -aG docker $USER
# 重新登录后生效
```

### 2.6 依赖关系总览图

```
                        ┌──────────────────────────────────┐
                        │          Docker Engine            │
                        │     + Docker Compose V2           │
                        └───────────────┬──────────────────┘
                                        │
                 ┌──────────────────────┼──────────────────────┐
                 ▼                      ▼                      ▼
          ┌─────────────┐      ┌──────────────┐      ┌──────────────┐
          │  oc-mysql    │      │  oc-redis     │      │    oc-app     │
          │ mysql:8.0    │      │ redis:7-alpine│      │ (本项目镜像)  │
          └─────────────┘      └──────────────┘      └───────┬──────┘
                                                             │
                                                     ┌───────▼──────┐
                                                     │   Drogon      │
                                                     │  v1.9.13      │
                                                     │  (源码编译)    │
                                                     └───────┬──────┘
                                                             │
                              ┌──────────────┬───────────────┼───────────────┐
                              ▼              ▼               ▼               ▼
                         libjsoncpp      libssl3        libmysqlclient   libhiredis
                         zlib1g          curl           ca-certificates
```

### 2.7 版本锁定与升级策略

| 组件 | 当前锁定版本 | 升级注意事项 |
|---|---|---|
| Drogon | v1.9.13 | 关注 CHANGELOG，API 向后兼容性较好 |
| MySQL | 8.0 | 不跨大版本升级（8.4 LTS 语法有变化） |
| Redis | 7.x | 小版本直接升级，注意持久化格式兼容 |
| Dockerfile 基础镜像 | ubuntu:22.04 | 升级到 24.04 需验证库包名变化 |

> **原则**：锁定主版本号，小版本自动跟进；大版本升级前在 Docker 环境中充分测试。

---

## 三、后端架构与模块划分

### 3.1 总体架构图

```
                          ┌─────────────────────────────┐
                          │      Browser (原生前端)       │
                          │  HTML/CSS/JS + WebSocket      │
                          └───────────────┬─────────────┘
                                          │ HTTPS / WSS
                                          ▼
        ┌──────────────────────────────────────────────────────────────┐
        │                    Drogon 服务进程 (C++)                        │
        │                                                                │
        │  ┌──────────────┐        ┌────────────────────────────────┐   │
        │  │ HTTP 控制器层 │        │     WebSocket 控制器层          │   │
        │  │ (Controllers)│        │  (连接管理 / 消息收发 / 心跳)    │   │
        │  └──────┬───────┘        └──────────────┬─────────────────┘   │
        │         │   JWT 鉴权中间件 (Filter)       │                     │
        │         ▼                                ▼                     │
        │  ┌──────────────────────────────────────────────────────┐    │
        │  │                  Service 业务逻辑层                     │    │
        │  │  Auth / User / Friend / Group / Message / Online       │    │
        │  └──────────────────────┬───────────────────────────────┘    │
        │                         ▼                                      │
        │  ┌──────────────────────────────────────────────────────┐    │
        │  │                   DAO / 数据访问层                      │    │
        │  └──────┬───────────────────────────────┬────────────────┘    │
        │         │                               │                     │
        │  ┌──────▼───────┐    ┌──────────────┐  ┌▼──────────────┐      │
        │  │ MySQL 连接池  │    │ SMTP 邮件客户端│  │ Redis 连接池  │      │
        │  └──────┬───────┘    └──────────────┘  └──────┬────────┘      │
        └─────────┼───────────────────────────────────┼───────────────┘
                  ▼                                    ▼
            ┌──────────┐                        ┌──────────┐
            │  MySQL   │                        │  Redis   │
            └──────────┘                        └──────────┘
```

### 3.2 模块划分

| 模块 | 职责 | 关键点 |
|---|---|---|
| **AuthModule** | 注册、验证码、登录、JWT 签发/校验、单点登录 | 验证码存 Redis；登录写 Redis 会话；JWT Filter 统一鉴权 |
| **UserModule** | 用户资料、搜索用户、role 判断 | 搜索按昵称模糊匹配 + 分页 |
| **FriendModule** | 好友申请/审批、好友列表、删除好友 | 双向关系；删除为双向解除，消息保留 |
| **GroupModule** | 建群、搜索群、加群申请/审批、退群、注销群 | 群主权限校验；注销=硬删除（事务） |
| **MessageModule** | 单聊/群聊消息收发、离线暂存、历史分页 | 服务端生成会话内单调消息序号；离线消息表 |
| **OnlineModule** | 在线表维护、状态推送 | Redis 维护 userId→连接；上下线广播给好友 |
| **WsGateway** | WebSocket 连接生命周期、路由、心跳、挤下线 | 握手时 JWT 鉴权；连接注册/注销；心跳保活 |
| **AdminModule** | 超管查看全部群、注销任意群 | role=admin 校验 |
| **Infra** | MySQL 连接池、Redis 客户端、SMTP 客户端、配置加载、日志 | 连接池复用；配置外置 |

### 3.3 WebSocket 消息协议（统一 JSON 信封）

```jsonc
// 客户端 → 服务端 / 服务端 → 客户端 通用结构
{
  "type": "chat_single | chat_group | notify_apply | notify_apply_result |
           presence | kicked | ack | error | ping | pong",
  "seq": 1024,                 // 消息/事件序号（服务端生成，用于排序）
  "data": { /* 按 type 而定 */ }
}
```

主要事件类型：
- `chat_single` / `chat_group`：聊天消息收发。
- `notify_apply`：收到新的加好友/加群申请。
- `notify_apply_result`：自己的申请被同意/拒绝。
- `presence`：好友上线/下线。
- `kicked`：被新登录挤下线。
- `ack`：服务端确认消息已落库（携带服务端消息 ID/seq）。
- `ping` / `pong`：心跳保活。

---

## 四、数据库结构建议（MySQL）

> 约定：所有表含 `created_at`、`updated_at`；删除多用状态位（好友）或硬删（群）。字符集 `utf8mb4`。

### 4.1 users — 用户表
| 字段 | 类型 | 说明 |
|---|---|---|
| id | BIGINT PK AUTO_INCREMENT | 用户 ID |
| email | VARCHAR(128) UNIQUE | 邮箱（兼作登录账号） |
| password_hash | VARCHAR(100) | bcrypt 哈希 |
| nickname | VARCHAR(64) | 用户名称（可重复，搜索键） |
| role | TINYINT | 0=普通用户，1=超级用户 |
| status | TINYINT | 0=正常，1=禁用 |
| created_at / updated_at | DATETIME | |

索引：`UNIQUE(email)`、`INDEX(nickname)`。

### 4.2 friendships — 好友关系表（双向，存一行）
| 字段 | 类型 | 说明 |
|---|---|---|
| id | BIGINT PK | |
| user_a | BIGINT | 较小的 userId |
| user_b | BIGINT | 较大的 userId |
| status | TINYINT | 1=好友，0=已解除 |
| created_at / updated_at | DATETIME | |

约定：始终 `user_a < user_b`，保证一对好友只有一行；`UNIQUE(user_a, user_b)`。删除好友→status=0（保留行以便复用与历史关联）。

### 4.3 friend_requests — 好友申请表
| 字段 | 类型 | 说明 |
|---|---|---|
| id | BIGINT PK | |
| from_user | BIGINT | 申请人 |
| to_user | BIGINT | 被申请人 |
| message | VARCHAR(255) | 验证留言（可空） |
| status | TINYINT | 0=待处理，1=同意，2=拒绝 |
| created_at / updated_at | DATETIME | |

索引：`INDEX(to_user, status)`。

### 4.4 group_chats — 群表
| 字段 | 类型 | 说明 |
|---|---|---|
| id | BIGINT PK | 群 ID |
| name | VARCHAR(64) | 群名称（搜索键） |
| owner_id | BIGINT | 群主 userId |
| member_count | INT | 成员数（冗余，便于管理面板展示） |
| created_at / updated_at | DATETIME | |

索引：`INDEX(name)`、`INDEX(owner_id)`。注销群=DELETE（连带成员、消息，事务内硬删）。

### 4.5 group_members — 群成员表
| 字段 | 类型 | 说明 |
|---|---|---|
| id | BIGINT PK | |
| group_id | BIGINT | |
| user_id | BIGINT | |
| role | TINYINT | 0=普通成员，1=群主 |
| joined_at | DATETIME | |

索引：`UNIQUE(group_id, user_id)`、`INDEX(user_id)`。退群=DELETE 该行。

### 4.6 group_requests — 加群申请表
| 字段 | 类型 | 说明 |
|---|---|---|
| id | BIGINT PK | |
| group_id | BIGINT | |
| from_user | BIGINT | 申请人 |
| status | TINYINT | 0=待处理，1=同意，2=拒绝 |
| created_at / updated_at | DATETIME | |

索引：`INDEX(group_id, status)`。

### 4.7 single_messages — 单聊消息表
| 字段 | 类型 | 说明 |
|---|---|---|
| id | BIGINT PK | 全局消息 ID |
| conv_key | VARCHAR(32) | 会话键 `min(uid)-max(uid)`，用于分页查询 |
| from_user | BIGINT | 发送者 |
| to_user | BIGINT | 接收者 |
| content | TEXT | 文本内容 |
| seq | BIGINT | 会话内单调序号（服务端生成） |
| created_at | DATETIME | |

索引：`INDEX(conv_key, seq)`。历史分页按 `conv_key` + `seq` 游标向前翻。

### 4.8 group_messages — 群聊消息表
| 字段 | 类型 | 说明 |
|---|---|---|
| id | BIGINT PK | |
| group_id | BIGINT | |
| from_user | BIGINT | |
| content | TEXT | |
| seq | BIGINT | 群内单调序号 |
| created_at | DATETIME | |

索引：`INDEX(group_id, seq)`。注销群时按 group_id 批量删除。

### 4.9 offline_messages — 离线消息索引表
| 字段 | 类型 | 说明 |
|---|---|---|
| id | BIGINT PK | |
| user_id | BIGINT | 接收者（离线时未投递） |
| msg_type | TINYINT | 1=单聊，2=群聊 |
| msg_id | BIGINT | 指向 single_messages / group_messages 的 id |
| created_at | DATETIME | |

索引：`INDEX(user_id)`。用户上线后按此表拉取并投递，投递成功后删除对应行。
> 说明：群离线消息也可按「成员 last_read_seq」方案实现，本期采用离线索引表，逻辑直观。

### 4.10 Redis 键设计
| Key | 类型 | 用途 |
|---|---|---|
| `verifycode:{email}` | String, TTL 5min | 注册验证码 |
| `verifycode_limit:{email}` | String, TTL 60s | 验证码发送限频 |
| `session:{userId}` | String | 当前有效 token/连接标识，用于单点挤下线 |
| `online:{userId}` | String, TTL+心跳续期 | 在线状态 |
| `msgseq:single:{convKey}` | INCR | 单聊会话内消息序号 |
| `msgseq:group:{groupId}` | INCR | 群消息序号 |

---

## 五、TODO 开发清单

### 阶段 0：工程脚手架
- [x] 0.1 初始化 Drogon 项目结构，配置 CMakeLists.txt
- [x] 0.2 编写 `config.json`（DB/Redis/SMTP/JWT 密钥/端口）
- [x] 0.3 封装 MySQL 连接池、Redis 客户端、SMTP 客户端
- [x] 0.4 统一日志、统一 JSON 响应结构、统一错误码
- [x] 0.5 编写建表 SQL（含预置 admin 超管账号）
- [x] 0.6 Dockerfile + docker-compose（app + MySQL + Redis）

### 阶段 1：认证模块
- [x] 1.1 发送邮箱验证码接口（写 Redis + 限频 + SMTP）
- [x] 1.2 注册接口（校验验证码、bcrypt 哈希、写 users）
- [x] 1.3 登录接口（校验密码、签发 JWT、写 Redis session）
- [x] 1.4 JWT 鉴权 Filter（HTTP）与 WS 握手鉴权
- [x] 1.5 单点登录挤下线逻辑（新登录覆盖 session + kick 旧连接）

### 阶段 2：WebSocket 网关
- [x] 2.1 连接注册/注销（userId ↔ 连接映射）
- [x] 2.2 消息信封解析与按 type 路由
- [x] 2.3 心跳 ping/pong 与超时断连
- [x] 2.4 在线表维护与好友上下线 presence 推送

### 阶段 3：用户与好友模块
- [x] 3.1 用户资料查询、按昵称模糊搜索（分页）
- [x] 3.2 加好友申请、申请列表、同意/拒绝（实时推送结果）
- [x] 3.3 好友列表查询（含在线状态）
- [x] 3.4 删除好友（双向解除，保留消息）

### 阶段 4：群聊模块
- [ ] 4.1 创建群、按群名模糊搜索（分页）
- [ ] 4.2 加群申请、群主审批（同意/拒绝）
- [ ] 4.3 退群、群成员列表
- [ ] 4.4 注销群（群主权限，事务硬删群/成员/消息）
- [ ] 4.5 我加入的群列表

### 阶段 5：消息模块
- [ ] 5.1 单聊消息收发（落库 + 序号 + 在线投递/离线暂存 + ack）
- [ ] 5.2 群聊消息收发（落库 + 广播在群在线成员 + 离线暂存）
- [ ] 5.3 上线后拉取离线消息并投递
- [ ] 5.4 历史消息分页查询接口（游标翻页）

### 阶段 6：管理模块
- [ ] 6.1 超管查看全部群列表（元数据 + 分页）
- [ ] 6.2 超管注销任意群（复用注销逻辑 + role 校验）

### 阶段 7：前端
- [ ] 7.1 注册/登录页（验证码流程）
- [ ] 7.2 主界面布局（好友/群列表、会话区、搜索）
- [ ] 7.3 WebSocket 客户端封装（重连、心跳、收发）
- [ ] 7.4 单聊/群聊界面 + 历史滚动加载
- [ ] 7.5 申请通知与审批交互
- [ ] 7.6 超管管理面板

### 阶段 8：联调与交付
- [ ] 8.1 API/WS 协议文档定稿
- [ ] 8.2 端到端联调（注册→加好友→聊天→建群→群聊→注销）
- [ ] 8.3 异常路径测试（挤下线、离线消息、权限越权）

---

## 六、核心模块验收标准

### 6.1 认证模块
- [ ] 未注册邮箱可收到验证码；验证码 5 分钟后失效；60s 内重复发送被限频拦截。
- [ ] 验证码错误/过期无法完成注册；密码以 bcrypt 哈希存储（库中无明文）。
- [ ] 正确邮箱+密码登录返回有效 JWT；错误密码被拒。
- [ ] 携带过期/伪造 JWT 访问受保护接口返回 401。
- [ ] **单点登录**：同账号在 B 端登录后，A 端连接收到 `kicked` 并断开，A 端旧 token 失效。

### 6.2 好友模块
- [ ] A 搜索昵称能命中 B（模糊+分页）；搜索结果不含敏感字段（无密码哈希）。
- [ ] A 向 B 申请，B 在线时实时收到 `notify_apply`；B 离线时上线后能在申请列表看到。
- [ ] B 同意后双方互为好友、互相出现在好友列表；B 拒绝则不建立关系。
- [ ] 删除好友后双方都不再是好友、互不能发消息；重新加为好友后能看到此前历史消息。

### 6.3 群聊模块
- [ ] 任意用户可建群且自动成为群主（group_members.role=1）。
- [ ] 非群主无法审批加群、无法注销群（越权返回 403）。
- [ ] 群主同意加群后申请人出现在成员列表、member_count 正确 +1。
- [ ] 成员退群后从成员列表移除、member_count 正确 -1，且不再收到该群消息。
- [ ] 注销群后：群、成员关系、群消息记录全部从库删除；所有原成员前端不再展示该群。

### 6.4 消息模块
- [ ] 单聊消息仅收发双方可见，第三方查询不到。
- [ ] 群消息推送给「当前在群且在线」的所有成员；退群者收不到后续消息。
- [ ] 接收方离线时消息入离线表，上线后按序拉取且不丢、不重复。
- [ ] 同一会话消息按 seq 严格有序展示；历史向上滚动能正确分页加载更早消息。
- [ ] 发送方收到服务端 `ack`（含服务端消息 ID/seq）确认已落库。

### 6.5 在线状态
- [ ] 好友上线/下线时，在线好友实时收到 `presence` 更新。
- [ ] 心跳超时的连接被判定离线并清理在线表。

### 6.6 权限模块
- [ ] 普通用户调用超管接口（查看全部群/注销任意群）返回 403。
- [ ] 超管能分页查看全部群的元数据，但无任意群消息内容查看接口。
- [ ] 超管注销任意群效果与群主注销一致（硬删 + 通知原成员）。

### 6.7 工程交付
- [ ] `cmake && make` 在 Linux 下一键编译通过。
- [ ] `docker-compose up` 一键拉起 app + MySQL + Redis 并可访问。
- [ ] 建表 SQL 可幂等初始化库并预置 admin 超管。
- [ ] API/WS 协议文档与实现一致，覆盖所有接口与事件类型。

---

## 七、约定与默认值

1. 密码：bcrypt 加盐哈希；JWT 有效期 7 天；验证码 5 分钟有效、60s 限频。
2. 消息顺序：服务端用 Redis INCR 生成会话内单调 seq，不依赖客户端时间。
3. 搜索：昵称/群名模糊匹配，分页默认每页 20。
4. 群离线消息采用离线索引表（offline_messages）实现，投递成功即删除索引。
5. 不实现：图片/文件消息、已读未读、踢人、转让群主、禁言、多端同时在线。

---

> 本 SPEC 为开发依据。如开发过程中发现需求遗漏或冲突，应回到本文档更新后再实现，保持文档与代码一致。
