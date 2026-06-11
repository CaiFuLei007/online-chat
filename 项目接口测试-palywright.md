# 项目接口测试-palywright

> 使用 Playwright 对 online-chat 前端 + 后端进行端到端（E2E）联调的方法与步骤。
>
> 与 `项目接口测试-curl.md` 的区别：curl 只测后端 HTTP 接口；Playwright 驱动真实浏览器，
> 走完整链路 **页面 UI → fetch/WebSocket → Drogon 后端 → MySQL/Redis**，
> 能覆盖验证码流程、双人实时聊天、挤下线、在线状态推送等 curl 无法验证的场景。

---

## 目录

- [一、环境准备](#一环境准备)
- [二、测试工程初始化](#二测试工程初始化)
- [三、关键问题：验证码如何自动获取](#三关键问题验证码如何自动获取)
- [四、测试用例编写（按模块）](#四测试用例编写按模块)
- [五、运行与调试](#五运行与调试)
- [六、联调步骤清单（对应 SPEC 阶段 8）](#六联调步骤清单对应-spec-阶段-8)
- [七、常见问题](#七常见问题)

---

## 一、环境准备

### 1.1 启动后端全家桶

```bash
# 方式 A：Docker 一键拉起（推荐）
cd ~/online-chat
docker compose -f docker/docker-compose.yml up -d

# 方式 B：本地直接运行（需本机已有 MySQL/Redis）
cd ~/online-chat/build && ./online_chat
```

验证服务就绪：

```bash
curl -s -o /dev/null -w "%{http_code}\n" http://localhost:8080/index.html
# 期望输出 200（前端静态页由 Drogon document_root=./frontend 直接托管）
```

### 1.2 安装 Node.js 与 Playwright

```bash
# Node.js ≥ 18（Ubuntu）
sudo apt-get install -y nodejs npm

# 在项目下新建独立的 e2e 测试目录（不污染 C++ 工程）
mkdir -p ~/online-chat/tests/e2e && cd ~/online-chat/tests/e2e
npm init -y
npm install -D @playwright/test

# 安装浏览器内核（chromium 足够；--with-deps 自动装系统依赖）
npx playwright install --with-deps chromium
```

### 1.3 测试账号约定（与 curl 文档保持一致）

| 角色 | 邮箱 | 密码 | 说明 |
|---|---|---|---|
| 普通用户 A | `alice@example.com` | `alice123` | 测试中动态注册 |
| 普通用户 B | `bob@example.com` | `bob123456` | 测试中动态注册 |
| 超管 | `admin@online-chat.local` | `admin123` | 建表 SQL 预置，role=1 |

> 超管密码不要写死在测试代码中，通过环境变量 `PASSWORD` 注入（同 `frontend/gen-env.sh` 约定）。

---

## 二、测试工程初始化

### 2.1 目录结构

```
tests/e2e/
├── package.json
├── playwright.config.js        # Playwright 配置
├── helpers/
│   └── utils.js                # 取验证码、注册登录等公共函数
└── specs/
    ├── 01-auth.spec.js         # 认证：验证码/注册/登录/挤下线
    ├── 02-friend.spec.js       # 好友：搜索/申请/审批/删除/在线状态
    ├── 03-group.spec.js        # 群聊：建群/搜群/加群审批/退群/注销
    ├── 04-message.spec.js      # 消息：单聊/群聊/历史分页/离线消息
    └── 05-admin.spec.js        # 管理面板：全部群列表/注销任意群
```

### 2.2 playwright.config.js

```js
// tests/e2e/playwright.config.js
const { defineConfig } = require('@playwright/test');

module.exports = defineConfig({
  testDir: './specs',
  timeout: 30_000,
  // 聊天用例涉及双浏览器实时交互，串行执行避免账号互踩（单点登录会互相挤下线）
  fullyParallel: false,
  workers: 1,
  retries: 0,
  reporter: [['list'], ['html', { open: 'never' }]],
  use: {
    baseURL: process.env.BASE_URL || 'http://localhost:8080',
    trace: 'retain-on-failure',     // 失败时保留 trace 便于回放
    screenshot: 'only-on-failure',
    video: 'retain-on-failure',
  },
});
```

### 2.3 package.json scripts

```json
{
  "scripts": {
    "test": "playwright test",
    "test:headed": "playwright test --headed",
    "test:debug": "playwright test --debug",
    "report": "playwright show-report"
  }
}
```

---

## 三、关键问题：验证码如何自动获取

注册流程需要邮箱验证码，E2E 测试不能人工查邮箱。验证码存在 Redis
（键 `verifycode:{email}`，TTL 5 分钟），测试中直接用 `redis-cli` 读取：

```js
// tests/e2e/helpers/utils.js
const { execSync } = require('child_process');

const BASE = process.env.BASE_URL || 'http://localhost:8080';

/** 从 Redis 读取邮箱验证码（后端发送后立即可读） */
function getVerifyCode(email) {
  // Docker 部署时改为：docker exec oc-redis redis-cli GET ...
  const cmd = process.env.REDIS_IN_DOCKER === '1'
    ? `docker exec oc-redis redis-cli GET "verifycode:${email}"`
    : `redis-cli GET "verifycode:${email}"`;
  const code = execSync(cmd).toString().trim().replace(/"/g, '');
  if (!/^\d{6}$/.test(code)) throw new Error(`未取到 ${email} 的验证码，得到: "${code}"`);
  return code;
}

/** 清理验证码限频键，便于重复跑测试（60s 限频会拦截重发） */
function clearCodeLimit(email) {
  const prefix = process.env.REDIS_IN_DOCKER === '1' ? 'docker exec oc-redis ' : '';
  execSync(`${prefix}redis-cli DEL "verifycode_limit:${email}" "verifycode:${email}"`);
}

/** 走 UI 完成注册（页面在 index.html 注册 Tab） */
async function registerViaUI(page, { email, password, nickname }) {
  await page.goto('/index.html');
  await page.getByRole('tab', { name: '注册' }).click();
  await page.locator('#reg-email').fill(email);
  await page.locator('#send-code-btn').click();
  // 等后端写入 Redis（toast 出现即已发送）
  await page.getByText('验证码已发送').waitFor({ timeout: 5000 });
  const code = getVerifyCode(email);
  await page.locator('#reg-code').fill(code);
  await page.locator('#reg-password').fill(password);
  if (nickname) await page.locator('#reg-nickname').fill(nickname);
  await page.locator('#reg-submit').click();
  await page.getByText('注册成功').waitFor({ timeout: 5000 });
}

/** 走 UI 登录，成功后落在 chat.html */
async function loginViaUI(page, email, password) {
  await page.goto('/index.html');
  await page.locator('#login-email').fill(email);
  await page.locator('#login-password').fill(password);
  await page.locator('#login-submit').click();
  await page.waitForURL('**/chat.html', { timeout: 5000 });
}

/** 用 API 直接注册+登录拿 token（跳过 UI，用于准备测试数据更快） */
async function apiRegisterAndLogin(request, { email, password, nickname }) {
  clearCodeLimit(email);
  await request.post(`${BASE}/api/auth/send-code`, { data: { email } });
  const code = getVerifyCode(email);
  await request.post(`${BASE}/api/auth/register`, {
    data: { email, code, password, nickname },
  }); // 已注册(2003)也无妨，继续登录
  const res = await request.post(`${BASE}/api/auth/login`, {
    data: { email, password },
  });
  const json = await res.json();
  if (json.code !== 0) throw new Error('登录失败: ' + json.msg);
  return json.data; // { token, userId, nickname, role }
}

module.exports = { getVerifyCode, clearCodeLimit, registerViaUI, loginViaUI, apiRegisterAndLogin };
```

> **SMTP 说明**：本地联调若未配置真实 SMTP，发信会失败但验证码仍先写入 Redis
> （取决于后端实现顺序）。若后端是"发信成功才写 Redis"，联调时可将 SMTP 指向
> [MailHog](https://github.com/mailhog/MailHog)（`docker run -p 1025:1025 -p 8025:8025 mailhog/mailhog`），
> 验证码改从 MailHog API `http://localhost:8025/api/v2/messages` 解析。

---

## 四、测试用例编写（按模块）

### 4.1 认证模块（01-auth.spec.js）

覆盖 SPEC 验收 6.1：验证码、注册、登录、错误密码、限频、**单点登录挤下线**。

```js
const { test, expect } = require('@playwright/test');
const { registerViaUI, loginViaUI, clearCodeLimit, getVerifyCode } = require('../helpers/utils');

const ALICE = { email: 'alice@example.com', password: 'alice123', nickname: 'Alice' };

test.describe('认证模块', () => {
  test('注册：发送验证码 → 填码注册 → 自动切回登录 Tab', async ({ page }) => {
    clearCodeLimit(ALICE.email);
    await registerViaUI(page, ALICE);
    await expect(page.locator('#form-login')).toBeVisible();
  });

  test('注册：60 秒内重复发送验证码被限频', async ({ page }) => {
    await page.goto('/index.html');
    await page.getByRole('tab', { name: '注册' }).click();
    await page.locator('#reg-email').fill(ALICE.email);
    await page.locator('#send-code-btn').click();
    // 前端倒计时按钮立即禁用；强行二发由后端 2007 拦截
    await expect(page.locator('#send-code-btn')).toBeDisabled();
  });

  test('登录：错误密码提示"邮箱或密码错误"', async ({ page }) => {
    await page.goto('/index.html');
    await page.locator('#login-email').fill(ALICE.email);
    await page.locator('#login-password').fill('wrong-password');
    await page.locator('#login-submit').click();
    await expect(page.locator('#login-error')).toHaveText(/邮箱或密码错误/);
  });

  test('登录：正确密码进入 chat.html 并显示昵称', async ({ page }) => {
    await loginViaUI(page, ALICE.email, ALICE.password);
    await expect(page.locator('#me-name')).toHaveText(ALICE.nickname);
  });

  test('单点登录：B 端登录后 A 端被 kicked 踢回登录页', async ({ browser }) => {
    // 两个独立 context = 两台"设备"
    const ctxA = await browser.newContext();
    const ctxB = await browser.newContext();
    const pageA = await ctxA.newPage();
    const pageB = await ctxB.newPage();

    await loginViaUI(pageA, ALICE.email, ALICE.password);
    // pageA 的 WS 已连接；B 端再登录同一账号
    await loginViaUI(pageB, ALICE.email, ALICE.password);

    // A 端收到 kicked → alert → 跳回 index.html（chat.js 中 alert 后跳转）
    pageA.on('dialog', d => d.accept());
    await pageA.waitForURL('**/index.html', { timeout: 10_000 });

    await ctxA.close();
    await ctxB.close();
  });
});
```

### 4.2 好友模块（02-friend.spec.js）

覆盖 SPEC 验收 6.2：搜索、申请实时推送、审批、在线状态、删除好友。
核心技巧：**用两个 `browser.newContext()` 模拟 Alice 和 Bob 同时在线**。

```js
const { test, expect } = require('@playwright/test');
const { loginViaUI, apiRegisterAndLogin } = require('../helpers/utils');

const ALICE = { email: 'alice@example.com', password: 'alice123', nickname: 'Alice' };
const BOB   = { email: 'bob@example.com',   password: 'bob123456', nickname: 'Bob' };

test.describe('好友模块', () => {
  test.beforeAll(async ({ request }) => {
    // 确保两个账号存在（API 准备数据，比 UI 快）
    await apiRegisterAndLogin(request, ALICE);
    await apiRegisterAndLogin(request, BOB);
  });

  test('加好友全流程：搜索 → 申请 → 实时通知 → 同意 → 双方好友列表可见', async ({ browser }) => {
    const ctxA = await browser.newContext();
    const ctxB = await browser.newContext();
    const alice = await ctxA.newPage();
    const bob = await ctxB.newPage();

    await loginViaUI(bob, BOB.email, BOB.password);     // Bob 先在线，等待实时通知
    await loginViaUI(alice, ALICE.email, ALICE.password);

    // Alice 搜索 Bob 并发申请（prompt 留言对话框自动确认）
    alice.on('dialog', d => d.accept('我是 Alice'));
    await alice.locator('#btn-search').click();
    await alice.locator('#search-user-input').fill('Bob');
    await alice.locator('#search-user-btn').click();
    await alice.locator('[data-add-friend]').first().click();
    await expect(alice.getByText('好友申请已发送')).toBeVisible();

    // Bob 实时收到 notify_apply（toast + 红点）
    await expect(bob.getByText(/收到新的.*申请/)).toBeVisible({ timeout: 10_000 });

    // Bob 打开通知面板同意
    await bob.locator('#btn-notify').click();
    await bob.locator('[data-friend-accept]').first().click();
    await expect(bob.getByText('已同意好友申请')).toBeVisible();
    await bob.keyboard.press('Escape');

    // 双方好友列表都出现对方
    await expect(bob.locator('#friend-list')).toContainText('Alice');
    await alice.reload();   // Alice 端刷新（或等 notify_apply_result 自动刷新）
    await expect(alice.locator('#friend-list')).toContainText('Bob', { timeout: 10_000 });

    await ctxA.close(); await ctxB.close();
  });

  test('在线状态：Bob 下线后 Alice 端 presence 实时变更', async ({ browser }) => {
    const ctxA = await browser.newContext();
    const ctxB = await browser.newContext();
    const alice = await ctxA.newPage();
    const bob = await ctxB.newPage();

    await loginViaUI(alice, ALICE.email, ALICE.password);
    await loginViaUI(bob, BOB.email, BOB.password);

    // Bob 在线：Alice 好友列表显示绿色在线点
    const bobItem = alice.locator('#friend-list .list-item', { hasText: 'Bob' });
    await expect(bobItem.locator('.dot.online')).toBeVisible({ timeout: 10_000 });

    // Bob 关闭页面（WS 断开）→ Alice 端收到 presence offline
    await ctxB.close();
    await expect(bobItem.locator('.dot.online')).toBeHidden({ timeout: 15_000 });

    await ctxA.close();
  });

  test('删除好友：双向解除', async ({ browser }) => {
    const ctx = await browser.newContext();
    const alice = await ctx.newPage();
    await loginViaUI(alice, ALICE.email, ALICE.password);

    alice.on('dialog', d => d.accept());
    await alice.locator('#friend-list .list-item', { hasText: 'Bob' }).click();
    await alice.locator('#btn-conv-action').click();
    await alice.locator('#conv-del-friend').click();
    await expect(alice.getByText('已删除好友')).toBeVisible();
    await expect(alice.locator('#friend-list')).not.toContainText('Bob');

    await ctx.close();
  });
});
```

### 4.3 群聊模块（03-group.spec.js）

覆盖 SPEC 验收 6.3：建群、搜群、加群审批、成员列表、退群、注销群。

```js
const { test, expect } = require('@playwright/test');
const { loginViaUI, apiRegisterAndLogin } = require('../helpers/utils');

const ALICE = { email: 'alice@example.com', password: 'alice123', nickname: 'Alice' };
const BOB   = { email: 'bob@example.com',   password: 'bob123456', nickname: 'Bob' };
const GROUP = 'E2E测试群-' + Date.now();   // 唯一群名避免重复跑冲突

test.describe('群聊模块', () => {
  test('建群 → Bob 搜群申请 → 群主审批 → 成员列表 → Bob 退群 → 群主注销', async ({ browser }) => {
    const ctxA = await browser.newContext();
    const ctxB = await browser.newContext();
    const alice = await ctxA.newPage();
    const bob = await ctxB.newPage();
    await loginViaUI(alice, ALICE.email, ALICE.password);
    await loginViaUI(bob, BOB.email, BOB.password);

    // 1) Alice 建群（自动成为群主）
    await alice.locator('#btn-search').click();
    await alice.locator('#search-tab-create').click();
    await alice.locator('#create-group-name').fill(GROUP);
    await alice.locator('#create-group-btn').click();
    await expect(alice.getByText('群创建成功')).toBeVisible();
    await expect(alice.locator('#group-list')).toContainText(GROUP);
    await expect(alice.locator('#group-list .owner-tag')).toBeVisible(); // 群主标识

    // 2) Bob 搜群并申请加入
    await bob.locator('#btn-search').click();
    await bob.locator('#search-tab-group').click();
    await bob.locator('#search-group-input').fill(GROUP);
    await bob.locator('#search-group-btn').click();
    await bob.locator('[data-join-group]').first().click();
    await expect(bob.getByText('加群申请已发送')).toBeVisible();
    await bob.keyboard.press('Escape');

    // 3) 群主 Alice 收到通知并同意
    await expect(alice.getByText(/收到新的.*申请/)).toBeVisible({ timeout: 10_000 });
    await alice.locator('#btn-notify').click();
    await alice.locator('#notify-tab-group').click();
    await alice.locator('[data-group-accept]').first().click();
    await expect(alice.getByText('已同意加群申请')).toBeVisible();
    await alice.keyboard.press('Escape');

    // 4) Bob 端群列表出现该群（notify_apply_result 触发刷新）
    await bob.locator('#tab-groups').click();
    await expect(bob.locator('#group-list')).toContainText(GROUP, { timeout: 10_000 });

    // 5) 群成员列表：群主在前，共 2 人
    await alice.locator('#tab-groups').click();
    await alice.locator('#group-list .list-item', { hasText: GROUP }).click();
    await alice.locator('#btn-conv-action').click();
    await expect(alice.locator('#modal-conv-body')).toContainText('成员（2）');
    await alice.keyboard.press('Escape');

    // 6) Bob 退群
    bob.on('dialog', d => d.accept());
    await bob.locator('#group-list .list-item', { hasText: GROUP }).click();
    await bob.locator('#btn-conv-action').click();
    await bob.locator('#conv-leave').click();
    await expect(bob.getByText('已退群')).toBeVisible();

    // 7) 群主注销群（硬删除）
    alice.on('dialog', d => d.accept());
    await alice.locator('#group-list .list-item', { hasText: GROUP }).click();
    await alice.locator('#btn-conv-action').click();
    await alice.locator('#conv-dissolve').click();
    await expect(alice.getByText('群已注销')).toBeVisible();
    await expect(alice.locator('#group-list')).not.toContainText(GROUP);

    await ctxA.close(); await ctxB.close();
  });
});
```

### 4.4 消息模块（04-message.spec.js）

覆盖 SPEC 验收 6.4：实时单聊、ack、历史分页滚动加载、离线消息。
前置：Alice 与 Bob 互为好友（可在 beforeAll 用 API 重新加回）。

```js
const { test, expect } = require('@playwright/test');
const { loginViaUI, apiRegisterAndLogin } = require('../helpers/utils');

const BASE = process.env.BASE_URL || 'http://localhost:8080';
const ALICE = { email: 'alice@example.com', password: 'alice123', nickname: 'Alice' };
const BOB   = { email: 'bob@example.com',   password: 'bob123456', nickname: 'Bob' };

test.describe('消息模块', () => {
  test.beforeAll(async ({ request }) => {
    // API 准备：确保互为好友（申请 + 对方 token 同意）
    const a = await apiRegisterAndLogin(request, ALICE);
    const b = await apiRegisterAndLogin(request, BOB);
    const req = await request.post(`${BASE}/api/friend/request`, {
      headers: { Authorization: `Bearer ${a.token}` },
      data: { userId: b.userId, message: 'e2e' },
    });
    const rj = await req.json();
    if (rj.code === 0) {
      const list = await (await request.get(`${BASE}/api/friend/requests`, {
        headers: { Authorization: `Bearer ${b.token}` },
      })).json();
      const id = list.data.list.find(x => x.fromUser === a.userId)?.id;
      if (id) await request.post(`${BASE}/api/friend/accept/${id}`, {
        headers: { Authorization: `Bearer ${b.token}` },
      });
    } // code=3001 已是好友则跳过
  });

  test('实时单聊：Alice 发送 → ack 确认 → Bob 实时收到', async ({ browser }) => {
    const ctxA = await browser.newContext();
    const ctxB = await browser.newContext();
    const alice = await ctxA.newPage();
    const bob = await ctxB.newPage();
    await loginViaUI(alice, ALICE.email, ALICE.password);
    await loginViaUI(bob, BOB.email, BOB.password);

    const text = 'E2E 单聊消息 ' + Date.now();

    // 双方都打开对方会话
    await alice.locator('#friend-list .list-item', { hasText: 'Bob' }).click();
    await bob.locator('#friend-list .list-item', { hasText: 'Alice' }).click();

    await alice.locator('#msg-input').fill(text);
    await alice.locator('#btn-send').click();

    // 发送方：气泡出现且 ack 后去掉 pending 半透明态
    const myBubble = alice.locator('.msg-row.mine', { hasText: text });
    await expect(myBubble).toBeVisible();
    await expect(myBubble).not.toHaveClass(/pending/, { timeout: 10_000 });

    // 接收方：实时收到
    await expect(bob.locator('#msg-list')).toContainText(text, { timeout: 10_000 });

    await ctxA.close(); await ctxB.close();
  });

  test('离线消息：Bob 离线时收消息，上线后自动补收', async ({ browser, request }) => {
    const a = await apiRegisterAndLogin(request, ALICE);
    const b = await apiRegisterAndLogin(request, BOB);
    void a; void b;

    // Bob 不在线；Alice 通过 UI 发消息
    const ctxA = await browser.newContext();
    const alice = await ctxA.newPage();
    await loginViaUI(alice, ALICE.email, ALICE.password);
    const text = 'E2E 离线消息 ' + Date.now();
    await alice.locator('#friend-list .list-item', { hasText: 'Bob' }).click();
    await alice.locator('#msg-input').fill(text);
    await alice.locator('#btn-send').click();
    await expect(alice.locator('.msg-row.mine', { hasText: text })).not.toHaveClass(/pending/, { timeout: 10_000 });

    // Bob 此后上线 → 收到离线消息 toast，打开会话能看到该消息
    const ctxB = await browser.newContext();
    const bob = await ctxB.newPage();
    await loginViaUI(bob, BOB.email, BOB.password);
    await expect(bob.getByText(/收到 \d+ 条离线消息/)).toBeVisible({ timeout: 10_000 });
    await bob.locator('#friend-list .list-item', { hasText: 'Alice' }).click();
    await expect(bob.locator('#msg-list')).toContainText(text, { timeout: 10_000 });

    await ctxA.close(); await ctxB.close();
  });

  test('历史分页：注水 40 条消息后向上滚动加载更早消息', async ({ browser, request }) => {
    // 用 API+WS 注水成本高，这里直接通过 UI 连发也可；为速度用页面内 WS 批量发
    const ctxA = await browser.newContext();
    const alice = await ctxA.newPage();
    await loginViaUI(alice, ALICE.email, ALICE.password);
    await alice.locator('#friend-list .list-item', { hasText: 'Bob' }).click();

    // 连发 40 条（首页默认 30 条，保证有第二页）
    for (let i = 1; i <= 40; i++) {
      await alice.locator('#msg-input').fill(`历史消息-${i}`);
      await alice.locator('#btn-send').click();
    }
    // 等最后一条 ack
    await expect(alice.locator('.msg-row.mine', { hasText: '历史消息-40' }))
      .not.toHaveClass(/pending/, { timeout: 10_000 });

    // 重开会话：只加载最近 30 条 → 第 1 条不可见
    await alice.reload();
    await alice.locator('#friend-list .list-item', { hasText: 'Bob' }).click();
    await expect(alice.locator('#msg-list')).toContainText('历史消息-40');

    // 滚动到顶部触发 beforeSeq 游标加载
    await alice.locator('#msg-scroll').evaluate(el => { el.scrollTop = 0; });
    await expect(alice.locator('#msg-list')).toContainText('历史消息-1', { timeout: 10_000 });

    await ctxA.close();
  });
});
```

### 4.5 超管管理面板（05-admin.spec.js）

覆盖 SPEC 验收 6.6：超管查看全部群、注销任意群、普通用户越权 403。
**密码经环境变量 `PASSWORD` 注入，不写死在代码中。**

```js
const { test, expect } = require('@playwright/test');
const { loginViaUI } = require('../helpers/utils');

const ADMIN_EMAIL = process.env.ADMIN_EMAIL || 'admin@online-chat.local';
const ADMIN_PASSWORD = process.env.PASSWORD;   // 运行时注入：PASSWORD=admin123 npx playwright test

test.describe('超管管理面板', () => {
  test.skip(!ADMIN_PASSWORD, '需设置环境变量 PASSWORD');

  test('超管登录后侧栏出现管理入口，面板可分页查看全部群', async ({ page }) => {
    await loginViaUI(page, ADMIN_EMAIL, ADMIN_PASSWORD);
    await page.locator('button[aria-label="管理面板"]').click();
    await page.waitForURL('**/admin.html');
    // 表格加载出数据或显示"暂无群聊"
    await expect(page.locator('#group-table-body tr').first()).not.toContainText('加载中', { timeout: 10_000 });
    await expect(page.locator('#total-info')).toContainText(/共 \d+ 个群/);
  });

  test('超管可注销任意群', async ({ page, request, browser }) => {
    // 先用普通用户 API 建一个待注销的群
    const { apiRegisterAndLogin } = require('../helpers/utils');
    const a = await apiRegisterAndLogin(request, {
      email: 'alice@example.com', password: 'alice123', nickname: 'Alice',
    });
    const name = '待注销群-' + Date.now();
    await request.post('http://localhost:8080/api/group/create', {
      headers: { Authorization: `Bearer ${a.token}` }, data: { name },
    });

    await loginViaUI(page, ADMIN_EMAIL, ADMIN_PASSWORD);
    await page.goto('/admin.html');
    page.on('dialog', d => d.accept());
    const row = page.locator('#group-table-body tr', { hasText: name });
    await row.locator('[data-del]').click();
    await expect(page.getByText('群已注销')).toBeVisible();
    await expect(page.locator('#group-table-body')).not.toContainText(name);
  });

  test('普通用户直接访问 admin.html 被拒（403 提示）', async ({ page }) => {
    await loginViaUI(page, 'alice@example.com', 'alice123');
    await page.goto('/admin.html');
    await expect(page.locator('#group-table-body'))
      .toContainText(/权限不足|非超管/, { timeout: 10_000 });
  });
});
```

---

## 五、运行与调试

```bash
cd ~/online-chat/tests/e2e

# 全量运行（无头模式）；管理面板用例需要 PASSWORD
PASSWORD=admin123 npx playwright test

# 只跑某个模块
npx playwright test specs/01-auth.spec.js

# 有头模式（看着浏览器跑，联调排错首选）
PASSWORD=admin123 npx playwright test --headed

# 单步调试（Playwright Inspector，可逐步执行、拾取选择器）
npx playwright test specs/02-friend.spec.js --debug

# 失败后查看 HTML 报告（含截图/视频/trace 回放）
npx playwright show-report

# Redis 在 Docker 中时，验证码读取走 docker exec
REDIS_IN_DOCKER=1 PASSWORD=admin123 npx playwright test
```

调试技巧：

| 手段 | 用途 |
|---|---|
| `npx playwright codegen http://localhost:8080` | 录制操作自动生成测试代码、拾取选择器 |
| `page.pause()` | 在用例中插入断点，弹出 Inspector |
| `trace: 'retain-on-failure'` | 失败用例可时间轴回放每一步 DOM/网络/控制台 |
| `page.on('console', m => console.log(m.text()))` | 透出页面 console（看 WS 收发日志） |
| `page.on('websocket', ws => ws.on('framereceived', f => console.log(f.payload)))` | 直接监听 WS 帧，验证信封格式 |

---

## 六、联调步骤清单（对应 SPEC 阶段 8）

按顺序执行，全部通过即完成端到端联调验收：

| 步骤 | 命令/操作 | 验收点（对应 SPEC 六） |
|---|---|---|
| 1 | `docker compose up -d` + `curl localhost:8080/index.html` → 200 | 6.7 一键拉起 |
| 2 | `npx playwright test specs/01-auth.spec.js` | 6.1 验证码/注册/登录/限频/挤下线 |
| 3 | `npx playwright test specs/02-friend.spec.js` | 6.2 搜索/实时申请/审批/presence/删好友 |
| 4 | `npx playwright test specs/03-group.spec.js` | 6.3 建群/审批/成员数/退群/注销硬删 |
| 5 | `npx playwright test specs/04-message.spec.js` | 6.4 实时收发/ack/seq 有序/离线/历史分页 |
| 6 | `PASSWORD=admin123 npx playwright test specs/05-admin.spec.js` | 6.6 超管面板/越权 403 |
| 7 | `npx playwright show-report` 归档报告 | 8.1~8.3 交付物 |

异常路径补充验证（已包含在上述用例中）：

- **挤下线**：01-auth「单点登录」用例（双 context 同账号）。
- **离线消息不丢**：04-message「离线消息」用例。
- **权限越权**：05-admin「普通用户访问 admin.html」用例；群模块中非群主无审批按钮。

---

## 七、常见问题

| 问题 | 原因与解决 |
|---|---|
| `curl localhost:8080` 返回 502/拒绝连接 | 后端未启动；`docker compose ps` 检查 oc-app 状态，看 `docker compose logs oc-app` |
| 取不到验证码 | ① Redis 不在本机 → 设 `REDIS_IN_DOCKER=1`；② 60s 限频未过 → 用例前调用 `clearCodeLimit()`；③ 后端发信失败未写 Redis → 接 MailHog |
| 第二次跑测试注册报 2003 | 邮箱已注册属预期；`apiRegisterAndLogin` 已兼容（注册失败继续登录）。需要全新环境时重建库：`docker compose down -v && docker compose up -d` |
| 双 context 用例互相挤下线 | 必须用**不同账号**开双端；同账号双登录是单点登录用例专属场景。`workers: 1` 串行也是为此 |
| WS 用例偶发超时 | 实时推送断言统一给 10s 超时；检查 presence TTL（60s）与心跳间隔（前端 25s） |
| `npx playwright install` 缺系统库 | 加 `--with-deps` 让 Playwright 自动 `apt install` 浏览器依赖 |
| 测试残留脏数据 | 群名/消息内容统一带 `Date.now()` 后缀隔离；或每轮联调前 `down -v` 重置 |

---

> 本文档与 `项目接口测试-curl.md` 互补：先用 curl 验证单接口正确性，
> 再用 Playwright 验证 UI 全链路与实时双端场景，二者全部通过即可勾选 SPEC 阶段 8。
