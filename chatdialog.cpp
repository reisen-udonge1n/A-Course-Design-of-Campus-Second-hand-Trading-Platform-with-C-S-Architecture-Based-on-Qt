#include "chatdialog.h"
#include "ui_chatdialog.h"
#include "config.h"
#include "chatbubble.h"
#include "chatclient.h"
#include "dbmanager.h"
#include "filter.h"
#include <QMessageBox>

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollBar>
#include <QFrame>
#include <QFont>
#include <QDateTime>
#include <QTimer>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include <memory>

ChatDialog::ChatDialog(int myUserId, const QString &myUsername,
                       int otherUserId, const QString &otherUsername,
                       QWidget *parent,
                       const QString &autoSendMsg)
    : QDialog(parent)
    , m_myUserId(myUserId)
    , m_myUsername(myUsername)
    , m_otherUserId(otherUserId)
    , m_otherUsername(otherUsername)
    , m_autoSendMsg(autoSendMsg)
    , ui(new Ui::ChatDialog)
{
    ui->setupUi(this);

    setWindowTitle(QString("与 %1 聊天").arg(m_otherUsername));
    setFixedSize(400, 600);

    ui->m_titleLabel->setText(m_otherUsername);

    // Set up messages area
    auto *msgContainer = new QWidget();
    msgContainer->setStyleSheet("background: transparent;");
    m_messagesLayout = new QVBoxLayout(msgContainer);
    m_messagesLayout->setContentsMargins(12, 12, 12, 12);
    m_messagesLayout->setSpacing(10);
    m_messagesLayout->addStretch();

    ui->m_scrollArea->setWidget(msgContainer);

    loadMessageHistory();

    connect(ui->m_backBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(ui->m_sendBtn, &QPushButton::clicked, this, &ChatDialog::sendMessage);
    connect(ui->m_inputEdit, &QLineEdit::returnPressed, this, &ChatDialog::sendMessage);
    connect(ui->m_followBtn, &QPushButton::clicked, this, &ChatDialog::onFollowBtnClicked);

    ensureConnected();

    auto &client = ChatClient::instance();
    connect(&client, &ChatClient::messageReceived,
            this, &ChatDialog::onMessageReceived);
    connect(&client, &ChatClient::disconnected,
            this, &ChatDialog::onClientDisconnected);
    connect(&client, &ChatClient::userListChanged,
            this, &ChatDialog::updateOtherUserStatus);
    updateOtherUserStatus();
}

ChatDialog::~ChatDialog()
{
    disconnect(&ChatClient::instance(), &ChatClient::messageReceived,
               this, &ChatDialog::onMessageReceived);
    disconnect(&ChatClient::instance(), &ChatClient::disconnected,
               this, &ChatDialog::onClientDisconnected);
    disconnect(&ChatClient::instance(), &ChatClient::userListChanged,
               this, &ChatDialog::updateOtherUserStatus);
    delete ui;
}

void ChatDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    ui->m_inputEdit->setFocus();
    updateFollowButton();

    if (!m_autoSendMsg.isEmpty()) {
        QTimer::singleShot(300, this, [this]() {
            if (!m_autoSendMsg.isEmpty()) {
                sendSystemMessage(m_autoSendMsg);
                m_autoSendMsg.clear();
            }
        });
    }
}

void ChatDialog::reject()
{
    QDialog::reject();
}

void ChatDialog::sendMessage()
{
    QString content = ui->m_inputEdit->text().trimmed();
    if (content.isEmpty())
        return;

    if (Filter::instance().containsSensitiveWords(content)) {
        ui->m_statusLabel->setText("消息包含敏感词汇，请修改后重试");
        ui->m_statusLabel->setStyleSheet("color: #e74c3c; font-size: 11px; background: transparent;");
        return;
    }

    auto &client = ChatClient::instance();
    if (!client.isConnected()) {
        ui->m_statusLabel->setText("连接已断开，正在重连...");
        ui->m_statusLabel->setStyleSheet("color: #e74c3c; font-size: 11px; background: transparent;");
        ensureConnected();
        return;
    }

    client.sendMessage(m_otherUserId, content);
    qint64 now = QDateTime::currentSecsSinceEpoch();

    if (DbManager::instance().isConnected()) {
        QSqlDatabase db = DbManager::instance().database();
        QSqlQuery q(db);
        q.prepare("INSERT INTO messages (sender_id, receiver_id, content, created_at) "
                  "VALUES (?, ?, ?, NOW())");
        q.addBindValue(m_myUserId);
        q.addBindValue(m_otherUserId);
        q.addBindValue(content);
        if (!q.exec())
            qDebug() << "ChatDialog: failed to save message:" << q.lastError().text();
    }

    addBubble(content, true, now);
    ui->m_inputEdit->clear();
}

void ChatDialog::sendSystemMessage(const QString &msg)
{
    if (msg.isEmpty()) return;

    auto &client = ChatClient::instance();
    if (!client.isConnected()) return;

    client.sendMessage(m_otherUserId, msg);
    qint64 now = QDateTime::currentSecsSinceEpoch();

    if (DbManager::instance().isConnected()) {
        QSqlDatabase db = DbManager::instance().database();
        QSqlQuery q(db);
        q.prepare("INSERT INTO messages (sender_id, receiver_id, content, created_at) "
                  "VALUES (?, ?, ?, NOW())");
        q.addBindValue(m_myUserId);
        q.addBindValue(m_otherUserId);
        q.addBindValue(msg);
        q.exec();
    }

    addBubble(msg, true, now);
}

void ChatDialog::onMessageReceived(int fromUserId, const QString &fromUsername,
                                    const QString &content, qint64 timestamp)
{
    if (fromUserId != m_otherUserId)
        return;

    addBubble(content, false, timestamp);
}

void ChatDialog::onClientDisconnected()
{
    ui->m_statusLabel->setText("连接已断开，正在重连...");
    ui->m_statusLabel->setStyleSheet("color: #e74c3c; font-size: 11px; background: transparent;");
    ensureConnected();
}

void ChatDialog::loadMessageHistory()
{
    if (!DbManager::instance().isConnected()) return;

    QSqlDatabase db = DbManager::instance().database();
    QSqlQuery q(db);
    q.prepare("SELECT sender_id, content, UNIX_TIMESTAMP(created_at) FROM messages "
              "WHERE (sender_id = ? AND receiver_id = ?) "
              "   OR (sender_id = ? AND receiver_id = ?) "
              "ORDER BY created_at ASC");
    q.addBindValue(m_myUserId);
    q.addBindValue(m_otherUserId);
    q.addBindValue(m_otherUserId);
    q.addBindValue(m_myUserId);

    if (!q.exec()) {
        qDebug() << "ChatDialog: failed to load message history:" << q.lastError().text();
        return;
    }

    while (q.next()) {
        int senderId = q.value(0).toInt();
        QString content = q.value(1).toString();
        qint64 timestamp = q.value(2).toLongLong();
        bool isSelf = (senderId == m_myUserId);
        addBubble(content, isSelf, timestamp);
    }

    scrollToBottom();
}

void ChatDialog::addBubble(const QString &text, bool isSelf, qint64 timestamp)
{
    int stretchIdx = m_messagesLayout->count() - 1;
    if (stretchIdx >= 0) {
        QLayoutItem *item = m_messagesLayout->takeAt(stretchIdx);
        if (item->spacerItem())
            delete item;
    }

    QDateTime dt = QDateTime::fromSecsSinceEpoch(timestamp);
    auto *bubble = new ChatBubble(text, isSelf,
        timestamp > 0 ? dt.toString("HH:mm") : QString());

    m_messagesLayout->addWidget(bubble);
    m_messagesLayout->addStretch();

    scrollToBottom();
}

void ChatDialog::scrollToBottom()
{
    QTimer::singleShot(50, this, [this]() {
        ui->m_scrollArea->verticalScrollBar()->setValue(
            ui->m_scrollArea->verticalScrollBar()->maximum()
        );
    });
}

void ChatDialog::ensureConnected()
{
    auto &client = ChatClient::instance();

    if (client.isConnected()) {
        updateOtherUserStatus();
        return;
    }

    if (!client.isLoggedIn()) {
        client.connectToServer(CHAT_HOST, CHAT_PORT);
        client.login(m_myUserId, m_myUsername);
    }

    auto conn = std::make_shared<QMetaObject::Connection>();
    *conn = connect(&client, &ChatClient::loginSuccess, this, [this, conn]() {
        updateOtherUserStatus();
        disconnect(*conn);
    });
}

void ChatDialog::updateOtherUserStatus()
{
    auto &client = ChatClient::instance();
    if (!client.isConnected()) {
        ui->m_statusLabel->setText("连接中...");
        ui->m_statusLabel->setStyleSheet("color: #95a5a6; font-size: 11px; background: transparent;");
        return;
    }

    if (client.isUserOnline(m_otherUserId)) {
        ui->m_statusLabel->setText("在线");
        ui->m_statusLabel->setStyleSheet("color: #27ae60; font-size: 11px; background: transparent;");
    } else {
        ui->m_statusLabel->setText("离线");
        ui->m_statusLabel->setStyleSheet("color: #95a5a6; font-size: 11px; background: transparent;");
    }
}

void ChatDialog::onFollowBtnClicked()
{
    if (!DbManager::instance().isConnected()) return;

    QSqlDatabase db = DbManager::instance().database();

    if (isFollowing()) {
        if (QMessageBox::question(this, "确认取消关注",
                                  QString("确定要取消关注 %1 吗？").arg(m_otherUsername),
                                  QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes) {
            return;
        }

        QSqlQuery q(db);
        q.prepare("DELETE FROM followers WHERE follower_id = ? AND followed_id = ?");
        q.addBindValue(m_myUserId);
        q.addBindValue(m_otherUserId);

        if (q.exec()) {
            QMessageBox::information(this, "成功", QString("已取消关注 %1").arg(m_otherUsername));
            updateFollowButton();
        } else {
            QMessageBox::warning(this, "失败", "取消关注失败：" + q.lastError().text());
        }
    } else {
        QSqlQuery q(db);
        q.prepare("INSERT INTO followers (follower_id, followed_id) VALUES (?, ?)");
        q.addBindValue(m_myUserId);
        q.addBindValue(m_otherUserId);

        if (q.exec()) {
            QMessageBox::information(this, "成功", QString("已关注 %1").arg(m_otherUsername));
            updateFollowButton();
        } else {
            QMessageBox::warning(this, "失败", "关注失败：" + q.lastError().text());
        }
    }
}

bool ChatDialog::isFollowing()
{
    if (!DbManager::instance().isConnected()) return false;

    QSqlDatabase db = DbManager::instance().database();
    QSqlQuery q(db);
    q.prepare("SELECT COUNT(*) FROM followers WHERE follower_id = ? AND followed_id = ?");
    q.addBindValue(m_myUserId);
    q.addBindValue(m_otherUserId);

    if (q.exec() && q.next()) {
        return q.value(0).toInt() > 0;
    }
    return false;
}

void ChatDialog::updateFollowButton()
{
    if (isFollowing()) {
        ui->m_followBtn->setText("已关注");
        ui->m_followBtn->setStyleSheet(
            "QPushButton {"
            "  background-color: #bdc3c7; color: white; border: none;"
            "  border-radius: 15px; font-size: 13px; font-weight: bold;"
            "}"
        );
    } else {
        ui->m_followBtn->setText("+ 关注");
        ui->m_followBtn->setStyleSheet(
            "QPushButton {"
            "  background-color: #3498db; color: white; border: none;"
            "  border-radius: 15px; font-size: 13px; font-weight: bold;"
            "}"
            "QPushButton:hover { background-color: #2980b9; }"
        );
    }
}
