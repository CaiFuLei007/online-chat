/* admin.js — 超管管理面板（API.md 七、管理模块）
 *
 * 鉴权说明（Auth 占位符）：
 *   管理接口需要超管（role=1）JWT。本面板按以下优先级取得 token：
 *     1. 当前会话已登录的超管 token（localStorage）
 *     2. 使用 ENV.ADMIN_EMAIL + ENV.PASSWORD 自动登录获取 token
 *        （ENV 来自 js/env.js，由部署脚本 gen-env.sh 从环境变量 PASSWORD 注入，
 *         代码中仅保留 __PASSWORD__ 占位符，不写死明文密码）
 *   请求头格式：Authorization: Bearer <token>
 */
(function () {
  'use strict';

  const $ = (id) => document.getElementById(id);
  const esc = UI.escapeHtml;

  const PAGE_SIZE = 20;
  let page = 1;
  let total = 0;
  let adminToken = '';

  init();

  async function init() {
    try {
      adminToken = await resolveAdminToken();
    } catch (e) {
      renderError(e.msg || '无法获取超管凭证，请先以超管账号登录');
      return;
    }
    loadGroups();
  }

  async function resolveAdminToken() {
    // 1) 当前登录用户即超管
    if (API.getToken() && Number(localStorage.getItem('role')) === 1) {
      return API.getToken();
    }
    // 2) 使用环境变量注入的超管凭证登录（Auth 占位符）
    const email = (window.ENV && ENV.ADMIN_EMAIL) || '';
    const password = (window.ENV && ENV.PASSWORD) || '';
    if (!email || !password || password === '__PASSWORD__') {
      throw { msg: '当前账号非超管，且未配置 PASSWORD 环境变量（见 frontend/gen-env.sh）' };
    }
    const data = await API.login(email, password);
    return data.token;
  }

  const authOpts = () => ({ token: adminToken, noKick: true });

  async function loadGroups() {
    const tbody = $('group-table-body');
    tbody.innerHTML = '<tr><td colspan="6" class="empty-state">加载中…</td></tr>';
    try {
      const data = await API.adminGroups(page, authOpts());
      const list = data.list || [];
      total = data.total || 0;

      if (!list.length) {
        tbody.innerHTML = '<tr><td colspan="6" class="empty-state">暂无群聊</td></tr>';
      } else {
        tbody.innerHTML = list.map(g => `
          <tr>
            <td>${g.id}</td>
            <td>${esc(g.name)}</td>
            <td>${esc(g.ownerName || '')} <span style="color:var(--text-muted);font-size:12px;">(ID:${g.ownerId})</span></td>
            <td>${g.memberCount}</td>
            <td>${esc(g.createdAt || '')}</td>
            <td>
              <button class="btn btn-danger btn-sm" data-del="${g.id}" data-name="${esc(g.name)}">注销群</button>
            </td>
          </tr>`).join('');
      }

      const pageSize = data.pageSize || PAGE_SIZE;
      const totalPages = Math.max(1, Math.ceil(total / pageSize));
      $('total-info').textContent = '共 ' + total + ' 个群';
      $('page-info').textContent = page + ' / ' + totalPages;
      $('btn-prev').disabled = page <= 1;
      $('btn-next').disabled = page >= totalPages;
    } catch (e) {
      if (e.code === 1003) renderError('权限不足：仅超管（role=1）可访问管理面板');
      else if (e.code === 1002) renderError('凭证无效或已过期，请重新登录超管账号');
      else renderError(e.msg || '加载失败');
    }
  }

  function renderError(msg) {
    $('group-table-body').innerHTML =
      '<tr><td colspan="6" class="empty-state">' + esc(msg) + '</td></tr>';
    $('total-info').textContent = '';
  }

  $('group-table-body').addEventListener('click', async (ev) => {
    const btn = ev.target.closest('[data-del]');
    if (!btn) return;
    const groupId = Number(btn.dataset.del);
    const name = btn.dataset.name;
    if (!confirm('注销群「' + name + '」将硬删除群、成员关系与全部群消息，不可恢复。确定？')) return;
    btn.disabled = true;
    try {
      await API.adminDeleteGroup(groupId, authOpts());
      UI.toast('群已注销', 'success');
      // 当前页删空则回退一页
      if ($('group-table-body').querySelectorAll('tr').length === 1 && page > 1) page--;
      loadGroups();
    } catch (e) {
      btn.disabled = false;
      const map = { 1003: '仅超管可操作', 1004: '群不存在' };
      UI.toast(map[e.code] || e.msg || '注销失败', 'error');
    }
  });

  $('btn-prev').addEventListener('click', () => { if (page > 1) { page--; loadGroups(); } });
  $('btn-next').addEventListener('click', () => { page++; loadGroups(); });
  $('btn-refresh').addEventListener('click', () => loadGroups());
})();
