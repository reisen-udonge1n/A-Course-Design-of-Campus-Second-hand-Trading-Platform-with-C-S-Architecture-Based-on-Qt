// ============================================================
// ChatClient — TCP 客户端实现
//
// 关键设计决策：
//
// 1. 帧同步方案（vs. 行协议）
//    使用 4 字节大端序长度前缀而不是特殊分隔符（如 \n）。
//    优点：(a) 不需要在 JSON 中进行转义；(b) 可精确预分配
//    缓冲区；(c) 帧边界明确，不会误分隔。
//
// 2. 重连策略（固定间隔 vs. 指数退避）
//    当前使用固定 5s 间隔，简化实现。指数退避（如 1s-2s-
//    4s-8s 封顶 30s）在服务端故障场景中更友好，可作为改进
//    方向。
//
// 3. 心跳机制
//    30s 定时发送 ping，服务端回复 pong。如果 TCP 本身检测
//    不到网络断开（如 WiFi 断连），心跳超时（收不到 pong）
//    可以触发主动断开和重连。当前简化版本只发送 ping 不检
//    测 pong 超时——因为 TCP RST 和底层保活机制已经能覆盖
//    大部分断线场景。
// ============================================================

#include "chatclient.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QDebug>

ChatClient &ChatClient::instance()
{
    static ChatClient client;
    return client;
}

ChatClient::ChatClient(QObject *parent)
    : QObject(parent)
    , m_socket(new QTcpSocket(this))
    , m_reconnectTimer(new QTimer(this))
    , m_pingTimer(new QTimer(this))
    , m_port(0)
    , m_userId(-1)
    , m_loggedIn(false)
    , m_intentionalDisconnect(false)
{
    m_reconnectTimer->setSingleShot(true);
    m_pingTimer->setInterval(30000);

    connect(m_socket, &QTcpSocket::connected, this, &ChatClient::onConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &ChatClient::onDisconnected);
    connect(m_socket, &QTcpSocket::readyRead, this, &ChatClient::onReadyRead);
    connect(m_socket, &QTcpSocket::errorOccurred, this, &ChatClient::onError);
    connect(m_reconnectTimer, &QTimer::timeout, this, &ChatClient::onReconnectTimer);
    connect(m_pingTimer, &QTimer::timeout, this, &ChatClient::onPingTimer);
}

void ChatClient::connectToServer(const QString &host, quint16 port)
{
    m_host = host;
    m_port = port;
    m_intentionalDisconnect = false;
    m_loggedIn = false;
    m_onlineUsers.clear();
    m_buffer.clear();

    qDebug() << "ChatClient: connecting to" << host << port;
    m_socket->connectToHost(host, port);
}

void ChatClient::disconnectFromServer()
{
    m_intentionalDisconnect = true;
    m_loggedIn = false;
    m_reconnectTimer->stop();
    m_pingTimer->stop();
    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        sendJson({{"type", "logout"}});
        m_socket->flush();
        m_socket->disconnectFromHost();
    }
}

bool ChatClient::isConnected() const
{
    return m_socket->state() == QAbstractSocket::ConnectedState && m_loggedIn;
}

bool ChatClient::isLoggedIn() const
{
    return m_loggedIn;
}

bool ChatClient::isUserOnline(int userId) const
{
    return m_onlineUsers.contains(userId);
}

void ChatClient::login(int userId, const QString &username)
{
    m_userId = userId;
    m_username = username;

    if (m_socket->state() == QAbstractSocket::ConnectedState) {
        sendJson({
            {"type", "login"},
            {"user_id", userId},
            {"username", username}
        });
    }
    // 如果还未连接, onConnected 会触发登录
}

void ChatClient::sendMessage(int toUserId, const QString &content)
{
    sendJson({
        {"type", "message"},
        {"to", toUserId},
        {"content", content}
    });
}

// ---- private slots ----

void ChatClient::onConnected()
{
    qDebug() << "ChatClient: connected to" << m_host << m_port;
    m_reconnectTimer->stop();
    emit connected();

    if (m_userId > 0 && !m_username.isEmpty())
        login(m_userId, m_username);
}

void ChatClient::onDisconnected()
{
    qDebug() << "ChatClient: disconnected";
    m_loggedIn = false;
    m_pingTimer->stop();
    emit disconnected();

    if (!m_intentionalDisconnect) {
        m_reconnectTimer->start(5000);
    }
}

// 【帧同步：长度前缀协议解析】
// TCP 是流式协议，不保证一次 read 返回完整的应用层消息。
// 因此需要 buffer 累积数据，通过长度前缀逐帧切割。
//
// 帧结构: [4-byte big-endian payload length][payload bytes]
//         └─ uint32, 网络字节序 ─┘
//
// 典型状态转换：
//   buffer.size() < 4          → 等待，未收到长度头
//   buffer.size() >= 4          → 解析出 len，检查 payload 完整性
//   buffer.size() < 4 + len     → 等待更多数据
//   buffer.size() >= 4 + len    → 取出完整帧，移除，继续检查下一个
void ChatClient::onReadyRead()
{
    m_buffer.append(m_socket->readAll());

    while (m_buffer.size() >= 4) {
        quint32 len = (static_cast<quint8>(m_buffer[0]) << 24) |
                      (static_cast<quint8>(m_buffer[1]) << 16) |
                      (static_cast<quint8>(m_buffer[2]) << 8)  |
                      static_cast<quint8>(m_buffer[3]);

        if (static_cast<quint32>(m_buffer.size()) < 4 + len)
            break;

        QByteArray payload = m_buffer.mid(4, len);
        m_buffer.remove(0, 4 + len);

        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(payload, &err);
        if (doc.isNull()) {
            qDebug() << "ChatClient: JSON parse error:" << err.errorString();
            continue;
        }

        processMessage(doc.object());
    }
}

void ChatClient::onError(QAbstractSocket::SocketError err)
{
    Q_UNUSED(err);
    qDebug() << "ChatClient: socket error:" << m_socket->errorString();
    if (!m_intentionalDisconnect && m_socket->state() != QAbstractSocket::ConnectedState) {
        m_reconnectTimer->start(5000);
    }
}

void ChatClient::onReconnectTimer()
{
    if (!m_intentionalDisconnect && m_socket->state() == QAbstractSocket::UnconnectedState) {
        qDebug() << "ChatClient: reconnecting...";
        m_socket->connectToHost(m_host, m_port);
    }
}

void ChatClient::onPingTimer()
{
    sendJson({{"type", "ping"}});
}

// ---- internal ----

void ChatClient::processMessage(const QJsonObject &obj)
{
    QString type = obj.value("type").toString();

    if (type == "login_ok") {
        m_loggedIn = true;
        m_pingTimer->start();
        emit loginSuccess();
        qDebug() << "ChatClient: login OK";
    } else if (type == "login_failed") {
        emit loginFailed(obj.value("reason").toString("登录失败"));
    } else if (type == "message") {
        int from = obj.value("from").toInt();
        QString fromName = obj.value("from_username").toString();
        QString content = obj.value("content").toString();
        qint64 ts = static_cast<qint64>(obj.value("timestamp").toDouble());
        emit messageReceived(from, fromName, content, ts);
    } else if (type == "message_ack") {
        // could track delivery status
    } else if (type == "error") {
        emit errorOccurred(obj.value("msg").toString("未知错误"));
    } else if (type == "pong") {
        // heartbeat OK
    } else if (type == "user_list") {
        QJsonArray users = obj.value("users").toArray();
        QSet<int> onlineSet;
        for (const QJsonValue &v : users) {
            QJsonObject u = v.toObject();
            int uid = u.value("user_id").toInt();
            if (uid > 0)
                onlineSet.insert(uid);
        }
        if (m_onlineUsers != onlineSet) {
            m_onlineUsers = onlineSet;
            emit userListChanged();
        }
    } else {
        qDebug() << "ChatClient: unknown message type:" << type;
    }
}

// 【编码：JSON + 长度前缀组帧】
// 手动构造大端序 4 字节长度头而非使用 htonl()，保持依赖简洁。
// QJsonDocument::Compact 确保 wire format 没有多余空白字符，
// 最小化网络传输量。
void ChatClient::sendJson(const QJsonObject &obj)
{
    if (m_socket->state() != QAbstractSocket::ConnectedState)
        return;

    QByteArray payload = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    quint32 len = static_cast<quint32>(payload.size());

    QByteArray frame;
    frame.append(static_cast<char>((len >> 24) & 0xFF));
    frame.append(static_cast<char>((len >> 16) & 0xFF));
    frame.append(static_cast<char>((len >> 8) & 0xFF));
    frame.append(static_cast<char>(len & 0xFF));
    frame.append(payload);

    m_socket->write(frame);
}
