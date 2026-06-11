/* ui.js — 通用 UI 工具：toast、HTML 转义、时间格式化、头像 */
(function () {
  'use strict';

  function escapeHtml(str) {
    return String(str ?? '')
      .replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;')
      .replace(/"/g, '&quot;').replace(/'/g, '&#39;');
  }

  function toast(message, type = 'info', duration = 3000) {
    let container = document.getElementById('toast-container');
    if (!container) {
      container = document.createElement('div');
      container.id = 'toast-container';
      document.body.appendChild(container);
    }
    const el = document.createElement('div');
    el.className = 'toast toast-' + type;
    el.setAttribute('role', 'status');
    el.textContent = message;
    container.appendChild(el);
    setTimeout(() => el.remove(), duration);
  }

  function avatarText(name) {
    const s = String(name || '?').trim();
    return escapeHtml(s.slice(0, 1).toUpperCase());
  }

  function formatTime(str) {
    if (!str) return '';
    const d = new Date(String(str).replace(' ', 'T'));
    if (isNaN(d.getTime())) return str;
    const now = new Date();
    const sameDay = d.toDateString() === now.toDateString();
    const hm = String(d.getHours()).padStart(2, '0') + ':' + String(d.getMinutes()).padStart(2, '0');
    if (sameDay) return hm;
    return (d.getMonth() + 1) + '月' + d.getDate() + '日 ' + hm;
  }

  window.UI = { escapeHtml, toast, avatarText, formatTime };
})();
