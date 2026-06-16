// 引入聊天服务器的头文件，其中声明了 ChatServer 类
#include "chatserver.h"
// 引入全局配置文件，包含服务器地址、端口等常量定义
#include "config.h"
// 引入 JSON 文档处理类，用于序列化和反序列化 JSON 数据
#include <QJsonDocument>
// 引入 JSON 对象类，用于构建和解析 JSON 对象
#include <QJsonObject>
// 引入 JSON 数组类，用于构建和解析 JSON 数组（如在线用户列表）
#include <QJsonArray>
// 引入日期时间类，用于生成消息时间戳
#include <QDateTime>
// 引入文件操作类，用于读取上传和存储的图片文件
#include <QFile>
// 引入文件信息类，用于获取文件后缀名等元数据
#include <QFileInfo>
// 引入 URL 处理类，用于解析 HTTP 请求中的路径和查询参数
#include <QUrl>
// 引入 URL 查询参数解析类，用于提取 HTTP 请求中的查询参数
#include <QUrlQuery>
// 引入随机数生成器类，用于生成唯一文件名
#include <QRandomGenerator>
// 引入调试输出头文件，用于打印服务器日志
#include <QDebug>

// ChatServer 构造函数
// 参数 parent：Qt 父对象，用于内存管理
ChatServer::ChatServer(QObject *parent)
    // 调用 QObject 基类构造函数
    : QObject(parent)
    // 创建 TCP 服务器实例，用于处理聊天客户端连接
    , m_server(new QTcpServer(this))
    // 创建另一个 TCP 服务器实例，用于处理 HTTP 文件上传/下载请求
    , m_httpServer(new QTcpServer(this))
{
    // 连接 TCP 聊天服务器的 newConnection 信号到 onNewConnection 槽
    // 当有聊天客户端连接时，自动调用 onNewConnection 处理
    connect(m_server, &QTcpServer::newConnection, this, &ChatServer::onNewConnection);
    // 连接 HTTP 服务器的 newConnection 信号到 onHttpNewConnection 槽
    // 当有 HTTP 客户端连接时，自动调用 onHttpNewConnection 处理
    connect(m_httpServer, &QTcpServer::newConnection, this, &ChatServer::onHttpNewConnection);
}

// ChatServer 析构函数
// 清理所有客户端连接和服务器资源
ChatServer::~ChatServer()
{
    // 遍历所有聊天客户端，逐个断开连接
    for (auto it = m_clients.begin(); it != m_clients.end(); ++it)
        it.key()->disconnectFromHost();  // 断开客户端 socket 连接
    // 关闭 TCP 聊天服务器，停止监听
    m_server->close();
    // 关闭 HTTP 服务器，停止监听
    m_httpServer->close();
}

// 启动 ChatServer，在指定端口上监听两种协议的连接
// 参数 tcpPort：聊天 TCP 协议的监听端口（如 8888）
// 参数 httpPort：HTTP 文件服务的监听端口（如 9999）
// 返回值：两个端口都绑定成功返回 true，否则返回 false
bool ChatServer::start(quint16 tcpPort, quint16 httpPort)
{
    // 尝试在 TCP 聊天端口上监听所有网络接口（0.0.0.0）
    if (!m_server->listen(QHostAddress::Any, tcpPort)) {
        // 如果监听失败，输出错误信息，包含端口号和具体错误描述
        qDebug() << "ChatServer: failed to listen on TCP port" << tcpPort
                 << m_server->errorString();
        return false;  // 启动失败
    }
    // 输出 TCP 服务启动成功的日志
    qDebug() << "ChatServer: TCP listening on port" << tcpPort;

    // 尝试在 HTTP 端口上监听所有网络接口
    if (!m_httpServer->listen(QHostAddress::Any, httpPort)) {
        // 如果监听失败，输出错误信息
        qDebug() << "ChatServer: failed to listen on HTTP port" << httpPort
                 << m_httpServer->errorString();
        return false;  // 启动失败
    }
    // 输出 HTTP 服务启动成功的日志
    qDebug() << "ChatServer: HTTP listening on port" << httpPort;

    // 初始化图片存储目录，名称为 "images"
    m_imagesDir = QDir("images");
    // 如果 images 目录不存在，则递归创建该目录及其父目录
    if (!m_imagesDir.exists())
        m_imagesDir.mkpath(".");

    return true;  // 所有服务启动成功
}

// ==================== TCP Chat ====================
// TCP 聊天协议处理部分：管理客户端连接、消息路由和用户状态

// 处理新的 TCP 聊天连接事件
// 当有聊天客户端连接到 TCP 端口时自动调用
void ChatServer::onNewConnection()
{
    // 循环取出所有待处理的挂起连接（可能同时有多个客户端连接）
    while (QTcpSocket *socket = m_server->nextPendingConnection()) {
        // 在日志中记录新连接的客户端 IP 地址和端口号
        qDebug() << "ChatServer: new TCP connection from"
                 << socket->peerAddress().toString() << socket->peerPort();

        // 连接 socket 的 readyRead 信号：当客户端发送数据时触发 onReadyRead
        connect(socket, &QTcpSocket::readyRead, this, &ChatServer::onReadyRead);
        // 连接 socket 的 disconnected 信号：断开连接时触发 onDisconnected
        connect(socket, &QTcpSocket::disconnected, this, &ChatServer::onDisconnected);
        // 连接 socket 的 errorOccurred 信号：发生错误时触发 onError
        connect(socket, &QTcpSocket::errorOccurred, this, &ChatServer::onError);

        // 将新客户端添加到 m_clients 映射表中
        // 键为 QSslSocket* 指针，值为 ClientInfo 结构体
        // 初始状态：userId 为 -1（未登录），username 为空，buffer 为空
        m_clients.insert(socket, {socket, -1, QString(), QByteArray()});
    }
}

// 处理来自聊天客户端的可读数据事件
// 从 socket 读取数据，解析 4 字节长度前缀的 JSON 协议帧
void ChatServer::onReadyRead()
{
    // 通过 sender() 获取触发信号的 socket 对象，并转换为 QTcpSocket 类型
    auto *socket = qobject_cast<QTcpSocket*>(sender());
    // 类型转换失败时直接返回（安全性检查）
    if (!socket) return;

    // 在 m_clients 映射表中查找对应的客户端信息
    auto it = m_clients.find(socket);
    // 如果找不到该客户端记录，直接返回（安全性检查）
    if (it == m_clients.end()) return;

    // 将 socket 中所有可读数据追加到该客户端的接收缓冲区
    it->buffer.append(socket->readAll());

    // 循环处理缓冲区中的完整协议帧
    // 协议格式：[4 字节长度][JSON 负载]
    while (it->buffer.size() >= 4) {
        // 从缓冲区前 4 字节解析出负载长度（大端序网络字节序）
        quint32 len = (static_cast<quint8>(it->buffer[0]) << 24) |
                      (static_cast<quint8>(it->buffer[1]) << 16) |
                      (static_cast<quint8>(it->buffer[2]) << 8)  |
                      static_cast<quint8>(it->buffer[3]);

        // 如果缓冲区的数据量不足一个完整的帧（4 字节头部 + 负载），则等待更多数据
        if (static_cast<quint32>(it->buffer.size()) < 4 + len) break;

        // 提取 4 字节头部之后的负载数据（JSON 文本）
        QByteArray payload = it->buffer.mid(4, len);
        // 从缓冲区中移除已处理完的整个帧（4 字节头部 + 负载长度）
        it->buffer.remove(0, 4 + len);

        // 尝试将负载数据解析为 JSON 文档
        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(payload, &err);
        // 如果 JSON 解析失败（如格式错误、不完整等）
        if (doc.isNull()) {
            // 输出 JSON 解析错误日志，帮助客户端排查问题
            qDebug() << "ChatServer: JSON parse error:" << err.errorString();
            continue;  // 跳过此帧，继续处理缓冲区中的剩余数据
        }

        // 将解析成功的 JSON 对象交由 processMessage 处理
        // 参数：当前 socket、对应的客户端信息引用、JSON 消息对象
        processMessage(socket, it.value(), doc.object());
    }
}

// 处理聊天客户端断开连接事件
// 清理用户状态并通知其他在线用户
void ChatServer::onDisconnected()
{
    // 获取断开连接的 socket 对象
    auto *socket = qobject_cast<QTcpSocket*>(sender());
    // 类型转换失败时直接返回
    if (!socket) return;

    // 在 m_clients 映射表中查找该 socket
    auto it = m_clients.find(socket);
    // 如果找到了该客户端的记录
    if (it != m_clients.end()) {
        // 获取该客户端的用户 ID（可能为 -1 表示未登录）
        int userId = it->userId;
        // 输出客户端断开连接的日志，包含用户 ID 和用户名
        qDebug() << "ChatServer: TCP client disconnected, userId:" << userId << it->username;

        // 如果用户已登录（userId > 0），需要清理该用户的映射关系
        if (userId > 0) {
            // 从 user_id -> socket 的映射表中移除该用户
            m_userToSocket.remove(userId);
            // 广播更新后的在线用户列表给所有在线用户
            broadcastUserList();
        }
        // 从 m_clients 映射表中删除该客户端的记录
        m_clients.erase(it);
    }
    // 发送 deleteLater 信号，由 Qt 事件循环在适当时机释放 socket 对象
    socket->deleteLater();
}

// 处理 Socket 错误事件
// 参数 err：Qt 定义的 socket 错误枚举值
void ChatServer::onError(QAbstractSocket::SocketError err)
{
    // 获取发生错误的 socket 对象
    auto *socket = qobject_cast<QTcpSocket*>(sender());
    // 如果 socket 有效，输出具体的错误描述信息
    if (socket)
        qDebug() << "ChatServer: socket error:" << socket->errorString();
    // 标记错误枚举参数为未使用，避免编译器警告
    Q_UNUSED(err);
}

// 处理解析后的 JSON 消息
// 根据消息类型（type 字段）执行不同的业务逻辑
// 参数 socket：发送消息的客户端 socket
// 参数 info：发送消息的客户端信息（引用，可修改）
// 参数 obj：解析后的 JSON 消息对象
void ChatServer::processMessage(QTcpSocket *socket, ClientInfo &info, const QJsonObject &obj)
{
    // 从 JSON 对象中提取消息类型字段
    QString type = obj.value("type").toString();

    // --- 登录处理 ---
    // 客户端发送 login 消息进行身份认证
    if (type == "login") {
        // 从 JSON 中提取用户 ID
        int userId = obj.value("user_id").toInt();
        // 从 JSON 中提取用户名
        QString username = obj.value("username").toString();

        // 校验用户信息的合法性：ID 必须大于 0，用户名不能为空
        if (userId <= 0 || username.isEmpty()) {
            // 参数校验失败，向客户端发送登录失败消息
            sendJson(socket, {
                {"type", "login_failed"},   // 消息类型：登录失败
                {"reason", "无效的用户信息"} // 失败原因：无效信息
            });
            return;  // 终止处理
        }

        // 【踢除旧连接：防止同一账号多点登录】
        // 在桌面应用场景中，通常不希望同一账号在多个设备同时
        // 在线（不像微信那样支持多设备共存）。这里采用"后登
        // 录踢先登录"的策略——新登录会向旧连接发送 "您的账号
        // 在其他设备登录" 消息后断开它。
        // 检查该用户是否已有其他在线连接
        QTcpSocket *oldSocket = m_userToSocket.value(userId);
        // 如果存在旧连接，且旧连接不是当前这个新连接
        if (oldSocket && oldSocket != socket) {
            // 向旧连接发送被踢出消息
            sendJson(oldSocket, {
                {"type", "error"},              // 消息类型：错误
                {"msg", "您的账号在其他设备登录"} // 提示信息
            });
            // 查找旧连接对应的客户端信息
            auto oldIt = m_clients.find(oldSocket);
            if (oldIt != m_clients.end()) {
                // 将旧连接的 userId 置为 -1（标记为未登录）
                oldIt->userId = -1;
                // 清空旧连接的用户名
                oldIt->username.clear();
            }
            // 强制断开旧连接
            oldSocket->disconnectFromHost();
        }

        // 将当前客户端的用户信息更新到 ClientInfo 中
        info.userId = userId;         // 设置用户 ID
        info.username = username;     // 设置用户名
        // 在 user_id -> socket 映射表中注册该用户
        m_userToSocket[userId] = socket;

        // 向登录成功的客户端发送确认消息
        sendJson(socket, {{"type", "login_ok"}});
        // 输出用户登录成功的日志
        qDebug() << "ChatServer: user logged in:" << userId << username;
        // 广播更新后的在线用户列表给所有用户
        broadcastUserList();

    // --- 登出处理 ---
    // 客户端发送 logout 消息主动退出登录
    } else if (type == "logout") {
        // 检查用户是否已登录（userId > 0）
        if (info.userId > 0) {
            // 从 user_id -> socket 映射表中移除该用户
            m_userToSocket.remove(info.userId);
            // 广播更新后的在线用户列表
            broadcastUserList();
        }
        // 将当前客户端的 userId 置为 -1，标记为未登录状态
        info.userId = -1;

    // --- 消息转发处理 ---
    // 客户端发送 message 消息进行点对点聊天
    } else if (type == "message") {
        // 检查发送者是否已登录（未登录用户不能发送消息）
        if (info.userId <= 0) {
            // 未登录则返回错误消息
            sendJson(socket, {
                {"type", "error"},   // 消息类型：错误
                {"msg", "请先登录"}   // 错误提示
            });
            return;  // 终止处理
        }

        // 从 JSON 中提取目标用户 ID（接收方）
        int to = obj.value("to").toInt();
        // 从 JSON 中提取消息内容
        QString content = obj.value("content").toString();

        // 校验目标用户 ID 和消息内容的合法性
        if (to <= 0 || content.isEmpty()) {
            // 参数校验失败，返回错误消息
            sendJson(socket, {
                {"type", "error"},     // 消息类型：错误
                {"msg", "消息格式错误"}  // 错误提示
            });
            return;  // 终止处理
        }

        // 在映射表中查找目标用户的 socket 连接
        QTcpSocket *targetSocket = m_userToSocket.value(to);
        // 获取当前时间的 Unix 时间戳（秒级）
        qint64 now = QDateTime::currentSecsSinceEpoch();

        // 如果目标用户在线且连接状态正常
        if (targetSocket && targetSocket->state() == QAbstractSocket::ConnectedState) {
            // 向目标用户转发消息
            sendJson(targetSocket, {
                {"type", "message"},                       // 消息类型：消息
                {"from", info.userId},                     // 发送者 ID
                {"from_username", info.username},           // 发送者用户名
                {"content", content},                      // 消息内容
                {"timestamp", static_cast<double>(now)}     // 服务器时间戳
            });
        }

        // 向发送者返回消息确认（ack），表示服务器已成功处理该消息
        sendJson(socket, {
            {"type", "message_ack"},                    // 消息类型：确认
            {"to", to},                                 // 目标用户 ID
            {"timestamp", static_cast<double>(now)}      // 服务器时间戳
        });

    // --- 心跳处理 ---
    // 客户端发送 ping 消息检测连接是否存活
    } else if (type == "ping") {
        // 回复 pong 消息，表示服务器正常运行
        sendJson(socket, {{"type", "pong"}});

    // --- 未知消息类型处理 ---
    } else {
        // 在日志中输出未知消息类型，方便调试
        qDebug() << "ChatServer: unknown message type:" << type;
    }
}

// 广播在线用户列表给所有已登录的客户端
// 用于在用户登录/登出时同步更新各客户端的联系人状态
void ChatServer::broadcastUserList()
{
    // 构建在线用户 JSON 数组
    QJsonArray arr;
    // 遍历 user_id -> socket 映射表，收集所有在线用户的信息
    for (auto it = m_userToSocket.begin(); it != m_userToSocket.end(); ++it) {
        // 根据 socket 查找对应的客户端信息
        auto ci = m_clients.find(it.value());
        // 创建单个用户的 JSON 对象
        QJsonObject u;
        u["user_id"] = it.key();             // 用户 ID
        u["username"] = (ci != m_clients.end()) ? ci->username : QString();  // 用户名（可能为空）
        arr.append(u);  // 将用户对象添加到数组中
    }

    // 构造完整的用户列表消息
    QJsonObject msg;
    msg["type"] = "user_list";   // 消息类型：用户列表
    msg["users"] = arr;          // 在线用户 JSON 数组

    // 向所有已登录的客户端广播该用户列表消息
    for (auto it = m_clients.begin(); it != m_clients.end(); ++it) {
        // 只发送给已登录（userId > 0）且连接状态正常的客户端
        if (it->userId > 0 && it->socket->state() == QAbstractSocket::ConnectedState)
            sendJson(it->socket, msg);  // 发送用户列表
    }
}

// 向指定客户端发送 JSON 格式的消息
// 使用 4 字节大端序长度前缀 + JSON 负载的协议格式
// 参数 socket：目标客户端 socket
// 参数 obj：要发送的 JSON 消息对象
void ChatServer::sendJson(QTcpSocket *socket, const QJsonObject &obj)
{
    // 检查 socket 是否有效且连接状态正常，否则跳过发送
    if (!socket || socket->state() != QAbstractSocket::ConnectedState) return;

    // 将 JSON 对象序列化为紧凑格式（无多余空格和换行）的字节数组
    QByteArray payload = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    // 获取负载数据的长度
    quint32 len = static_cast<quint32>(payload.size());

    // 构造协议帧：4 字节长度头部 + JSON 负载
    QByteArray frame;
    // 将 32 位长度值按大端序拆分为 4 个字节
    frame.append(static_cast<char>((len >> 24) & 0xFF));  // 最高 8 位
    frame.append(static_cast<char>((len >> 16) & 0xFF));  // 次高 8 位
    frame.append(static_cast<char>((len >> 8) & 0xFF));   // 次低 8 位
    frame.append(static_cast<char>(len & 0xFF));           // 最低 8 位
    frame.append(payload);  // 追加 JSON 负载数据

    // 将完整帧写入 socket，发送给客户端
    socket->write(frame);
}

// ==================== HTTP File Server ====================
// HTTP 文件服务部分：支持图片上传（POST）和图片下载（GET）

// 处理新的 HTTP 连接事件
// 当有 HTTP 客户端连接到文件服务端口时自动调用
void ChatServer::onHttpNewConnection()
{
    // 循环取出所有待处理的挂起 HTTP 连接
    while (QTcpSocket *socket = m_httpServer->nextPendingConnection()) {
        // 在日志中记录新的 HTTP 连接来源地址
        qDebug() << "ChatServer: new HTTP connection from"
                 << socket->peerAddress().toString();

        // 连接 readyRead 信号：当 HTTP 请求数据到达时触发 onHttpReadyRead
        connect(socket, &QTcpSocket::readyRead, this, &ChatServer::onHttpReadyRead);
        // 连接 disconnected 信号：断开连接时触发 onHttpDisconnected
        connect(socket, &QTcpSocket::disconnected, this, &ChatServer::onHttpDisconnected);

        // 将新的 HTTP 客户端添加到 m_httpClients 映射表
        // 初始状态：缓冲区为空，请求方法/路径为空，内容长度为 0，头部未解析
        m_httpClients.insert(socket, {socket, QByteArray(), QString(), QString(), 0, false});
    }
}

// 处理 HTTP 请求数据的读取事件
// 解析 HTTP 请求头，并在接收到完整请求体后调用 handleHttpRequest
void ChatServer::onHttpReadyRead()
{
    // 通过 sender() 获取触发信号的 socket 对象
    auto *socket = qobject_cast<QTcpSocket*>(sender());
    // 类型转换失败时直接返回
    if (!socket) return;

    // 在 m_httpClients 映射表中查找对应的 HTTP 客户端信息
    auto it = m_httpClients.find(socket);
    // 如果找不到记录，直接返回
    if (it == m_httpClients.end()) return;

    // 将 socket 中所有可读数据追加到该 HTTP 客户端的接收缓冲区
    it->buffer.append(socket->readAll());

    // 如果头部尚未解析完成，则尝试解析 HTTP 请求头
    if (!it->headersParsed) {
        // 查找头部结束标记（连续的两个 \r\n，即空行）
        int headerEnd = it->buffer.indexOf("\r\n\r\n");
        // 如果找不到头部结束标记，说明数据还不完整，等待更多数据
        if (headerEnd == -1) return;

        // 提取请求头部分（头部结束标记之前的内容）
        QByteArray headerBlock = it->buffer.left(headerEnd);
        // 将头部按行拆分
        QStringList lines = QString::fromUtf8(headerBlock).split("\r\n");
        // 如果没有任何行，说明请求格式错误
        if (lines.isEmpty()) {
            // 返回 400 Bad Request 响应
            sendHttpResponse(socket, 400, "Bad Request", "text/plain", "Bad Request");
            // 从映射表中移除该客户端记录
            m_httpClients.erase(it);
            // 断开连接
            socket->disconnectFromHost();
            return;
        }

        // 解析请求行，格式："GET /path HTTP/1.1"
        QStringList requestLine = lines[0].split(' ');
        // 请求行至少应包含方法和路径两部分
        if (requestLine.size() < 2) {
            // 格式错误，返回 400
            sendHttpResponse(socket, 400, "Bad Request", "text/plain", "Bad Request");
            m_httpClients.erase(it);
            socket->disconnectFromHost();
            return;
        }

        // 提取 HTTP 方法（GET/POST 等）并转为大写
        it->method = requestLine[0].toUpper();
        // 提取请求路径并进行 URL 解码（处理中文字符和特殊字符）
        it->path = QUrl::fromPercentEncoding(requestLine[1].toUtf8());

        // 解析其余头部行，提取 Content-Length 字段
        it->contentLength = 0;
        for (int i = 1; i < lines.size(); ++i) {
            // 查找 Content-Length 字段（不区分大小写）
            if (lines[i].startsWith("Content-Length:", Qt::CaseInsensitive)) {
                // 提取 Content-Length 的值（跳过 "Content-Length: " 前缀）
                it->contentLength = lines[i].mid(15).trimmed().toLongLong();
            }
        }

        // 标记头部已解析完成
        it->headersParsed = true;

        // 从缓冲区中移除已经解析过的头部数据，只保留请求体
        it->buffer.remove(0, headerEnd + 4);
    }

    // 检查是否已收到完整的请求体
    if (it->headersParsed) {
        // 如果 Content-Length > 0 但缓冲区中的数据量还不足，继续等待
        if (it->contentLength > 0 && it->buffer.size() < it->contentLength)
            return;

        // 收到完整的 HTTP 请求，调用 handleHttpRequest 进行处理
        handleHttpRequest(it.value());

        // 处理完成后从映射表中移除该客户端记录
        m_httpClients.erase(it);
        // 断开与该 HTTP 客户端的连接
        socket->disconnectFromHost();
    }
}

// 处理 HTTP 客户端断开连接事件
// 清理 m_httpClients 映射表中的记录
void ChatServer::onHttpDisconnected()
{
    // 获取断开连接的 socket 对象
    auto *socket = qobject_cast<QTcpSocket*>(sender());
    // 类型转换失败时直接返回
    if (!socket) return;

    // 在 m_httpClients 映射表中查找该 socket
    auto it = m_httpClients.find(socket);
    // 如果找到记录，从映射表中删除
    if (it != m_httpClients.end())
        m_httpClients.erase(it);
    // 发送 deleteLater，在事件循环中安全释放 socket 对象
    socket->deleteLater();
}

// 处理 HTTP 请求，根据请求方法和路径分发到不同的处理逻辑
// 支持 POST /upload（图片上传）和 GET /images/（图片下载）
// 参数 info：HTTP 客户端信息结构体的引用
void ChatServer::handleHttpRequest(HttpClientInfo &info)
{
    // 判断是否为 POST 请求且路径为 /upload（图片上传）
    if (info.method == "POST" && info.path == "/upload") {
        // --- Image upload ---
        // 检查请求体是否为空（没有上传数据）
        if (info.buffer.isEmpty()) {
            // 返回 400 Bad Request，提示数据为空
            sendHttpResponse(info.socket, 400, "Bad Request",
                             "application/json", R"({"error":"empty data"})");
            return;
        }

        // 从请求路径的查询参数中获取原始文件名
        QUrl url(info.path);
        QString originalName = QUrlQuery(url).queryItemValue("filename");
        // 如果未提供文件名参数，使用默认文件名 "upload.jpg"
        if (originalName.isEmpty())
            originalName = "upload.jpg";

        // 将上传的图片数据保存到服务器文件系统，返回 URL 路径
        QString urlPath = saveUploadedFile(info.buffer, originalName);
        // 构造完整的图片访问 URL
        QString fullUrl = QString("http://%1:%2%3")
                              .arg(IMAGE_SERVER_HOST)   // 服务器主机名（来自 config.h）
                              .arg(IMAGE_SERVER_PORT)   // 服务器端口（来自 config.h）
                              .arg(urlPath);             // 图片路径

        // 构造 JSON 响应对象，包含可访问的图片 URL
        QJsonObject resp;
        resp["url"] = fullUrl;
        // 将 JSON 响应序列化为字节数组
        QByteArray body = QJsonDocument(resp).toJson(QJsonDocument::Compact);
        // 返回 200 OK，内容类型为 application/json
        sendHttpResponse(info.socket, 200, "OK", "application/json", body);

    // 判断是否为 GET 请求且路径以 /images/ 开头（图片下载）
    } else if (info.method == "GET" && info.path.startsWith("/images/")) {
        // --- Serve image file ---
        // 提取路径中的文件名（去掉 /images/ 前缀）
        QString filename = info.path.mid(QString("/images/").length());
        // 校验文件名不能为空，且不能包含 ".."（防止目录遍历攻击）
        if (filename.isEmpty() || filename.contains("..")) {
            // 安全校验失败，返回 404 Not Found
            sendHttpResponse(info.socket, 404, "Not Found",
                             "text/plain", "Not Found");
            return;
        }

        // 拼接图片文件的完整系统路径
        QString filePath = m_imagesDir.absoluteFilePath(filename);
        // 打开图片文件以只读方式
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly)) {
            // 文件不存在或无法打开，返回 404 Not Found
            sendHttpResponse(info.socket, 404, "Not Found",
                             "text/plain", "Not Found");
            return;
        }

        // 读取文件的全部数据到字节数组
        QByteArray data = file.readAll();
        // 关闭文件
        file.close();

        // 根据文件后缀名确定 MIME 类型
        QString ext = QFileInfo(filename).suffix().toLower();
        QString mime = "application/octet-stream";  // 默认二进制流类型
        if (ext == "jpg" || ext == "jpeg") mime = "image/jpeg";   // JPEG 图片
        else if (ext == "png") mime = "image/png";                 // PNG 图片
        else if (ext == "gif") mime = "image/gif";                 // GIF 图片
        else if (ext == "bmp") mime = "image/bmp";                 // BMP 图片
        else if (ext == "webp") mime = "image/webp";               // WebP 图片

        // 返回 200 OK，设置正确的 MIME 类型和文件数据
        sendHttpResponse(info.socket, 200, "OK", mime, data);

    } else {
        // 请求的方法或路径不匹配任何已知路由，返回 404
        sendHttpResponse(info.socket, 404, "Not Found",
                         "text/plain", "Not Found");
    }
}

// 发送 HTTP 响应到客户端
// 参数 socket：客户端 socket 连接
// 参数 statusCode：HTTP 状态码（如 200、400、404）
// 参数 statusText：状态码对应的文本描述（如 "OK"、"Not Found"）
// 参数 contentType：响应体的 MIME 类型
// 参数 body：响应体的字节数据
void ChatServer::sendHttpResponse(QTcpSocket *socket, int statusCode,
                                   const QString &statusText,
                                   const QString &contentType,
                                   const QByteArray &body)
{
    // 构建完整的 HTTP 响应报文
    QByteArray response;
    // 状态行：HTTP 版本、状态码、状态文本
    response.append(QString("HTTP/1.1 %1 %2\r\n").arg(statusCode).arg(statusText).toUtf8());
    // Content-Type 头：指明响应体的数据类型
    response.append(QString("Content-Type: %1\r\n").arg(contentType).toUtf8());
    // Content-Length 头：指明响应体的字节数
    response.append(QString("Content-Length: %1\r\n").arg(body.size()).toUtf8());
    // Connection 头：响应完成后关闭连接（短连接模式）
    response.append("Connection: close\r\n");
    // 头部结束的空行（\r\n）
    response.append("\r\n");
    // 响应体数据
    response.append(body);

    // 将完整的 HTTP 响应写入 socket
    socket->write(response);
    // 强制刷新 socket 缓冲区，立即发送数据
    socket->flush();
}

// 保存上传的图片文件到服务器文件系统
// 参数 data：图片文件的原始二进制数据
// 参数 originalFilename：上传时的原始文件名（用于获取扩展名）
// 返回值：可公开访问的 URL 路径（如 "/images/123456_78901.jpg"）
QString ChatServer::saveUploadedFile(const QByteArray &data, const QString &originalFilename)
{
    // 从原始文件名中提取扩展名（如 "photo.png" 得到 "png"）
    QString ext = QFileInfo(originalFilename).suffix();
    // 如果没有扩展名，默认使用 jpg
    if (ext.isEmpty()) ext = "jpg";

    // 生成唯一文件名，避免文件名冲突
    // 格式：时间戳_随机数.扩展名
    qint64 ts = QDateTime::currentMSecsSinceEpoch();  // 当前毫秒级时间戳
    int rnd = QRandomGenerator::global()->bounded(10000, 99999);  // 5 位随机数
    QString filename = QString("%1_%2.%3").arg(ts).arg(rnd).arg(ext);

    // 拼接完整的文件系统路径（在 images 目录下）
    QString filePath = m_imagesDir.absoluteFilePath(filename);
    // 创建 QFile 对象并尝试以只写模式打开
    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly)) {
        // 写入图片数据到文件
        file.write(data);
        // 关闭文件
        file.close();
        // 输出保存成功的日志
        qDebug() << "ChatServer: saved image" << filePath;
    } else {
        // 输出保存失败的警告日志
        qWarning() << "ChatServer: failed to save image" << filePath;
    }

    // 返回可公开访问的 URL 路径（不包含域名和端口）
    return QString("/images/%1").arg(filename);
}
