-- ============================================================================
-- online-chat 建库脚本
-- MySQL 8.x | 字符集 utf8mb4
-- 用法: mysql -u root -p < sql/schema.sql
-- 或在 docker-compose 的 mysql 容器初始化时自动执行
-- ============================================================================

CREATE DATABASE IF NOT EXISTS online_chat
    DEFAULT CHARACTER SET utf8mb4
    DEFAULT COLLATE utf8mb4_unicode_ci;

USE online_chat;

-- 防止重复执行报错：先删后建（开发阶段用法，生产应使用 migration 工具）
SET FOREIGN_KEY_CHECKS = 0;

-- ============================================================================
-- 1. users — 用户表
-- ============================================================================
DROP TABLE IF EXISTS users;
CREATE TABLE users (
    id            BIGINT       NOT NULL AUTO_INCREMENT COMMENT '用户ID',
    email         VARCHAR(128) NOT NULL                COMMENT '邮箱（兼作登录账号，唯一）',
    password_hash VARCHAR(100) NOT NULL                COMMENT 'bcrypt 哈希',
    nickname      VARCHAR(64)  NOT NULL DEFAULT ''     COMMENT '用户名称（可重复，搜索键）',
    role          TINYINT      NOT NULL DEFAULT 0      COMMENT '0=普通用户 1=超级用户',
    status        TINYINT      NOT NULL DEFAULT 0      COMMENT '0=正常 1=禁用',
    created_at    DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at    DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    PRIMARY KEY (id),
    UNIQUE KEY uk_email (email),
    KEY idx_nickname (nickname)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- ============================================================================
-- 2. friendships — 好友关系表（双向单行，约定 user_a < user_b）
-- ============================================================================
DROP TABLE IF EXISTS friendships;
CREATE TABLE friendships (
    id         BIGINT   NOT NULL AUTO_INCREMENT,
    user_a     BIGINT   NOT NULL COMMENT '较小的 userId',
    user_b     BIGINT   NOT NULL COMMENT '较大的 userId',
    status     TINYINT  NOT NULL DEFAULT 1 COMMENT '1=好友 0=已解除',
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    PRIMARY KEY (id),
    UNIQUE KEY uk_pair (user_a, user_b)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- ============================================================================
-- 3. friend_requests — 好友申请表
-- ============================================================================
DROP TABLE IF EXISTS friend_requests;
CREATE TABLE friend_requests (
    id         BIGINT       NOT NULL AUTO_INCREMENT,
    from_user  BIGINT       NOT NULL COMMENT '申请人',
    to_user    BIGINT       NOT NULL COMMENT '被申请人',
    message    VARCHAR(255) NOT NULL DEFAULT '' COMMENT '验证留言',
    status     TINYINT      NOT NULL DEFAULT 0 COMMENT '0=待处理 1=同意 2=拒绝',
    created_at DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    PRIMARY KEY (id),
    KEY idx_to_user_status (to_user, status)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- ============================================================================
-- 4. group_chats — 群表
-- ============================================================================
DROP TABLE IF EXISTS group_chats;
CREATE TABLE group_chats (
    id           BIGINT      NOT NULL AUTO_INCREMENT COMMENT '群ID',
    name         VARCHAR(64) NOT NULL                COMMENT '群名称（搜索键）',
    owner_id     BIGINT      NOT NULL                COMMENT '群主 userId',
    member_count INT         NOT NULL DEFAULT 1      COMMENT '成员数（冗余）',
    created_at   DATETIME    NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at   DATETIME    NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    PRIMARY KEY (id),
    KEY idx_name (name),
    KEY idx_owner (owner_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- ============================================================================
-- 5. group_members — 群成员表
-- ============================================================================
DROP TABLE IF EXISTS group_members;
CREATE TABLE group_members (
    id        BIGINT   NOT NULL AUTO_INCREMENT,
    group_id  BIGINT   NOT NULL,
    user_id   BIGINT   NOT NULL,
    role      TINYINT  NOT NULL DEFAULT 0 COMMENT '0=普通成员 1=群主',
    joined_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (id),
    UNIQUE KEY uk_group_user (group_id, user_id),
    KEY idx_user (user_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- ============================================================================
-- 6. group_requests — 加群申请表
-- ============================================================================
DROP TABLE IF EXISTS group_requests;
CREATE TABLE group_requests (
    id         BIGINT   NOT NULL AUTO_INCREMENT,
    group_id   BIGINT   NOT NULL,
    from_user  BIGINT   NOT NULL COMMENT '申请人',
    status     TINYINT  NOT NULL DEFAULT 0 COMMENT '0=待处理 1=同意 2=拒绝',
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    PRIMARY KEY (id),
    KEY idx_group_status (group_id, status)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- ============================================================================
-- 7. single_messages — 单聊消息表
-- ============================================================================
DROP TABLE IF EXISTS single_messages;
CREATE TABLE single_messages (
    id         BIGINT   NOT NULL AUTO_INCREMENT COMMENT '全局消息ID',
    conv_key   VARCHAR(32) NOT NULL             COMMENT '会话键 min(uid)-max(uid)',
    from_user  BIGINT   NOT NULL,
    to_user    BIGINT   NOT NULL,
    content    TEXT     NOT NULL                COMMENT '文本内容',
    seq        BIGINT   NOT NULL               COMMENT '会话内单调序号',
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (id),
    KEY idx_conv_seq (conv_key, seq)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- ============================================================================
-- 8. group_messages — 群聊消息表
-- ============================================================================
DROP TABLE IF EXISTS group_messages;
CREATE TABLE group_messages (
    id         BIGINT   NOT NULL AUTO_INCREMENT COMMENT '全局消息ID',
    group_id   BIGINT   NOT NULL,
    from_user  BIGINT   NOT NULL,
    content    TEXT     NOT NULL                COMMENT '文本内容',
    seq        BIGINT   NOT NULL               COMMENT '群内单调序号',
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (id),
    KEY idx_group_seq (group_id, seq)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- ============================================================================
-- 9. offline_messages — 离线消息索引表
-- ============================================================================
DROP TABLE IF EXISTS offline_messages;
CREATE TABLE offline_messages (
    id         BIGINT   NOT NULL AUTO_INCREMENT,
    user_id    BIGINT   NOT NULL COMMENT '接收者（离线时未投递）',
    msg_type   TINYINT  NOT NULL COMMENT '1=单聊 2=群聊',
    msg_id     BIGINT   NOT NULL COMMENT '指向 single_messages 或 group_messages.id',
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (id),
    KEY idx_user (user_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- ============================================================================
-- 10. 预置 admin 超管账号
--
-- 密码: admin123（bcrypt cost=10 哈希）
-- ⚠️ 此为开发默认密码，生产环境务必在部署后立即修改！
-- ============================================================================
INSERT INTO users (email, password_hash, nickname, role)
VALUES (
    'admin@online-chat.local',
    '$2a$10$N9qo8uLOickgx2ZMRZoMyeIjZAgcfl7p92ldGxad68LJZdL17lhWy',
    '管理员',
    1  -- role=1 超级用户
) ON DUPLICATE KEY UPDATE email=email;  -- 幂等，重复执行不报错

SET FOREIGN_KEY_CHECKS = 1;

-- 验证
SELECT '✅ Schema initialized, admin account ready' AS status;
