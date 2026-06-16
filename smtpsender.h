#ifndef SMTPSENDER_H
#define SMTPSENDER_H

// ============================================================
// SmtpSender — SSL SMTP 客户端（有限状态机实现）
//
// SMTP 协议是一个典型的请求-响应式文本协议。本实现使用
// 显式状态机（State pattern via enum + switch）驱动会话流程：
//
//   Init → EHLO → AUTH LOGIN → USER → PASS →
//   MAIL FROM → RCPT TO → DATA → BODY → QUIT
//
// 为什么不用现成的邮件库？
//   避免引入额外的第三方依赖。Qt 本身不提供 SMTP 客户端，
//   这个 FSM 的实现不到 150 行且覆盖了最常用的场景。
//   生产环境可替换为 libsmtp或 curl。
//
// 超时处理：
//   每个步骤有 10s 定时器，防止 SMTP 服务器无响应导致
//   连接挂死。超时时直接断开并通知调用方。
// ============================================================

#include <QObject>
#include <QSslSocket>
#include <QTimer>

class SmtpSender : public QObject
{
    Q_OBJECT
public:
    explicit SmtpSender(const QString &fromEmail, const QString &password,
                        const QString &smtpHost = "smtp.126.com", int smtpPort = 465,
                        QObject *parent = nullptr);

    void sendVerificationCode(const QString &toEmail, const QString &code);

signals:
    void finished(bool success, const QString &errorMsg);

private slots:
    void onConnected();
    void onReadyRead();
    void onError(QAbstractSocket::SocketError error);
    void onTimeout();

private:
    void sendNext();
    void processResponse(const QString &line);

    enum State {
        Init, EhloSent, AuthLoginSent, AuthUserSent, AuthPassSent,
        MailFromSent, RcptToSent, DataSent, BodySent, QuitSent
    };

    QSslSocket *m_socket;
    QTimer *m_timer;
    State m_state;
    QString m_buffer;

    QString m_smtpHost;
    int m_smtpPort;
    QString m_fromEmail;
    QString m_password;
    QString m_toEmail;
    QString m_code;
};

#endif // SMTPSENDER_H
