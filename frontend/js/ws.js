/* ws.js — WebSocket 客户端封装
 * 功能：自动重连（指数退避）、心跳保活、统一信封收发、事件订阅
 * 协议见 API.md 四、WebSocket 网关
 */
(function () {
  'use strict';

  const HEARTBEAT_INTERVAL = 25000;   // 25s 主动 ping（服务端 60s 无心跳判离线）
  const RECONNECT_BASE = 1000;
  const RECONNECT_MAX = 30000;

  class WsClient {
    constructor() {
      this.ws = null;
      this.seq = 0;
      this.handlers = {};          // type -> [fn]
      this.pendingAcks = new Map(); // seq -> {resolve, reject, timer}
      this.heartbeatTimer = null;
      this.reconnectAttempts = 0;
      this.reconnectTimer = null;
      this.manualClose = false;
      this.connected = false;
    }

    connect() {
      const token = window.API.getToken();
      if (!token) return;
      this.manualClose = false;

      const proto = location.protocol === 'https:' ? 'wss://' : 'ws://';
      const url = proto + location.host + '/ws?token=' + encodeURIComponent(token);

      try { this.ws = new WebSocket(url); }
      catch (e) { this._scheduleReconnect(); return; }

      this.ws.onopen = () => {
        this.connected = true;
        this.reconnectAttempts = 0;
        this._startHeartbeat();
        this._emit('_open', {});
      };

      this.ws.onmessage = (ev) => {
        let msg;
        try { msg = JSON.parse(ev.data); } catch (e) { return; }
        if (!msg || typeof msg.type !== 'string') return;

        if (msg.type === 'ack' && this.pendingAcks.has(msg.seq)) {
          const p = this.pendingAcks.get(msg.seq);
          this.pendingAcks.delete(msg.seq);
          clearTimeout(p.timer);
          p.resolve(msg.data);
          return;
        }
        if (msg.type === 'kicked') {
          this.manualClose = true;  // 被挤下线不再重连
        }
        this._emit(msg.type, msg.data, msg.seq);
      };

      this.ws.onclose = () => {
        this.connected = false;
        this._stopHeartbeat();
        this._rejectAllPending('连接已断开');
        this._emit('_close', {});
        if (!this.manualClose) this._scheduleReconnect();
      };

      this.ws.onerror = () => { /* onclose 统一处理 */ };
    }

    close() {
      this.manualClose = true;
      clearTimeout(this.reconnectTimer);
      this._stopHeartbeat();
      if (this.ws) this.ws.close();
    }

    on(type, fn) {
      (this.handlers[type] = this.handlers[type] || []).push(fn);
    }

    /* 发送并等待服务端 ack（用于聊天消息） */
    sendWithAck(type, data, timeoutMs = 10000) {
      return new Promise((resolve, reject) => {
        if (!this.connected) { reject({ msg: '未连接' }); return; }
        const seq = ++this.seq;
        const timer = setTimeout(() => {
          this.pendingAcks.delete(seq);
          reject({ msg: '发送超时' });
        }, timeoutMs);
        this.pendingAcks.set(seq, { resolve, reject, timer });
        this.ws.send(JSON.stringify({ type, seq, data }));
      });
    }

    send(type, data) {
      if (!this.connected) return false;
      this.ws.send(JSON.stringify({ type, seq: ++this.seq, data }));
      return true;
    }

    _emit(type, data, seq) {
      (this.handlers[type] || []).forEach(fn => {
        try { fn(data, seq); } catch (e) { console.error('[ws handler]', type, e); }
      });
    }

    _startHeartbeat() {
      this._stopHeartbeat();
      this.heartbeatTimer = setInterval(() => {
        if (this.connected) this.send('ping', {});
      }, HEARTBEAT_INTERVAL);
    }

    _stopHeartbeat() {
      clearInterval(this.heartbeatTimer);
      this.heartbeatTimer = null;
    }

    _scheduleReconnect() {
      const delay = Math.min(RECONNECT_BASE * Math.pow(2, this.reconnectAttempts), RECONNECT_MAX);
      this.reconnectAttempts++;
      this._emit('_reconnecting', { attempt: this.reconnectAttempts, delay });
      clearTimeout(this.reconnectTimer);
      this.reconnectTimer = setTimeout(() => this.connect(), delay);
    }

    _rejectAllPending(msg) {
      this.pendingAcks.forEach(p => { clearTimeout(p.timer); p.reject({ msg }); });
      this.pendingAcks.clear();
    }
  }

  window.WsClient = WsClient;
})();
