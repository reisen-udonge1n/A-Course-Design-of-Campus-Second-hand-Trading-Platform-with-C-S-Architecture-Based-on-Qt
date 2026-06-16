#ifndef CHATSERVER_H
#define CHATSERVER_H

// ============================================================
// ChatServer — 双协议服务器（TCP 聊天 + HTTP 图片服务）
//
// TCP 协议细节（应用层）：
//   所有消息以 4 字节大端序长度前缀 + JSON payload 传输。
//   帧格式: [4-byte BE length][UTF-8 JSON]
//   消息类型: login, logout, message, ping/pong, user_list
//
// HTTP 服务：
//   POST /upload          — 上传图片，返回 JSON {url: ...}
//   GET /images/<file>    — 获取图片文件
//
// 并发模型：
//   单线程事件驱动（Qt 事件循环 + 非阻塞 I/O）。
//   每个连接由 socket 上的 readyRead/disconnected 信号驱动。
//   不涉及线程池或 epoll 直接操作，适合小型应用。
//
// 状态管理：
//   m_clients:      所有已建立 TCP 连接的客户端（含未登录）
//   m_userToSocket: 已登录用户的 userId→socket 映射，用于路由
// ============================================================

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QMap>
#include <QDir>
#include <QJsonObject>

struct ClientInfo {
    QTcpSocket *socket;
    int userId;
    QString username;
    QByteArray buffer;
};

struct HttpClientInfo {
    QTcpSocket *socket;
    QByteArray buffer;
    QString method;
    QString path;
    qint64 contentLength;
    bool headersParsed;
};

class ChatServer : public QObject
{
    Q_OBJECT

public:
    explicit ChatServer(QObject *parent = nullptr);
    ~ChatServer();

    bool start(quint16 tcpPort, quint16 httpPort);

private slots:
    void onNewConnection();
    void onReadyRead();
    void onDisconnected();
    void onError(QAbstractSocket::SocketError err);

    // HTTP handlers
    void onHttpNewConnection();
    void onHttpReadyRead();
    void onHttpDisconnected();

private:
    void processMessage(QTcpSocket *socket, ClientInfo &info, const QJsonObject &obj);
    void sendJson(QTcpSocket *socket, const QJsonObject &obj);
    void broadcastUserList();

    // HTTP helpers
    void handleHttpRequest(HttpClientInfo &info);
    void sendHttpResponse(QTcpSocket *socket, int statusCode,
                          const QString &statusText,
                          const QString &contentType,
                          const QByteArray &body);
    QString saveUploadedFile(const QByteArray &data, const QString &originalFilename);

    QTcpServer *m_server;
    QTcpServer *m_httpServer;
    QMap<QTcpSocket*, ClientInfo> m_clients;
    QMap<int, QTcpSocket*> m_userToSocket;
    QMap<QTcpSocket*, HttpClientInfo> m_httpClients;
    QDir m_imagesDir;
};

#endif // CHATSERVER_H
