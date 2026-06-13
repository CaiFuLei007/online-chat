# online-chat

一个基于 C++ / Drogon 框架的网页版聊天工具，类似微信的轻量级 Web IM 系统。

支持注册登录、好友聊天、群聊、离线消息、在线状态推送、超管管理面板等完整功能。

---

## 功能特性

| 模块 | 功能 |
|------|------|
| **认证** | 邮箱验证码注册、JWT 鉴权、单点登录挤下线 |
| **好友** | 搜索用户、发送/同意/拒绝好友申请、删除好友、在线状态实时推送 |
| **群聊** | 创建群、搜索群、申请/审批加群、退群、注销群（硬删） |
| **消息** | 单聊实时收发、群聊广播、离线消息补收、历史分页滚动加载 |
| **管理** | 超管查看全部群列表、注销任意群、普通用户越权拦截 |

---

## 技术栈

| 层级 | 技术 |
|------|------|
| 后端框架 | C++17 / [Drogon](https://github.com/drogonframework/drogon) v1.9.13 |
| 数据库 | MySQL 8.0 |
| 缓存 | Redis 7 |
| 前端 | 原生 HTML / CSS / JavaScript + WebSocket |
| 认证 | JWT + bcrypt 密码哈希 |
| 部署 | Docker Compose（app + MySQL + Redis 一键拉起） |

---

## 快速开始

### 方式一：Docker 部署（推荐）

```bash
# 1. 克隆项目
git clone https://github.com/CaiFuLei007/online-chat.git
cd online-chat

# 2. 配置环境变量
cd docker
cp .env.example .env
vim .env    # 填入 JWT_SECRET 和 SMTP 配置

# 3. 构建并启动（首次约 10-20 分钟，之后有缓存）
docker compose up -d --build

# 4. 验证
curl -s -o /dev/null -w "%{http_code}\n" http://localhost:8080/
# 期望输出 200
```

浏览器访问：

- 首页（登录/注册）：http://localhost:8080/
- 聊天页：http://localhost:8080/chat.html
- 管理面板：http://localhost:8080/admin.html

### 方式二：本地编译

```bash
# 1. 安装依赖（Ubuntu/Debian）
sudo apt-get update && sudo apt-get install -y --no-install-recommends \
    build-essential cmake git ca-certificates \
    libjsoncpp-dev uuid-dev libssl-dev zlib1g-dev \
    libmysqlclient-dev libhiredis-dev curl libgtest-dev

# 2. 编译 Drogon 框架（从源码）
git clone --depth 1 --branch v1.9.13 \
    https://github.com/drogonframework/drogon.git /tmp/drogon
cd /tmp/drogon && git submodule update --init --recursive
cmake -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_EXAMPLES=OFF -DBUILD_CTL=OFF
cmake --build build -j$(nproc) && sudo cmake --install build

# 3. 配置本地开发（将 Docker 服务名改为 127.0.0.1）
cd ~/online-chat
cp config/config.local.json.example config/config.local.json
# 按需修改 config.local.json 中的数据库密码

# 4. 编译项目
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

# 5. 初始化数据库
mysql -u root -p < sql/schema.sql

# 6. 设置环境变量并运行
export JWT_SECRET="your-secret-key"
export SMTP_HOST="smtp.example.com"
export SMTP_PORT=465
export SMTP_USERNAME="your-email@example.com"
export SMTP_PASSWORD="your-password"
export SMTP_FROM_EMAIL="your-email@example.com"
cd build && ./online_chat
```

---

## 项目结构

```
online-chat/
├── CMakeLists.txt              # 构建配置
├── config/
│   ├── config.json             # Drogon 运行时配置（Docker 环境）
│   └── config.local.json.example  # 本地开发配置模板
├── docker/
│   ├── Dockerfile              # 多阶段构建（builder + runtime）
│   ├── docker-compose.yml      # 三容器编排（app + MySQL + Redis）
│   ├── .env.example            # 环境变量模板
│   └── .env                    # 实际配置（不入库）
├── frontend/
│   ├── index.html              # 登录/注册页
│   ├── chat.html               # 聊天主界面
│   ├── admin.html              # 超管管理面板
│   ├── css/                    # 样式
│   ├── js/                     # 前端逻辑（api/ui/ws/chat/auth/admin）
│   └── uploads/                # 用户上传文件
├── sql/
│   └── schema.sql              # 建表 SQL（含预置 admin 超管账号）
├── src/
│   ├── controllers/            # HTTP / WebSocket 控制器
│   │   ├── auth_controller.*   # 认证（注册/登录/验证码）
│   │   ├── user_controller.*   # 用户（资料/搜索）
│   │   ├── friend_controller.* # 好友（申请/审批/列表/删除）
│   │   ├── group_controller.*  # 群聊（建群/搜索/审批/退群/注销）
│   │   ├── message_controller.*# 消息（历史查询/离线拉取）
│   │   ├── admin_controller.*  # 管理（全部群/注销群）
│   │   └── ws_gateway.*        # WebSocket 网关（连接/消息路由/心跳）
│   ├── services/               # 业务逻辑层
│   ├── filters/                # JWT 鉴权中间件
│   └── utils/                  # 工具类（JWT/密码/SMTP/协议/错误码）
├── tests/                      # C++ 单元测试 + E2E 测试
│   ├── test_*.cpp              # GTest 单元测试（733 个用例）
│   └── e2e/                    # Playwright E2E 测试（48 个用例）
├── API.md                      # 接口文档
├── SPEC.md                     # 技术规格说明书
├── 使用手册.md                  # Docker 使用手册
└── 项目接口测试-*.md            # 接口测试文档（curl / Playwright）
```

---

## 数据库预置账号

| 账号 | 密码 | 角色 |
|------|------|------|
| `admin@online-chat.local` | `admin123` | 超级用户（管理面板） |

> 其他测试账号见 `项目接口测试-palywright.md` 中的测试账号约定。

---

## API 接口概览

所有 HTTP 接口返回格式：`{ "code": 0, "msg": "success", "data": {...} }`

| 模块 | 接口 | 说明 |
|------|------|------|
| 认证 | `POST /api/auth/send-code` | 发送邮箱验证码 |
| 认证 | `POST /api/auth/register` | 注册 |
| 认证 | `POST /api/auth/login` | 登录，返回 JWT |
| 用户 | `GET /api/user/profile` | 获取当前用户资料 |
| 用户 | `GET /api/user/search?keyword=` | 按昵称搜索用户 |
| 好友 | `POST /api/friend/request` | 发送好友申请 |
| 好友 | `GET /api/friend/requests` | 获取待处理申请列表 |
| 好友 | `POST /api/friend/accept/{id}` | 同意好友申请 |
| 好友 | `POST /api/friend/reject/{id}` | 拒绝好友申请 |
| 好友 | `GET /api/friend/list` | 好友列表（含在线状态） |
| 好友 | `DELETE /api/friend/{id}` | 删除好友 |
| 群聊 | `POST /api/group/create` | 创建群 |
| 群聊 | `GET /api/group/search?keyword=` | 按群名搜索群 |
| 群聊 | `POST /api/group/join/{id}` | 申请加群 |
| 群聊 | `POST /api/group/accept/{id}` | 同意加群（群主） |
| 群聊 | `POST /api/group/reject/{id}` | 拒绝加群（群主） |
| 群聊 | `POST /api/group/leave/{id}` | 退群 |
| 群聊 | `DELETE /api/group/{id}` | 注销群（群主） |
| 消息 | `GET /api/message/single/history` | 单聊历史分页 |
| 消息 | `GET /api/message/group/history` | 群聊历史分页 |
| 消息 | `GET /api/message/offline` | 拉取离线消息 |
| 管理 | `GET /api/admin/groups` | 全部群列表（超管） |
| 管理 | `DELETE /api/admin/groups/{id}` | 注销任意群（超管） |

详细参数与响应格式见 [API.md](API.md)。

---

## WebSocket 协议

连接地址：`ws://localhost:8080/ws?token={JWT}`

统一信封格式：

```json
{
  "type": "消息类型",
  "seq": 12345,
  "data": { }
}
```

| type | 方向 | 说明 |
|------|------|------|
| `ping` / `pong` | 双向 | 心跳保活 |
| `chat_single` | 双向 | 单聊消息 |
| `chat_group` | 双向 | 群聊消息 |
| `notify_apply` | S→C | 收到好友/加群申请 |
| `notify_apply_result` | S→C | 自己的申请被处理 |
| `presence` | S→C | 好友上下线 |
| `kicked` | S→C | 被挤下线 |
| `ack` | S→C | 消息已落库确认 |

---

## 测试

### C++ 单元测试（733 个用例）

```bash
cd build
ctest --output-on-failure
```

### Playwright E2E 测试（48 个用例）

```bash
cd tests/e2e
npm install
npx playwright install --with-deps chromium

# 运行全部（管理面板需要 PASSWORD 环境变量）
REDIS_IN_DOCKER=1 PASSWORD=admin123 npx playwright test

# 运行单个模块
npx playwright test specs/01-auth.spec.js
npx playwright test specs/03-friend.spec.js

# 有头模式调试
npx playwright test --headed --debug
```

---

## 文档

| 文档 | 说明 |
|------|------|
| [SPEC.md](SPEC.md) | 技术规格说明书（需求、架构、数据库设计、开发清单） |
| [API.md](API.md) | HTTP / WebSocket 接口文档 |
| [使用手册.md](使用手册.md) | Docker 部署与日常管理 |
| [项目接口测试-curl.md](项目接口测试-curl.md) | curl 接口测试手册 |
| [项目接口测试-palywright.md](项目接口测试-palywright.md) | Playwright E2E 测试文档 |

---

## 安全提醒

- `JWT_SECRET` 必须在 `.env` 中设置强随机值，不要使用默认值。
- `.env` 文件包含敏感凭据，已被 `.gitignore` 排除，**不要提交到版本库**。
- `config/config.json` 中的数据库密码是开发用弱口令，生产环境请修改。
- Docker 端口映射（3307/6380）仅用于本机调试，生产环境建议删除。

---

## License

MIT
