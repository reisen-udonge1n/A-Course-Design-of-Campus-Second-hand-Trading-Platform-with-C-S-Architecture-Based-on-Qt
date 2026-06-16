#include <QCoreApplication>
#include "chatserver.h"
#include "config.h"
#include <QDebug>
#include <QDir>

// ============================================================
// 聊天&图片服务器入口（无头进程，无 GUI）
//
// 架构说明：
//   ChatServer 在一个进程中同时启动两个监听端口：
//   - TCP 端口 (CHAT_PORT=8888)：处理实时消息路由、用户在线
//     状态广播、心跳检测（30s ping/pong）
//   - HTTP 端口 (IMAGE_SERVER_PORT=9999)：提供图片上传(POST)
//     和访问(GET)服务，将文件持久化到本地 images/ 目录
//
// 为什么合并在一个进程？
//   简化部署：只需在服务器上启动一个进程即可同时提供聊天和
//   图片服务。生产环境应考虑用 nginx 反向代理 HTTP 端口。
// ============================================================

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    app.setApplicationName("Z-Trade Chat & File Server");

    QDir().mkpath("images");

    ChatServer server;
    quint16 chatPort = CHAT_PORT;
    quint16 httpPort = IMAGE_SERVER_PORT;

    if (!server.start(chatPort, httpPort)) {
        qCritical() << "Failed to start server";
        return 1;
    }

    qInfo() << "Chat server running on port" << chatPort;
    qInfo() << "Image HTTP server running on port" << httpPort;
    return app.exec();
}
