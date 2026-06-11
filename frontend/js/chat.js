/* chat.js — 主界面逻辑
 * 覆盖：好友/群列表、搜索添加、单聊/群聊、历史滚动加载、
 *       申请通知与审批、在线状态、离线消息、挤下线
 */
(function () {
  'use strict';

  if (!API.getToken()) { location.href = 'index.html'; return; }

  const myId = Number(localStorage.getItem('userId'));
  const myNickname = localStorage.getItem('nickname') || '';
  const myRole = Number(localStorage.getItem('role') || 0);

  const $ = (id) => document.getElementById(id);
  const esc = UI.escapeHtml;

  // ---------- 全局状态 ----------
  const state = {
    friends: [],          // {id, nickname, role, online}
    groups: [],           // {id, name, memberCount, role}
    conv: null,           // {type:'single'|'group', id, name}
    oldestSeq: 0,
    hasMore: false,
    loadingHistory: false,
    friendReqs: [],
    groupReqs: [],        // 聚合各群的待处理申请 {id, groupId, groupName, fromUser, nickname, createdAt}
    nameCache: new Map(), // userId -> nickname
  };

  const ws = new WsClient();

  // ---------- 初始化 ----------
  $('me-avatar').textContent = UI.avatarText(myNickname);
  $('me-name').textContent = myNickname;

  init();

  async function init() {
    try {
      const p = await API.profile();
      $('me-email').textContent = p.user.email;
      $('me-name').textContent = p.user.nickname;
      $('me-avatar').textContent = UI.avatarText(p.user.nickname);
      if (p.user.role === 1) addAdminEntry();
    } catch (e) { /* 1002 已在 api.js 统一跳转 */ }

    await Promise.all([loadFriends(), loadGroups(), loadNotifications()]);
    setupWs();
    ws.connect();
  }

  function addAdminEntry() {
    const header = document.querySelector('.sidebar-header');
    const btn = document.createElement('button');
    btn.className = 'icon-btn';
    btn.title = '管理面板';
    btn.setAttribute('aria-label', '管理面板');
    btn.innerHTML = '<svg class="icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M12 22s8-4 8-10V5l-8-3-8 3v7c0 6 8 10 8 10"/></svg>';
    btn.addEventListener('click', () => { location.href = 'admin.html'; });
    header.insertBefore(btn, $('btn-logout'));
  }

  // ---------- 好友 / 群列表 ----------
  async function loadFriends() {
    try {
      const data = await API.friendList();
      state.friends = data.list || [];
      state.friends.forEach(f => state.nameCache.set(f.id, f.nickname));
      renderFriendList();
    } catch (e) { UI.toast(e.msg || '好友列表加载失败', 'error'); }
  }

  async function loadGroups() {
    try {
      const data = await API.myGroups();
      state.groups = data.list || [];
      renderGroupList();
    } catch (e) { UI.toast(e.msg || '群列表加载失败', 'error'); }
  }

  function renderFriendList() {
    const box = $('friend-list');
    if (!state.friends.length) {
      box.innerHTML = '<div class="empty-state">暂无好友，点击右上角搜索添加</div>';
      return;
    }
    box.innerHTML = state.friends.map(f => `
      <button class="list-item${isActive('single', f.id) ? ' active' : ''}" role="listitem"
              data-type="single" data-id="${f.id}" data-name="${esc(f.nickname)}">
        <span class="avatar">${UI.avatarText(f.nickname)}</span>
        <span class="item-main">
          <span class="item-name">${esc(f.nickname)} <span class="dot${f.online ? ' online' : ''}" title="${f.online ? '在线' : '离线'}"></span></span>
          <span class="item-sub">${f.online ? '在线' : '离线'}</span>
        </span>
      </button>`).join('');
  }

  function renderGroupList() {
    const box = $('group-list');
    if (!state.groups.length) {
      box.innerHTML = '<div class="empty-state">暂未加入群聊，点击右上角搜索或创建</div>';
      return;
    }
    box.innerHTML = state.groups.map(g => `
      <button class="list-item${isActive('group', g.id) ? ' active' : ''}" role="listitem"
              data-type="group" data-id="${g.id}" data-name="${esc(g.name)}">
        <span class="avatar avatar-group">${UI.avatarText(g.name)}</span>
        <span class="item-main">
          <span class="item-name">${esc(g.name)} ${g.role === 1 ? '<span class="owner-tag">群主</span>' : ''}</span>
          <span class="item-sub">${g.memberCount} 名成员</span>
        </span>
      </button>`).join('');
  }

  function isActive(type, id) {
    return state.conv && state.conv.type === type && state.conv.id === id;
  }

  // 侧栏 tab 切换
  $('tab-friends').addEventListener('click', () => switchSideTab(true));
  $('tab-groups').addEventListener('click', () => switchSideTab(false));
  function switchSideTab(friends) {
    $('tab-friends').classList.toggle('active', friends);
    $('tab-groups').classList.toggle('active', !friends);
    $('tab-friends').setAttribute('aria-selected', String(friends));
    $('tab-groups').setAttribute('aria-selected', String(!friends));
    $('friend-list').hidden = !friends;
    $('group-list').hidden = friends;
  }

  // 列表点击 → 打开会话（事件委托）
  ['friend-list', 'group-list'].forEach(id => {
    $(id).addEventListener('click', (ev) => {
      const item = ev.target.closest('.list-item');
      if (!item) return;
      openConv(item.dataset.type, Number(item.dataset.id), item.dataset.name);
    });
  });

  // ---------- 会话 ----------
  async function openConv(type, id, name) {
    state.conv = { type, id, name };
    state.oldestSeq = 0;
    state.hasMore = false;
    renderFriendList();
    renderGroupList();

    $('chat-welcome').hidden = true;
    $('chat-pane').hidden = false;
    $('chat-title-name').textContent = name;
    $('msg-list').innerHTML = '';
    document.body.classList.add('chat-open');
    updateConvStatus();

    await loadHistory(true);
    $('msg-input').focus();
  }

  function updateConvStatus() {
    if (!state.conv) return;
    const el = $('chat-title-status');
    if (state.conv.type === 'single') {
      const f = state.friends.find(x => x.id === state.conv.id);
      el.textContent = f ? (f.online ? '在线' : '离线') : '';
    } else {
      const g = state.groups.find(x => x.id === state.conv.id);
      el.textContent = g ? g.memberCount + ' 名成员' : '';
    }
  }

  $('btn-back').addEventListener('click', () => {
    document.body.classList.remove('chat-open');
    state.conv = null;
    $('chat-pane').hidden = true;
    $('chat-welcome').hidden = false;
    renderFriendList();
    renderGroupList();
  });

  // ---------- 历史消息（游标分页） ----------
  async function loadHistory(initial) {
    if (state.loadingHistory || !state.conv) return;
    state.loadingHistory = true;
    $('msg-loading').hidden = false;

    const conv = state.conv;
    const scroller = $('msg-scroll');
    const prevHeight = scroller.scrollHeight;

    try {
      const data = conv.type === 'single'
        ? await API.singleHistory(conv.id, state.oldestSeq)
        : await API.groupHistory(conv.id, state.oldestSeq);
      if (!state.conv || state.conv.type !== conv.type || state.conv.id !== conv.id) return;

      const list = (data.list || []).slice().sort((a, b) => a.seq - b.seq); // 接口最新在前 → 升序
      state.hasMore = !!data.hasMore;
      if (list.length) state.oldestSeq = list[0].seq;

      const html = list.map(m => msgHtml(m)).join('');
      $('msg-list').insertAdjacentHTML('afterbegin', html);

      if (initial) {
        scroller.scrollTop = scroller.scrollHeight;
      } else {
        scroller.scrollTop = scroller.scrollHeight - prevHeight; // 保持视口位置
      }
      // 群聊需要补充发送者昵称
      fillUnknownNames(list);
    } catch (e) {
      UI.toast(e.msg || '历史消息加载失败', 'error');
    } finally {
      state.loadingHistory = false;
      $('msg-loading').hidden = true;
    }
  }

  // 向上滚动加载更早消息
  $('msg-scroll').addEventListener('scroll', () => {
    if ($('msg-scroll').scrollTop < 60 && state.hasMore && !state.loadingHistory) {
      loadHistory(false);
    }
  });

  function msgHtml(m, pending) {
    const mine = m.from === myId;
    const name = mine ? myNickname : (state.nameCache.get(m.from) || ('用户' + m.from));
    const showSender = state.conv && state.conv.type === 'group' && !mine;
    return `
      <div class="msg-row${mine ? ' mine' : ''}${pending ? ' pending' : ''}" data-seq="${m.seq || ''}" ${pending ? `data-local="${pending}"` : ''}>
        <span class="avatar" data-uid="${m.from}">${UI.avatarText(name)}</span>
        <div class="msg-body">
          ${showSender ? `<span class="msg-sender" data-uid="${m.from}">${esc(name)}</span>` : ''}
          <div class="msg-bubble">${esc(m.content)}</div>
          <span class="msg-time">${UI.formatTime(m.createdAt)}</span>
        </div>
      </div>`;
  }

  async function fillUnknownNames(list) {
    const unknown = [...new Set(list.map(m => m.from))]
      .filter(uid => uid !== myId && !state.nameCache.has(uid));
    for (const uid of unknown) {
      try {
        const data = await API.profileOf(uid);
        state.nameCache.set(uid, data.user.nickname);
      } catch (e) { state.nameCache.set(uid, '用户' + uid); }
      const nick = state.nameCache.get(uid);
      document.querySelectorAll(`.msg-sender[data-uid="${uid}"]`)
        .forEach(el => { el.textContent = nick; });
      document.querySelectorAll(`.avatar[data-uid="${uid}"]`)
        .forEach(el => { el.textContent = UI.avatarText(nick); });
    }
  }

  // ---------- 发送消息 ----------
  const input = $('msg-input');
  input.addEventListener('keydown', (ev) => {
    if (ev.key === 'Enter' && !ev.shiftKey) {
      ev.preventDefault();
      sendMessage();
    }
  });
  $('btn-send').addEventListener('click', sendMessage);

  let localMsgId = 0;

  async function sendMessage() {
    const content = input.value.trim();
    if (!content || !state.conv) return;
    if (!ws.connected) { UI.toast('连接已断开，请稍候重试', 'error'); return; }

    const conv = state.conv;
    const localId = 'local-' + (++localMsgId);
    input.value = '';
    input.style.height = '';

    appendMsg({ from: myId, content, createdAt: nowStr() }, localId);

    try {
      const type = conv.type === 'single' ? 'chat_single' : 'chat_group';
      const data = conv.type === 'single'
        ? { to: conv.id, content }
        : { groupId: conv.id, content };
      const ack = await ws.sendWithAck(type, data);
      const row = document.querySelector(`[data-local="${localId}"]`);
      if (row) {
        row.classList.remove('pending');
        if (ack && ack.seq) row.dataset.seq = ack.seq;
      }
    } catch (e) {
      const row = document.querySelector(`[data-local="${localId}"]`);
      if (row) { row.classList.remove('pending'); row.classList.add('failed'); }
      UI.toast(e.msg || '消息发送失败', 'error');
    }
  }

  function appendMsg(m, pendingLocalId) {
    const scroller = $('msg-scroll');
    const nearBottom = scroller.scrollHeight - scroller.scrollTop - scroller.clientHeight < 120;
    $('msg-list').insertAdjacentHTML('beforeend', msgHtml(m, pendingLocalId));
    if (m.from === myId || nearBottom) scroller.scrollTop = scroller.scrollHeight;
  }

  function nowStr() {
    const d = new Date();
    const p = (n) => String(n).padStart(2, '0');
    return `${d.getFullYear()}-${p(d.getMonth() + 1)}-${p(d.getDate())} ${p(d.getHours())}:${p(d.getMinutes())}:${p(d.getSeconds())}`;
  }

  // ---------- WebSocket 事件 ----------
  function setupWs() {
    ws.on('_open', async () => {
      $('conn-banner').classList.remove('show');
      await pullOffline();
      loadFriends(); // 刷新在线状态
    });

    ws.on('_close', () => { $('conn-banner').classList.add('show'); });
    ws.on('_reconnecting', () => { $('conn-banner').classList.add('show'); });

    ws.on('kicked', () => {
      API.clearSession();
      alert('您的账号已在其他设备登录，当前连接已断开');
      location.href = 'index.html';
    });

    ws.on('chat_single', (d) => {
      if (state.conv && state.conv.type === 'single' && state.conv.id === d.from) {
        appendMsg({ from: d.from, content: d.content, createdAt: d.createdAt || nowStr(), seq: d.seq });
      } else {
        const name = state.nameCache.get(d.from) || ('用户' + d.from);
        UI.toast(name + '：' + truncate(d.content), 'info');
      }
    });

    ws.on('chat_group', (d) => {
      if (d.from === myId) return; // 自己发的群消息由 ack 处理
      if (state.conv && state.conv.type === 'group' && state.conv.id === d.groupId) {
        appendMsg({ from: d.from, content: d.content, createdAt: d.createdAt || nowStr(), seq: d.seq });
        fillUnknownNames([{ from: d.from }]);
      } else {
        const g = state.groups.find(x => x.id === d.groupId);
        UI.toast('[' + (g ? g.name : '群' + d.groupId) + '] ' + truncate(d.content), 'info');
      }
    });

    ws.on('presence', (d) => {
      const f = state.friends.find(x => x.id === d.userId);
      if (f) {
        f.online = !!d.online;
        renderFriendList();
        updateConvStatus();
      }
    });

    ws.on('notify_apply', () => {
      UI.toast('收到新的好友/加群申请', 'info');
      loadNotifications();
    });

    ws.on('notify_apply_result', (d) => {
      UI.toast((d && d.message) || '您的申请已被处理', 'success');
      loadFriends();
      loadGroups();
    });

    ws.on('error', (d) => {
      if (d && d.message) UI.toast(d.message, 'error');
    });
  }

  function truncate(s, n = 30) {
    s = String(s || '');
    return s.length > n ? s.slice(0, n) + '…' : s;
  }

  // ---------- 离线消息 ----------
  async function pullOffline() {
    try {
      const data = await API.offlineMessages();
      const msgs = data.messages || [];
      if (!msgs.length) return;
      UI.toast('收到 ' + msgs.length + ' 条离线消息', 'info');
      msgs.forEach(m => {
        if (m.type === 'chat_single' && state.conv && state.conv.type === 'single' && state.conv.id === m.from) {
          appendMsg({ from: m.from, content: m.content, seq: m.seq, createdAt: m.createdAt });
        } else if (m.type === 'chat_group' && state.conv && state.conv.type === 'group' && state.conv.id === m.groupId) {
          appendMsg({ from: m.from, content: m.content, seq: m.seq, createdAt: m.createdAt });
        }
      });
    } catch (e) { /* 静默失败，历史接口可兜底 */ }
  }

  // ---------- 搜索 / 添加 弹窗 ----------
  setupModal('modal-search', 'btn-search');
  setupModal('modal-notify', 'btn-notify');

  function setupModal(modalId, triggerId) {
    const overlay = $(modalId);
    $(triggerId).addEventListener('click', () => {
      overlay.classList.add('show');
      if (modalId === 'modal-notify') renderNotifications();
    });
    overlay.addEventListener('click', (ev) => {
      if (ev.target === overlay || ev.target.closest('[data-close]')) {
        overlay.classList.remove('show');
      }
    });
  }
  document.addEventListener('keydown', (ev) => {
    if (ev.key === 'Escape') {
      document.querySelectorAll('.modal-overlay.show').forEach(el => el.classList.remove('show'));
    }
  });

  // 搜索弹窗 tab
  const searchTabs = [
    ['search-tab-user', 'search-pane-user'],
    ['search-tab-group', 'search-pane-group'],
    ['search-tab-create', 'search-pane-create'],
  ];
  searchTabs.forEach(([tabId]) => {
    $(tabId).addEventListener('click', () => {
      searchTabs.forEach(([t, p]) => {
        $(t).classList.toggle('active', t === tabId);
        $(p).hidden = (t !== tabId);
      });
    });
  });

  // -- 搜索用户 --
  let userPage = 1, userKeyword = '';
  $('search-user-btn').addEventListener('click', () => { userPage = 1; doSearchUsers(); });
  $('search-user-input').addEventListener('keydown', (ev) => {
    if (ev.key === 'Enter') { userPage = 1; doSearchUsers(); }
  });
  $('user-prev').addEventListener('click', () => { if (userPage > 1) { userPage--; doSearchUsers(); } });
  $('user-next').addEventListener('click', () => { userPage++; doSearchUsers(); });

  async function doSearchUsers() {
    userKeyword = $('search-user-input').value.trim();
    if (!userKeyword) return;
    try {
      const data = await API.searchUsers(userKeyword, userPage);
      const list = data.list || [];
      const box = $('search-user-results');
      if (!list.length) {
        box.innerHTML = '<div class="empty-state">没有找到匹配的用户</div>';
      } else {
        box.innerHTML = list.map(u => {
          const isSelf = u.id === myId;
          const isFriend = state.friends.some(f => f.id === u.id);
          let action;
          if (isSelf) action = '<span class="item-sub">自己</span>';
          else if (isFriend) action = '<span class="item-sub">已是好友</span>';
          else action = `<button class="btn btn-cta btn-sm" data-add-friend="${u.id}" data-name="${esc(u.nickname)}">加好友</button>`;
          return `
            <div class="result-item">
              <span class="avatar">${UI.avatarText(u.nickname)}</span>
              <span class="item-main">
                <span class="item-name">${esc(u.nickname)}</span>
                <span class="item-sub">ID: ${u.id}</span>
              </span>
              <span class="result-actions">${action}</span>
            </div>`;
        }).join('');
      }
      const totalPages = Math.max(1, Math.ceil((data.total || 0) / (data.pageSize || 20)));
      $('search-user-pager').hidden = totalPages <= 1;
      $('user-page-info').textContent = userPage + ' / ' + totalPages;
      $('user-prev').disabled = userPage <= 1;
      $('user-next').disabled = userPage >= totalPages;
    } catch (e) { UI.toast(e.msg || '搜索失败', 'error'); }
  }

  $('search-user-results').addEventListener('click', async (ev) => {
    const btn = ev.target.closest('[data-add-friend]');
    if (!btn) return;
    const userId = Number(btn.dataset.addFriend);
    const message = prompt('验证留言（可选）：', '我是 ' + myNickname) ?? '';
    btn.disabled = true;
    try {
      await API.friendRequest(userId, message);
      UI.toast('好友申请已发送', 'success');
      btn.outerHTML = '<span class="item-sub">已申请</span>';
    } catch (e) {
      btn.disabled = false;
      const map = { 3001: '已是好友', 3002: '已发送过申请，等待对方处理', 3004: '不能添加自己' };
      UI.toast(map[e.code] || e.msg || '申请失败', 'error');
    }
  });

  // -- 搜索群 --
  let groupPage = 1;
  $('search-group-btn').addEventListener('click', () => { groupPage = 1; doSearchGroups(); });
  $('search-group-input').addEventListener('keydown', (ev) => {
    if (ev.key === 'Enter') { groupPage = 1; doSearchGroups(); }
  });
  $('group-prev').addEventListener('click', () => { if (groupPage > 1) { groupPage--; doSearchGroups(); } });
  $('group-next').addEventListener('click', () => { groupPage++; doSearchGroups(); });

  async function doSearchGroups() {
    const keyword = $('search-group-input').value.trim();
    if (!keyword) return;
    try {
      const data = await API.searchGroups(keyword, groupPage);
      const list = data.list || [];
      const box = $('search-group-results');
      if (!list.length) {
        box.innerHTML = '<div class="empty-state">没有找到匹配的群聊</div>';
      } else {
        box.innerHTML = list.map(g => {
          const joined = state.groups.some(x => x.id === g.id);
          const action = joined
            ? '<span class="item-sub">已加入</span>'
            : `<button class="btn btn-cta btn-sm" data-join-group="${g.id}">申请加入</button>`;
          return `
            <div class="result-item">
              <span class="avatar avatar-group">${UI.avatarText(g.name)}</span>
              <span class="item-main">
                <span class="item-name">${esc(g.name)}</span>
                <span class="item-sub">${g.memberCount} 名成员</span>
              </span>
              <span class="result-actions">${action}</span>
            </div>`;
        }).join('');
      }
      const totalPages = Math.max(1, Math.ceil((data.total || 0) / 20));
      $('search-group-pager').hidden = totalPages <= 1;
      $('group-page-info').textContent = groupPage + ' / ' + totalPages;
      $('group-prev').disabled = groupPage <= 1;
      $('group-next').disabled = groupPage >= totalPages;
    } catch (e) { UI.toast(e.msg || '搜索失败', 'error'); }
  }

  $('search-group-results').addEventListener('click', async (ev) => {
    const btn = ev.target.closest('[data-join-group]');
    if (!btn) return;
    btn.disabled = true;
    try {
      await API.groupJoin(Number(btn.dataset.joinGroup));
      UI.toast('加群申请已发送', 'success');
      btn.outerHTML = '<span class="item-sub">已申请</span>';
    } catch (e) {
      btn.disabled = false;
      const map = { 4002: '已在群中', 4005: '已发送过申请，等待群主处理' };
      UI.toast(map[e.code] || e.msg || '申请失败', 'error');
    }
  });

  // -- 创建群 --
  $('create-group-btn').addEventListener('click', async () => {
    const name = $('create-group-name').value.trim();
    if (!name) { UI.toast('请输入群名称', 'error'); return; }
    const btn = $('create-group-btn');
    btn.disabled = true;
    try {
      await API.groupCreate(name);
      UI.toast('群创建成功', 'success');
      $('create-group-name').value = '';
      $('modal-search').classList.remove('show');
      await loadGroups();
      switchSideTab(false);
    } catch (e) {
      UI.toast(e.msg || '创建失败', 'error');
    } finally { btn.disabled = false; }
  });

  // ---------- 申请通知与审批 ----------
  const notifyTabs = [
    ['notify-tab-friend', 'notify-pane-friend'],
    ['notify-tab-group', 'notify-pane-group'],
  ];
  notifyTabs.forEach(([tabId]) => {
    $(tabId).addEventListener('click', () => {
      notifyTabs.forEach(([t, p]) => {
        $(t).classList.toggle('active', t === tabId);
        $(p).hidden = (t !== tabId);
      });
    });
  });

  async function loadNotifications() {
    try {
      const data = await API.friendRequests();
      state.friendReqs = data.list || [];
    } catch (e) { state.friendReqs = []; }

    // 我是群主的群 → 拉取各群待处理加群申请
    state.groupReqs = [];
    const owned = state.groups.filter(g => g.role === 1);
    for (const g of owned) {
      try {
        const data = await API.groupRequests(g.id);
        (data.list || []).forEach(r => state.groupReqs.push({ ...r, groupId: g.id, groupName: g.name }));
      } catch (e) { /* 单群失败不影响其他 */ }
    }

    const count = state.friendReqs.length + state.groupReqs.length;
    const badge = $('notify-badge');
    badge.textContent = count > 99 ? '99+' : String(count || '');
    badge.dataset.count = String(count);

    if ($('modal-notify').classList.contains('show')) renderNotifications();
  }

  function renderNotifications() {
    const fbox = $('notify-pane-friend');
    fbox.innerHTML = state.friendReqs.length
      ? state.friendReqs.map(r => `
          <div class="result-item">
            <span class="avatar">${UI.avatarText(r.nickname)}</span>
            <span class="item-main">
              <span class="item-name">${esc(r.nickname)}</span>
              <span class="item-sub">${esc(r.message || '请求加为好友')} · ${UI.formatTime(r.createdAt)}</span>
            </span>
            <span class="result-actions">
              <button class="btn btn-cta btn-sm" data-friend-accept="${r.id}">同意</button>
              <button class="btn btn-ghost btn-sm" data-friend-reject="${r.id}">拒绝</button>
            </span>
          </div>`).join('')
      : '<div class="empty-state">暂无待处理的好友申请</div>';

    const gbox = $('notify-pane-group');
    gbox.innerHTML = state.groupReqs.length
      ? state.groupReqs.map(r => `
          <div class="result-item">
            <span class="avatar">${UI.avatarText(r.nickname)}</span>
            <span class="item-main">
              <span class="item-name">${esc(r.nickname)}</span>
              <span class="item-sub">申请加入「${esc(r.groupName)}」 · ${UI.formatTime(r.createdAt)}</span>
            </span>
            <span class="result-actions">
              <button class="btn btn-cta btn-sm" data-group-accept="${r.id}">同意</button>
              <button class="btn btn-ghost btn-sm" data-group-reject="${r.id}">拒绝</button>
            </span>
          </div>`).join('')
      : '<div class="empty-state">暂无待处理的加群申请</div>';
  }

  $('modal-notify').addEventListener('click', async (ev) => {
    const btn = ev.target.closest('button[data-friend-accept],button[data-friend-reject],button[data-group-accept],button[data-group-reject]');
    if (!btn) return;
    btn.disabled = true;
    try {
      if (btn.dataset.friendAccept) {
        await API.friendAccept(Number(btn.dataset.friendAccept));
        UI.toast('已同意好友申请', 'success');
        await loadFriends();
      } else if (btn.dataset.friendReject) {
        await API.friendReject(Number(btn.dataset.friendReject));
        UI.toast('已拒绝好友申请', 'info');
      } else if (btn.dataset.groupAccept) {
        await API.groupAccept(Number(btn.dataset.groupAccept));
        UI.toast('已同意加群申请', 'success');
        await loadGroups();
      } else if (btn.dataset.groupReject) {
        await API.groupReject(Number(btn.dataset.groupReject));
        UI.toast('已拒绝加群申请', 'info');
      }
      await loadNotifications();
      renderNotifications();
    } catch (e) {
      btn.disabled = false;
      UI.toast(e.msg || '操作失败', 'error');
    }
  });

  // ---------- 会话操作（删好友 / 群成员 / 退群 / 注销群） ----------
  $('btn-conv-action').addEventListener('click', async () => {
    if (!state.conv) return;
    const body = $('modal-conv-body');
    $('modal-conv').classList.add('show');

    if (state.conv.type === 'single') {
      $('modal-conv-title').textContent = state.conv.name;
      body.innerHTML = `
        <p style="margin-bottom:16px;color:var(--text-muted);font-size:14px;">
          删除好友后双方互不可见、不能发消息；历史消息保留，重新加为好友后可继续聊天。
        </p>
        <button class="btn btn-danger btn-block" id="conv-del-friend">删除好友</button>`;
      $('conv-del-friend').addEventListener('click', async () => {
        if (!confirm('确定删除好友「' + state.conv.name + '」？')) return;
        try {
          await API.friendDelete(state.conv.id);
          UI.toast('已删除好友', 'success');
          $('modal-conv').classList.remove('show');
          $('btn-back').click();
          await loadFriends();
        } catch (e) { UI.toast(e.msg || '删除失败', 'error'); }
      });
    } else {
      $('modal-conv-title').textContent = state.conv.name;
      const g = state.groups.find(x => x.id === state.conv.id);
      const isOwner = g && g.role === 1;
      body.innerHTML = '<div class="empty-state">加载成员中…</div>';
      try {
        const data = await API.groupMembers(state.conv.id);
        const members = data.list || [];
        members.forEach(m => state.nameCache.set(m.id, m.nickname));
        body.innerHTML = `
          <h4 style="font-size:14px;font-weight:700;margin-bottom:8px;">成员（${members.length}）</h4>
          <div style="max-height:280px;overflow-y:auto;margin-bottom:16px;">
            ${members.map(m => `
              <div class="result-item">
                <span class="avatar">${UI.avatarText(m.nickname)}</span>
                <span class="item-main">
                  <span class="item-name">${esc(m.nickname)} ${m.role === 1 ? '<span class="owner-tag">群主</span>' : ''}</span>
                  <span class="item-sub">ID: ${m.id}</span>
                </span>
              </div>`).join('')}
          </div>
          ${isOwner
            ? '<button class="btn btn-danger btn-block" id="conv-dissolve">注销群（不可恢复）</button>'
            : '<button class="btn btn-danger btn-block" id="conv-leave">退出群聊</button>'}`;

        const dissolve = document.getElementById('conv-dissolve');
        if (dissolve) dissolve.addEventListener('click', async () => {
          if (!confirm('注销群将硬删除群、成员关系与全部群消息，不可恢复。确定？')) return;
          try {
            await API.groupDelete(state.conv.id);
            UI.toast('群已注销', 'success');
            $('modal-conv').classList.remove('show');
            $('btn-back').click();
            await loadGroups();
          } catch (e) { UI.toast(e.msg || '注销失败', 'error'); }
        });

        const leave = document.getElementById('conv-leave');
        if (leave) leave.addEventListener('click', async () => {
          if (!confirm('确定退出群聊「' + state.conv.name + '」？')) return;
          try {
            await API.groupLeave(state.conv.id);
            UI.toast('已退群', 'success');
            $('modal-conv').classList.remove('show');
            $('btn-back').click();
            await loadGroups();
          } catch (e) { UI.toast(e.msg || '退群失败', 'error'); }
        });
      } catch (e) {
        body.innerHTML = '<div class="empty-state">' + esc(e.msg || '加载失败') + '</div>';
      }
    }
  });
  setupModal('modal-conv', 'btn-conv-action');

  // ---------- 退出登录 ----------
  $('btn-logout').addEventListener('click', () => {
    if (!confirm('确定退出登录？')) return;
    ws.close();
    API.clearSession();
    location.href = 'index.html';
  });

  // 输入框自适应高度
  input.addEventListener('input', () => {
    input.style.height = 'auto';
    input.style.height = Math.min(input.scrollHeight, 120) + 'px';
  });
})();
