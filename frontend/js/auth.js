/* auth.js — 注册/登录页逻辑（验证码流程 + 60s 倒计时） */
(function () {
  'use strict';

  // 已登录则直接进入主界面
  if (API.getToken()) {
    location.href = 'chat.html';
    return;
  }

  const tabLogin = document.getElementById('tab-login');
  const tabRegister = document.getElementById('tab-register');
  const formLogin = document.getElementById('form-login');
  const formRegister = document.getElementById('form-register');

  function switchTab(isLogin) {
    tabLogin.classList.toggle('active', isLogin);
    tabRegister.classList.toggle('active', !isLogin);
    tabLogin.setAttribute('aria-selected', String(isLogin));
    tabRegister.setAttribute('aria-selected', String(!isLogin));
    formLogin.classList.toggle('active', isLogin);
    formRegister.classList.toggle('active', !isLogin);
  }
  tabLogin.addEventListener('click', () => switchTab(true));
  tabRegister.addEventListener('click', () => switchTab(false));

  function showError(id, msg) {
    const el = document.getElementById(id);
    el.textContent = msg || '';
    el.classList.toggle('show', !!msg);
  }

  // ---- 发送验证码（60s 倒计时） ----
  const sendCodeBtn = document.getElementById('send-code-btn');
  let countdownTimer = null;

  function startCountdown(seconds) {
    let left = seconds;
    sendCodeBtn.disabled = true;
    sendCodeBtn.textContent = left + 's 后重发';
    countdownTimer = setInterval(() => {
      left--;
      if (left <= 0) {
        clearInterval(countdownTimer);
        sendCodeBtn.disabled = false;
        sendCodeBtn.textContent = '发送验证码';
      } else {
        sendCodeBtn.textContent = left + 's 后重发';
      }
    }, 1000);
  }

  sendCodeBtn.addEventListener('click', async () => {
    showError('reg-error', '');
    const email = document.getElementById('reg-email').value.trim();
    if (!email || !/^[^\s@]+@[^\s@]+\.[^\s@]+$/.test(email)) {
      showError('reg-error', '请输入有效的邮箱地址');
      return;
    }
    sendCodeBtn.disabled = true;
    sendCodeBtn.textContent = '发送中…';
    try {
      await API.sendCode(email);
      UI.toast('验证码已发送，请查收邮箱', 'success');
      startCountdown(60);
    } catch (e) {
      sendCodeBtn.disabled = false;
      sendCodeBtn.textContent = '发送验证码';
      if (e.code === 2007) {
        showError('reg-error', '发送过于频繁，请 60 秒后重试');
        startCountdown(60);
      } else {
        showError('reg-error', e.msg || '验证码发送失败');
      }
    }
  });

  // ---- 注册 ----
  formRegister.addEventListener('submit', async (ev) => {
    ev.preventDefault();
    showError('reg-error', '');

    const email = document.getElementById('reg-email').value.trim();
    const code = document.getElementById('reg-code').value.trim();
    const password = document.getElementById('reg-password').value;
    const nickname = document.getElementById('reg-nickname').value.trim();

    if (!email) { showError('reg-error', '请输入邮箱'); return; }
    if (!/^\d{6}$/.test(code)) { showError('reg-error', '请输入 6 位数字验证码'); return; }
    if (password.length < 6) { showError('reg-error', '密码至少 6 位'); return; }

    const btn = document.getElementById('reg-submit');
    btn.disabled = true;
    btn.textContent = '注册中…';
    try {
      const payload = { email, code, password };
      if (nickname) payload.nickname = nickname;
      await API.register(payload);
      UI.toast('注册成功，请登录', 'success');
      switchTab(true);
      document.getElementById('login-email').value = email;
      document.getElementById('login-password').focus();
    } catch (e) {
      const map = {
        2001: '验证码已过期，请重新获取',
        2002: '验证码错误',
        2003: '该邮箱已注册，请直接登录',
      };
      showError('reg-error', map[e.code] || e.msg || '注册失败');
    } finally {
      btn.disabled = false;
      btn.textContent = '注 册';
    }
  });

  // ---- 登录 ----
  formLogin.addEventListener('submit', async (ev) => {
    ev.preventDefault();
    showError('login-error', '');

    const email = document.getElementById('login-email').value.trim();
    const password = document.getElementById('login-password').value;
    if (!email || !password) { showError('login-error', '请输入邮箱和密码'); return; }

    const btn = document.getElementById('login-submit');
    btn.disabled = true;
    btn.textContent = '登录中…';
    try {
      const data = await API.login(email, password);
      API.setSession(data);
      location.href = 'chat.html';
    } catch (e) {
      const map = {
        2005: '邮箱或密码错误',
        2006: '账号已被禁用',
      };
      showError('login-error', map[e.code] || e.msg || '登录失败');
      btn.disabled = false;
      btn.textContent = '登 录';
    }
  });
})();
