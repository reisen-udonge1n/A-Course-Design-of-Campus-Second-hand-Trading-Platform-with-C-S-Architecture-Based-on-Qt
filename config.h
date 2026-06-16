#ifndef CONFIG_H
#define CONFIG_H

// ============================================================
// 集中配置 · 基础设施拓扑
//
// 这是一个三节点拓扑：
//   [客户端] ←TCP 8888→ [ChatServer] ───→ [MySQL 3306]
//              ←HTTP 9999→               └──local images/
//
// ChatServer 承担双重角色：
//   1. TCP 消息路由（在线用户之间的 JSON 消息转发）
//   2. HTTP 图片服务器（上传、访问、持久化）
//
// SMTP 通过 126.com 的 SSL 端口 465 发送验证码邮件。
// 所有密码在生产环境应替换为环境变量或配置中心下发。
// ============================================================

// ---- MySQL 数据库 ----
#define DB_HOST     "132.232.253.110"
#define DB_PORT     3306
#define DB_USER     "remote_user"
#define DB_PASSWORD "lubeijiahao123"
#define DB_NAME     "secondhand_trading"

// ---- 聊天服务器 ----
#define CHAT_HOST   "132.232.253.110"
#define CHAT_PORT   8888

// ---- 图片服务器（HTTP 文件服务，与聊天服务器同机部署） ----
#define IMAGE_SERVER_HOST   "132.232.253.110"
#define IMAGE_SERVER_PORT   9999

// ---- SMTP 邮件 ----
#define SMTP_USER     "LUBEI2004@126.COM"
#define SMTP_PASSWORD "HDk9YXMVYuKuhpKA"
#define SMTP_HOST     "smtp.126.com"
#define SMTP_PORT     465

#endif // CONFIG_H
