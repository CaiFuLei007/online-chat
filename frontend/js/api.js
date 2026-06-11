/* api.js — HTTP 接口封装（对应 API.md）
 * 所有受保护接口自动携带 Authorization: Bearer <token>
 */
(function () {
  'use strict';

  const BASE_URL = '';

  function getToken() { return localStorage.getItem('token') || ''; }
  function setSession(data) {
    localStorage.setItem('token', data.token);
    localStorage.setItem('userId', String(data.userId));
    localStorage.setItem('nickname', data.nickname || '');
    localStorage.setItem('role', String(data.role ?? 0));
  }
  function clearSession() {
    ['token', 'userId', 'nickname', 'role'].forEach(k => localStorage.removeItem(k));
  }

  async function request(method, path, body, opts = {}) {
    const headers = { ...(opts.headers || {}) };
    if (body !== undefined) headers['Content-Type'] = 'application/json';
    if (!opts.noAuth) {
      // Auth 占位符：受保护接口统一携带 JWT
      headers['Authorization'] = 'Bearer ' + (opts.token || getToken());
    }

    let res;
    try {
      res = await fetch(BASE_URL + path, {
        method,
        headers,
        body: body !== undefined ? JSON.stringify(body) : undefined,
      });
    } catch (e) {
      throw { code: -1, msg: '网络错误，请检查连接' };
    }

    let json;
    try { json = await res.json(); }
    catch (e) { throw { code: -1, msg: '服务器响应异常 (HTTP ' + res.status + ')' }; }

    if (json.code !== 0) {
      if (json.code === 1002 && !opts.noKick) {
        clearSession();
        if (!location.pathname.endsWith('index.html') && location.pathname !== '/') {
          location.href = 'index.html';
        }
      }
      throw json;
    }
    return json.data;
  }

  window.API = {
    getToken, setSession, clearSession,
    get: (path, opts) => request('GET', path, undefined, opts),
    post: (path, body, opts) => request('POST', path, body, opts),
    del: (path, opts) => request('DELETE', path, undefined, opts),

    // ---- 认证（无需鉴权） ----
    sendCode: (email) => request('POST', '/api/auth/send-code', { email }, { noAuth: true }),
    register: (payload) => request('POST', '/api/auth/register', payload, { noAuth: true }),
    login: (email, password) => request('POST', '/api/auth/login', { email, password }, { noAuth: true }),

    // ---- 用户 ----
    profile: () => request('GET', '/api/user/profile'),
    profileOf: (id) => request('GET', '/api/user/profile/' + id),
    searchUsers: (keyword, page = 1) =>
      request('GET', '/api/user/search?keyword=' + encodeURIComponent(keyword) + '&page=' + page),

    // ---- 好友 ----
    friendRequest: (userId, message) => request('POST', '/api/friend/request', { userId, message }),
    friendRequests: () => request('GET', '/api/friend/requests'),
    friendAccept: (id) => request('POST', '/api/friend/accept/' + id),
    friendReject: (id) => request('POST', '/api/friend/reject/' + id),
    friendList: () => request('GET', '/api/friend/list'),
    friendDelete: (id) => request('DELETE', '/api/friend/' + id),

    // ---- 群聊 ----
    groupCreate: (name) => request('POST', '/api/group/create', { name }),
    searchGroups: (keyword, page = 1) =>
      request('GET', '/api/group/search?keyword=' + encodeURIComponent(keyword) + '&page=' + page),
    groupJoin: (groupId) => request('POST', '/api/group/join/' + groupId),
    groupRequests: (groupId) => request('GET', '/api/group/requests/' + groupId),
    groupAccept: (requestId) => request('POST', '/api/group/accept/' + requestId),
    groupReject: (requestId) => request('POST', '/api/group/reject/' + requestId),
    groupLeave: (groupId) => request('POST', '/api/group/leave/' + groupId),
    groupMembers: (groupId) => request('GET', '/api/group/members/' + groupId),
    groupDelete: (groupId) => request('DELETE', '/api/group/' + groupId),
    myGroups: () => request('GET', '/api/group/my'),

    // ---- 消息 ----
    singleHistory: (peerId, beforeSeq = 0, limit = 30) =>
      request('GET', `/api/message/single/history?peerId=${peerId}&beforeSeq=${beforeSeq}&limit=${limit}`),
    groupHistory: (groupId, beforeSeq = 0, limit = 30) =>
      request('GET', `/api/message/group/history?groupId=${groupId}&beforeSeq=${beforeSeq}&limit=${limit}`),
    offlineMessages: () => request('GET', '/api/message/offline'),

    // ---- 管理（需超管 role=1，Authorization 头同上为 Auth 占位符） ----
    adminGroups: (page = 1, opts) => request('GET', '/api/admin/groups?page=' + page, opts),
    adminDeleteGroup: (groupId, opts) => request('DELETE', '/api/admin/groups/' + groupId, opts),
  };
})();
