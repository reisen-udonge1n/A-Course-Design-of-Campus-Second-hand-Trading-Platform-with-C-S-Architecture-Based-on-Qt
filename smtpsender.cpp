// ============================================================
// SMTP 有限状态机实现
//
// 协议概览（RFC 5321 / RFC 4954）：
//   C: EHLO localhost
//   S: 250-...
//   C: AUTH LOGIN
//   S: 334 VXNlcm5hbWU6
//   C: <base64-email>
//   S: 334 UGFzc3dvcmQ6
//   C: <base64-password>
//   S: 235 Authentication successful
//   C: MAIL FROM:<sender>
//   S: 250 OK
//   C: RCPT TO:<recipient>
//   S: 250 OK
//   C: DATA
//   S: 354 End data with <CRLF>.<CRLF>
//   C: <email body + .>
//   S: 250 OK
//   C: QUIT
//
// 每步都在 processResponse() 中校验服务端状态码：
//   2xx / 3xx → 继续下一步
//   其他      → 终止并报告错误
//
// 多行响应处理：SMTP 的多行响应形如 "250-..."（第三个字符
// 为连字符），只有最后一行 "250 ..."（空格分隔）才触发
// 状态转换。
// ============================================================

// 引入 SmtpSender 类的头文件声明
#include "smtpsender.h"
// 引入 Qt 调试输出头文件，用于打印日志
#include <QDebug>

// SmtpSender 构造函数
// 参数 fromEmail：发件人邮箱地址（如 service@126.com）
// 参数 password：发件人邮箱的 SMTP 授权码（非登录密码）
// 参数 smtpHost：SMTP 服务器地址（如 smtp.126.com）
// 参数 smtpPort：SMTP 服务器端口（如 465）
// 参数 parent：Qt 父对象，用于内存管理
SmtpSender::SmtpSender(const QString &fromEmail, const QString &password,
                       const QString &smtpHost, int smtpPort, QObject *parent)
    // 调用 QObject 基类构造函数，设置父对象
    : QObject(parent)
    // 创建 QSslSocket 实例，用于建立 SSL/TLS 加密的 SMTP 连接
    , m_socket(new QSslSocket(this))
    // 创建 QTimer 实例，用于实现 SMTP 操作超时机制
    , m_timer(new QTimer(this))
    // 初始化状态机为 Init（初始状态，尚未发送任何命令）
    , m_state(Init)
    // 保存 SMTP 服务器地址
    , m_smtpHost(smtpHost)
    // 保存 SMTP 服务器端口
    , m_smtpPort(smtpPort)
    // 保存发件人邮箱地址
    , m_fromEmail(fromEmail)
    // 保存发件人邮箱 SMTP 授权码
    , m_password(password)
{
    // 将定时器设置为单次触发模式，超时后只触发一次，不会循环
    m_timer->setSingleShot(true);
    // 连接定时器的 timeout 信号到 onTimeout 槽函数，处理超时
    connect(m_timer, &QTimer::timeout, this, &SmtpSender::onTimeout);
    // 连接 socket 的 connected 信号到 onConnected 槽函数，处理连接建立
    connect(m_socket, &QSslSocket::connected, this, &SmtpSender::onConnected);
    // 连接 socket 的 readyRead 信号到 onReadyRead 槽函数，处理服务器响应
    connect(m_socket, &QSslSocket::readyRead, this, &SmtpSender::onReadyRead);
    // 连接 socket 的 errorOccurred 信号到 onError 槽函数，处理连接异常
    connect(m_socket, &QSslSocket::errorOccurred, this, &SmtpSender::onError);
}

// 发送验证码邮件
// 参数 toEmail：收件人邮箱地址
// 参数 code：6 位数字验证码
// 该方法是 SmtpSender 的入口点，外部调用此方法触发邮件发送流程
void SmtpSender::sendVerificationCode(const QString &toEmail, const QString &code)
{
    // 保存收件人邮箱地址到成员变量，供后续 SMTP 命令使用
    m_toEmail = toEmail;
    // 保存验证码到成员变量，用于拼装邮件正文
    m_code = code;
    // 将状态机重置为 Init 状态，开始新的 SMTP 会话流程
    m_state = Init;
    // 清空接收缓冲区，为新的响应数据做准备
    m_buffer.clear();

    // 启动超时定时器，设置 10 秒超时
    // 如果 10 秒内没有完成任何步骤，将触发超时处理
    m_timer->start(10000);
    // 向 SMTP 服务器发起 SSL 加密连接
    // 连接成功后会自动触发 onConnected 信号
    m_socket->connectToHostEncrypted(m_smtpHost, m_smtpPort);
}

// 处理 SSL 连接成功事件
// 当 QSslSocket 成功完成 SSL 握手后自动调用此槽函数
void SmtpSender::onConnected()
{
    // 在调试控制台输出连接成功信息
    qDebug() << "SMTP 连接成功";
    // 等待服务器发送问候消息，由 onReadyRead / processResponse 触发下一步
    // SMTP 协议规定：连接建立后服务器会先发送 220 问候消息
    // 该消息会被 onReadyRead 接收并调用 processResponse 处理
}

// 处理服务器响应数据的读取事件
// 当 socket 接收到服务器发来的数据时自动调用
void SmtpSender::onReadyRead()
{
    // 有数据到达，重置超时定时器（延长 10 秒）
    m_timer->start(10000);
    // 将 socket 中所有可读数据追加到缓冲区
    m_buffer += QString::fromUtf8(m_socket->readAll());

    // 循环处理缓冲区中所有完整的行（以 \r\n 结尾）
    while (m_buffer.contains("\r\n")) {
        // 查找第一个 \r\n 的位置
        int idx = m_buffer.indexOf("\r\n");
        // 提取该行内容（不含 \r\n）
        QString line = m_buffer.left(idx);
        // 从缓冲区中移除已处理的行（包括 \r\n）
        m_buffer = m_buffer.mid(idx + 2);
        // 如果该行不为空，则交由 processResponse 处理
        if (!line.isEmpty())
            processResponse(line);
    }
}

// 处理 SMTP 服务器的单行响应
// 参数 line：服务器返回的一行响应文本
// SMTP 响应格式：三位状态码 + 空格/连字符 + 附加信息
void SmtpSender::processResponse(const QString &line)
{
    // 在调试控制台输出收到的服务器响应
    qDebug() << "SMTP <" << line;

    // 多行响应中形如 "250-..." 的是续行，其中第三个字符是连字符 '-'
    // 只有最后一行（第三个字符是空格）才触发状态转换
    if (line.length() >= 4 && line[3] == '-')
        return;  // 跳过续行，继续等待最后一行

    // 获取响应状态码的第一位字符（如 '2'、'3'、'4'、'5'）
    QChar code = line[0];
    // 如果状态码以 2 或 3 开头，表示成功或需要继续输入
    if (code == '2' || code == '3') {
        sendNext();  // 发送 SMTP 会话的下一步命令
    } else {
        // 状态码以 4 或 5 开头，表示临时或永久错误
        m_timer->stop();  // 停止超时定时器
        // 构造错误消息，包含服务器返回的完整响应行
        QString errMsg = QString("SMTP 错误: %1").arg(line);
        // 在调试控制台输出错误信息
        qDebug() << errMsg;
        // 断开与 SMTP 服务器的连接
        m_socket->disconnectFromHost();
        // 发送失败信号，通知调用方邮件发送失败及原因
        emit finished(false, errMsg);
    }
}

// SMTP 有限状态机：发送下一步命令
// 根据当前状态 m_state 决定发送什么 SMTP 命令，并转换到下一状态
void SmtpSender::sendNext()
{
    // 基于当前状态执行对应的 SMTP 命令
    switch (m_state) {
    case Init:
        // 状态：初始 —— 发送 EHLO 命令，开始 SMTP 会话
        m_state = EhloSent;                  // 转换到 EHLO 已发送状态
        m_socket->write("EHLO localhost\r\n"); // 发送 EHLO 命令
        break;

    case EhloSent:
        // 状态：EHLO 已发送且成功 —— 发送 AUTH LOGIN 开始身份验证
        m_state = AuthLoginSent;             // 转换到 AUTH LOGIN 已发送状态
        m_socket->write("AUTH LOGIN\r\n");    // 发送 AUTH LOGIN 命令
        break;

    case AuthLoginSent:
        // 状态：AUTH LOGIN 已发送且成功 —— 发送 Base64 编码的用户名（邮箱地址）
        m_state = AuthUserSent;              // 转换到用户名已发送状态
        m_socket->write(m_fromEmail.toUtf8().toBase64() + "\r\n");  // 发送 Base64 编码的邮箱
        break;

    case AuthUserSent:
        // 状态：用户名已发送且成功 —— 发送 Base64 编码的密码（授权码）
        m_state = AuthPassSent;              // 转换到密码已发送状态
        m_socket->write(m_password.toUtf8().toBase64() + "\r\n");   // 发送 Base64 编码的密码
        break;

    case AuthPassSent:
        // 状态：密码验证通过 —— 发送 MAIL FROM 指定发件人
        m_state = MailFromSent;              // 转换到发件人已指定状态
        m_socket->write(QString("MAIL FROM:<%1>\r\n").arg(m_fromEmail).toUtf8());
        break;

    case MailFromSent:
        // 状态：发件人已指定 —— 发送 RCPT TO 指定收件人
        m_state = RcptToSent;                // 转换到收件人已指定状态
        m_socket->write(QString("RCPT TO:<%1>\r\n").arg(m_toEmail).toUtf8());
        break;

    case RcptToSent:
        // 状态：收件人已指定 —— 发送 DATA 命令准备发送邮件内容
        m_state = DataSent;                  // 转换到 DATA 已发送状态
        m_socket->write("DATA\r\n");          // 发送 DATA 命令
        break;

    case DataSent:
    {
        // 状态：DATA 已发送且服务器返回 354 —— 发送邮件正文
        m_state = BodySent;                   // 转换到正文已发送状态
        // 构造邮件主题，使用 Base64 编码的中文主题
        // 格式：=?UTF-8?B?<Base64编码内容>?=
        // 这是邮件协议中传输非 ASCII 字符的标准方式
        QString subject = "=?UTF-8?B?" +
            QString("校园二手交易平台 - 验证码").toUtf8().toBase64() + "?=";
        // 构造完整的邮件正文，包含 SMTP 邮件格式所需的全部头字段
        QString body = QString(
            "From: %1\r\n"                                       // 发件人
            "To: %2\r\n"                                         // 收件人
            "Subject: %3\r\n"                                    // 主题（UTF-8 编码）
            "MIME-Version: 1.0\r\n"                              // MIME 版本
            "Content-Type: text/plain; charset=UTF-8\r\n"        // 内容类型：纯文本，UTF-8
            "Content-Transfer-Encoding: 8bit\r\n"               // 传输编码：8bit
            "\r\n"                                                // 头字段和正文之间的空行分隔
            "您的验证码为：%4\r\n"                                 // 正文：验证码内容
            "有效期5分钟，请勿泄露给他人。\r\n"                     // 正文：安全提示
            "\r\n"                                                // 空行
            "（本邮件由系统自动发送，请勿回复）\r\n"                // 正文：自动发送提示
            ".\r\n"                                               // DATA 结束标记（单独的句点）
        // 依次填入发件人邮箱、收件人邮箱、主题和验证码
        ).arg(m_fromEmail, m_toEmail, subject, m_code);
        // 将邮件正文写入 socket，发送到 SMTP 服务器
        m_socket->write(body.toUtf8());
        break;
    }

    case BodySent:
        // 状态：正文已发送且成功 —— 发送 QUIT 命令结束 SMTP 会话
        m_state = QuitSent;                   // 转换到 QUIT 已发送状态
        m_socket->write("QUIT\r\n");           // 发送 QUIT 命令
        break;

    case QuitSent:
        // 状态：QUIT 已发送 —— SMTP 会话正常结束
        m_timer->stop();                      // 停止超时定时器
        m_socket->disconnectFromHost();        // 断开与服务器的连接
        emit finished(true, "");               // 发送成功信号，空字符串表示无错误
        break;
    }
}

// 处理 Socket 错误事件
// 参数 error：Qt 定义的 socket 错误枚举值
// 当网络连接出现异常时自动调用
void SmtpSender::onError(QAbstractSocket::SocketError error)
{
    // 标记 error 参数为未使用，避免编译器警告
    Q_UNUSED(error);
    // 停止超时定时器
    m_timer->stop();
    // 获取 socket 的错误描述信息
    QString errMsg = "网络连接失败: " + m_socket->errorString();
    // 在调试控制台输出错误信息
    qDebug() << errMsg;
    // 发送失败信号，通知调用方连接失败及原因
    emit finished(false, errMsg);
}

// 处理 SMTP 操作超时事件
// 当在指定时间内未完成 SMTP 会话时自动调用
void SmtpSender::onTimeout()
{
    // 超时后主动断开与 SMTP 服务器的连接
    m_socket->disconnectFromHost();
    // 发送失败信号，通知调用方超时错误
    emit finished(false, "连接超时，请检查网络");
}
