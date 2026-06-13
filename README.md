# online-chat

一个基于 C++ / Drogon 框架的网页版聊天工具，类似微信的轻量级 Web IM 系统。

支持注册登录、好友聊天、群聊、离线消息、在线状态推送、超管管理面板等完整功能。

---

## 🤖 AI 开发说明

**本项目完全由 AI（Claude）辅助开发完成**，从需求分析、架构设计到代码实现、测试编写，全程采用人机协作模式。

### 开发理念

本项目采用 **「深度访谈 → 需求确认 → 分阶段实现」** 的 AI 驱动开发模式：
- **不急于编码**：先通过多轮对话彻底理清需求，避免返工
- **渐进式开发**：将大型项目拆分为多个阶段，每阶段独立完成、测试、提交
- **质量优先**：每个阶段都配套完整的单元测试，确保代码质量

### 开发流程

```
┌─────────────────────────────────────────────────────────────────┐
│                      AI 驱动开发流程                              │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│   ① 需求访谈          ② 规格文档          ③ 分阶段开发           │
│   ───────────        ───────────        ───────────            │
│   • 功能需求          • SPEC.md           • 阶段 0: 脚手架        │
│   • 技术选型    ──→   • 数据库设计   ──→  • 阶段 1: 认证模块      │
│   • 细节确认          • API 设计          • 阶段 2: WebSocket     │
│   • 边界条件          • 开发清单          • 阶段 3~6: 业务模块    │
│                                             • 阶段 7: 前端页面    │
│                                                                 │
│                          ④ 测试验证                             │
│                          ───────────                            │
│                          • 单元测试 (733 个)                      │
│                          • E2E 测试 (48 个)                       │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### 开发阶段详情

| 阶段 | 内容 | 测试用例 | Git 提交 |
|------|------|----------|----------|
| **阶段 0** | 工程脚手架：CMakeLists、Drogon 骨架、配置文件 | 36 | `b060b69` ~ `8f93eb1` |
| **阶段 1** | 认证模块：邮箱验证码、JWT 鉴权、bcrypt 密码哈希 | 51 | `e32a4d7` |
| **阶段 2** | WebSocket 网关：连接管理、消息路由、心跳保活 | 62 | `b4870aa` |
| **阶段 3** | 用户与好友：搜索、好友申请/审批/删除、在线状态推送 | 81 | `cde14cb` |
| **阶段 4** | 群聊模块：建群、搜索、申请/审批、退群、注销 | 147 | `b969d64` |
| **阶段 5** | 消息模块：单聊/群聊实时收发、离线消息、历史分页 | 221 | `c66a004` |
| **阶段 6** | 管理模块：超管面板、全部群列表、注销任意群 | 249 | `08356bf` |
| **阶段 7** | 前端页面：登录/注册、聊天界面、管理面板 | 48 (E2E) | `4ca1c40` |

### AI 协作方式

1. **需求澄清阶段**
   - AI 通过 Socratic 提问法，每轮 3~5 个问题深入挖掘需求细节
   - 覆盖功能边界、技术选型、异常处理、权限控制等维度
   - 用户确认后输出完整的技术规格文档（SPEC.md）

2. **代码实现阶段**
   - AI 根据 SPEC.md 逐阶段实现功能
   - 每个阶段完成后进行代码审查和重构
   - 遵循 C++17 标准，注重代码可读性和逻辑清晰

3. **测试保障阶段**
   - 每个模块配套 GTest 单元测试
   - 最终集成 Playwright E2E 测试
   - 测试覆盖率达 733 个单元测试 + 48 个 E2E 测试

### 开发成果

- **后端代码**：C++ 实现，包含 7 个控制器、完整的服务层和工具类
- **前端页面**：原生 HTML/CSS/JS，支持响应式设计
- **测试覆盖**：781 个测试用例（733 单元 + 48 E2E）
- **完整文档**：SPEC.md、API.md、使用手册、测试文档

### prompt.md

项目根目录下的 `prompt.md` 文件记录了最初的 AI 对话 prompt，展示了需求访谈的起点。这是 AI 驱动开发的起点——通过精心设计的 prompt 引导 AI 进行深度需求分析。

### 开发过程中遇到的问题与修复

在 AI 驱动开发过程中，遇到了多个典型问题，通过代码审查和测试逐步发现并修复：

#### 阶段 0：工程脚手架阶段（3 个问题）

| 问题 | 原因 | 修复方案 |
|------|------|----------|
| **头文件缺失** | `config.h` 缺少 `<sstream>` 和 `<json/json.h>` 的 include | 补齐头文件依赖 |
| **JSON 解析崩溃** | JSON 路径遍历时未检查节点类型，遇到非对象节点抛异常 | 增加 `isObject()` 类型检查 |
| **Shell 注入漏洞** | `smtp_client.h` 中的 `escapeShell` 函数存在缺陷，未正确转义特殊字符 | 重写为 `shellQuote`，统一转义所有壳参数 |

#### 阶段 7：接口联调阶段（7 个问题）

| 问题 | 原因 | 修复方案 |
|------|------|----------|
| **JWT 过滤器编译失败** | Drogon 1.9.13 API 变更（`kString`/`asString`/`insert`） | 适配新 API，将 `doFilter` 移至 `.cc` 文件生成静态注册符号 |
| **WebSocket 控制器未注册** | 错误设置 `AutoCreation=false` 导致控制器未自动注册 | 移除该设置，恢复自动注册 |
| **JWT 密钥泄露** | `issuer`/`expire` 误用 `JWT_SECRET` 环境变量，导致密钥写入 token 的 `iss` 字段 | 改用独立环境变量配置 |
| **越权审批加群** | `group_service` 中加群审批缺少群主权限校验，任何用户均可审批 | 补充群主权限验证逻辑 |
| **好友重复申请竞态** | 删除好友后无法重新发起申请；审批状态更新前返回响应导致重复处理 | 允许软删后重新申请；等待状态更新完成后再返回 |
| **离线消息重复拉取** | 离线消息索引删除前返回响应，导致重复拉取 | 等待索引删除完成后再返回 |
| **单点登录失效** | 登录成功后未踢掉旧的 WebSocket 连接 | 通过 `ConnectionManager` 踢掉旧连接，发送 `kicked` 推送 |

#### 问题分类与经验总结

| 问题类型 | 数量 | 典型表现 | 预防措施 |
|----------|------|----------|----------|
| **编译错误** | 2 | 头文件缺失、API 不兼容 | 依赖检查、版本适配 |
| **安全漏洞** | 2 | Shell 注入、密钥泄露 | 安全审查、最小权限原则 |
| **逻辑缺陷** | 4 | 权限校验缺失、竞态条件 | 单元测试覆盖、边界条件检查 |
| **框架适配** | 2 | API 变更、自动注册机制 | 框架文档学习、版本锁定 |

#### 修复统计

```
修复提交：2 个（8f93eb1, 68d788e）
涉及文件：24 个
代码变更：+541 行 / -370 行
测试验证：249/249 单元测试通过
```

这些问题反映了 AI 开发中的典型挑战：
1. **框架版本兼容性**：AI 需要适配特定版本的 API 变更
2. **安全意识**：AI 可能忽略安全边界，需要人工审查
3. **竞态条件**：并发场景下的时序问题需要仔细设计
4. **权限控制**：业务逻辑中的权限校验容易遗漏

通过「AI 实现 + 人工审查 + 测试验证」的协作模式，这些问题都被及时发现并修复，最终保证了项目的质量。

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
