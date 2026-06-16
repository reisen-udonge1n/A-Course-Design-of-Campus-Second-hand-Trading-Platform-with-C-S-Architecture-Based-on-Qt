#ifndef CHATCLIENT_H
#define CHATCLIENT_H

// ============================================================
// ChatClient — 聊天 TCP 客户端 Singleton
//
// 职责：
//   1. 与服务端建立持久 TCP 连接
//   2. 登录鉴权、消息收发
//   3. 心跳保活（30s ping/pong）
//   4. 断线自动重连（5s 定时器，非指数退避）
//   5. 本地在线用户状态跟踪
//
// 协议：
//   与服务端共享相同的长度前缀 JSON 帧格式。
//   信号驱动模型——所有入站消息通过 Qt 信号派发，
//   调用方无需处理 socket 细节。
//
// 生命周期：
//   进程级别 Singleton。MainWindow 构造时自动连接，
//   应用退出时自动断开。
// ============================================================

#include <QObject>
#include <QTcpSocket>
#include <QJsonObject>
#include <QTimer>
#include <QSet>

class ChatClient : public QObject
{
    Q_OBJECT

public:
    static ChatClient &instance();

    void connectToServer(const QString &host, quint16 port);
    void disconnectFromServer();
    bool isConnected() const;
    bool isLoggedIn() const;

    void login(int userId, const QString &username);
    void sendMessage(int toUserId, const QString &content);
    bool isUserOnline(int userId) const;

signals:
    void connected();
    void disconnected();
    void loginSuccess();
    void loginFailed(const QString &reason);
    void messageReceived(int fromUserId, const QString &fromUsername,
                         const QString &content, qint64 timestamp);
    void messageSent(int toUserId, const QString &content);
    void userListChanged();
    void errorOccurred(const QString &error);

private slots:
    void onConnected();
    void onDisconnected();
    void onReadyRead();
    void onError(QAbstractSocket::SocketError err);
    void onReconnectTimer();
    void onPingTimer();

private:
    explicit ChatClient(QObject *parent = nullptr);

    void processMessage(const QJsonObject &obj);
    void sendJson(const QJsonObject &obj);

    QTcpSocket *m_socket;
    QTimer *m_reconnectTimer;
    QTimer *m_pingTimer;
    QString m_host;
    quint16 m_port;
    int m_userId;
    QString m_username;
    QByteArray m_buffer;
    bool m_loggedIn;
    bool m_intentionalDisconnect;
    QSet<int> m_onlineUsers;
};

#endif // CHATCLIENT_H
