#include "chatlistpage.h"
#include "ui_chatlistpage.h"
#include "chatbubble.h"
#include "chatclient.h"
#include "dbmanager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QFrame>
#include <QFont>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDateTime>
#include <QDebug>
#include <QScrollBar>

ChatListPage::ChatListPage(int userId, QWidget *parent)
    : QWidget(parent)
    , m_userId(userId)
    , m_otherUserId(-1)
    , ui(new Ui::ChatListPage)
{
    ui->setupUi(this);

    // Set up list page
    auto *scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setStyleSheet(
        "QScrollArea { border: none; background: transparent; }"
        "QScrollBar:vertical { width: 4px; }"
        "QScrollBar::handle:vertical { background: #ccc; border-radius: 2px; }"
    );

    auto *container = new QWidget();
    container->setStyleSheet("background: transparent;");
    m_listLayout = new QVBoxLayout(container);
    m_listLayout->setContentsMargins(12, 12, 12, 12);
    m_listLayout->setSpacing(10);

    scrollArea->setWidget(container);
    auto *listPage = ui->m_listPage;
    auto *listPageLayout = qobject_cast<QVBoxLayout*>(listPage->layout());
    listPageLayout->addWidget(scrollArea);

    // Set up chat page
    auto *chatScrollArea = ui->m_chatScrollArea;
    auto *container2 = new QWidget();
    container2->setStyleSheet("background: transparent;");
    m_messagesLayout = new QVBoxLayout(container2);
    m_messagesLayout->setContentsMargins(12, 12, 12, 12);
    m_messagesLayout->setSpacing(8);
    m_messagesLayout->addStretch();

    chatScrollArea->setWidget(container2);

    connect(ui->m_backBtn, &QPushButton::clicked, this, [this]() {
        if (ui->m_stack->currentIndex() == 1) {
            ui->m_stack->setCurrentIndex(0);
            ui->m_chatTitleLabel->setText("我的消息");
            loadConversations();
        } else {
            emit goBack();
        }
    });

    connect(ui->m_sendBtn, &QPushButton::clicked, this, &ChatListPage::sendMessage);
    connect(ui->m_inputEdit, &QLineEdit::returnPressed, this, &ChatListPage::sendMessage);

    auto &client = ChatClient::instance();
    connect(&client, &ChatClient::messageReceived,
            this, &ChatListPage::onMessageReceived);

    loadConversations();
}

ChatListPage::~ChatListPage()
{
    delete ui;
}

void ChatListPage::loadConversations()
{
    QLayoutItem *item;
    while ((item = m_listLayout->takeAt(0)) != nullptr) {
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }

    if (!DbManager::instance().isConnected()) return;

    QSqlDatabase db = DbManager::instance().database();

    QSqlQuery q(db);
    q.prepare(R"(
        SELECT u.id, u.username,
               m.content, m.created_at, m.sender_id, m.is_read
        FROM users u
        JOIN (
            SELECT other_id, MAX(msg_id) AS last_id
            FROM (
                SELECT CASE WHEN sender_id = ? THEN receiver_id
                            ELSE sender_id END AS other_id,
                       id AS msg_id
                FROM messages
                WHERE sender_id = ? OR receiver_id = ?
            ) t
            GROUP BY other_id
        ) conv ON u.id = conv.other_id
        JOIN messages m ON m.id = conv.last_id
        ORDER BY m.created_at DESC
    )");
    q.addBindValue(m_userId);
    q.addBindValue(m_userId);
    q.addBindValue(m_userId);

    if (!q.exec()) {
        qDebug() << "ChatListPage: loadConversations failed:" << q.lastError().text();
        m_listLayout->addStretch();
        return;
    }

    if (!q.next()) {
        auto *empty = new QLabel("暂无消息\n\n去商品页面找个商品聊聊吧");
        empty->setAlignment(Qt::AlignCenter);
        empty->setStyleSheet("color: #bdc3c7; font-size: 15px; padding: 80px 0; background: transparent;");
        m_listLayout->addWidget(empty);
        m_listLayout->addStretch();
        return;
    }

    do {
        int otherId = q.value(0).toInt();
        QString otherName = q.value(1).toString();
        QString lastContent = q.value(2).toString();
        QDateTime dt = q.value(3).toDateTime();
        QString lastTime = dt.isValid() ? dt.toString("MM-dd HH:mm") : "";
        bool isRead = q.value(5).toBool();

        auto *card = new QPushButton();
        card->setCursor(Qt::PointingHandCursor);
        card->setFixedHeight(72);
        card->setStyleSheet(
            "QPushButton { background-color: #ffffff; border-radius: 10px; border: none; text-align: left; }"
            "QPushButton:hover { background-color: #f8f9fa; }"
        );

        auto *cardLayout = new QHBoxLayout(card);
        cardLayout->setContentsMargins(12, 10, 12, 10);
        cardLayout->setSpacing(12);

        auto *avatar = new QLabel(otherName.left(1).toUpper());
        avatar->setFixedSize(50, 50);
        avatar->setAlignment(Qt::AlignCenter);
        avatar->setStyleSheet(
            "background-color: #3498db; border-radius: 25px;"
            "font-size: 22px; font-weight: bold; color: white;"
        );
        cardLayout->addWidget(avatar);

        auto *infoLayout = new QVBoxLayout();
        infoLayout->setSpacing(4);

        auto *nameRow = new QHBoxLayout();
        auto *nameLbl = new QLabel(otherName);
        QFont nf;
        nf.setPointSize(14);
        nf.setBold(true);
        nameLbl->setFont(nf);
        nameLbl->setStyleSheet("color: #2c3e50; background: transparent;");
        nameRow->addWidget(nameLbl, 1);

        if (!lastTime.isEmpty()) {
            auto *timeLbl = new QLabel(lastTime);
            timeLbl->setStyleSheet("color: #95a5a6; font-size: 11px; background: transparent;");
            nameRow->addWidget(timeLbl);
        }
        infoLayout->addLayout(nameRow);

        QString preview = lastContent.length() > 30 ? lastContent.left(30) + "..." : lastContent;
        auto *previewLbl = new QLabel(preview);
        previewLbl->setStyleSheet(
            QString("color: %1; font-size: 13px; background: transparent;")
                .arg(isRead ? "#7f8c8d" : "#2c3e50")
        );
        infoLayout->addWidget(previewLbl);

        cardLayout->addLayout(infoLayout, 1);

        m_listLayout->addWidget(card);

        connect(card, &QPushButton::clicked, this, [this, otherId, otherName]() {
            openChat(otherId, otherName);
        });
    } while (q.next());

    m_listLayout->addStretch();
}

void ChatListPage::refreshList()
{
    loadConversations();
}

void ChatListPage::openChat(int otherUserId, const QString &otherUsername)
{
    m_otherUserId = otherUserId;
    m_otherUsername = otherUsername;
    ui->m_chatTitleLabel->setText(otherUsername);

    QSqlDatabase db = DbManager::instance().database();
    QSqlQuery q(db);
    q.prepare("UPDATE messages SET is_read = 1 WHERE sender_id = ? AND receiver_id = ?");
    q.addBindValue(otherUserId);
    q.addBindValue(m_userId);
    q.exec();

    loadMessages();
    ui->m_stack->setCurrentIndex(1);
    ui->m_inputEdit->setFocus();
}

void ChatListPage::loadMessages()
{
    QLayoutItem *item;
    while ((item = m_messagesLayout->takeAt(0)) != nullptr) {
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }

    if (!DbManager::instance().isConnected() || m_otherUserId < 0) {
        m_messagesLayout->addStretch();
        return;
    }

    QSqlDatabase db = DbManager::instance().database();
    QSqlQuery q(db);
    q.prepare("SELECT sender_id, content, created_at FROM messages "
              "WHERE (sender_id = ? AND receiver_id = ?) OR (sender_id = ? AND receiver_id = ?) "
              "ORDER BY created_at ASC");
    q.addBindValue(m_userId); q.addBindValue(m_otherUserId);
    q.addBindValue(m_otherUserId); q.addBindValue(m_userId);

    bool hasMsg = false;
    if (q.exec()) {
    while (q.next()) {
        hasMsg = true;
        int senderId = q.value(0).toInt();
        QString content = q.value(1).toString();

        bool isSelf = (senderId == m_userId);

        auto *bubble = new ChatBubble(content, isSelf);
        m_messagesLayout->addWidget(bubble);
    }

    if (!hasMsg) {
        auto *empty = new QLabel("暂无消息，发送第一条消息吧");
        empty->setAlignment(Qt::AlignCenter);
        empty->setStyleSheet("color: #bdc3c7; font-size: 14px; padding: 40px; background: transparent;");
        m_messagesLayout->addWidget(empty);
    }

    m_messagesLayout->addStretch();
    }
}

void ChatListPage::onMessageReceived(int fromUserId, const QString &fromUsername,
                                      const QString &content, qint64 timestamp)
{
    if (ui->m_stack->currentIndex() != 1 || fromUserId != m_otherUserId)
        return;

    int stretchIdx = m_messagesLayout->count() - 1;
    if (stretchIdx >= 0) {
        QLayoutItem *item = m_messagesLayout->takeAt(stretchIdx);
        if (item->spacerItem())
            delete item;
    }

    QDateTime dt = QDateTime::fromSecsSinceEpoch(timestamp);
    auto *bubble = new ChatBubble(content, false,
        timestamp > 0 ? dt.toString("HH:mm") : QString());
    m_messagesLayout->addWidget(bubble);
    m_messagesLayout->addStretch();

    auto *scrollArea = ui->m_chatPage->findChild<QScrollArea*>();
    if (scrollArea) {
        scrollArea->verticalScrollBar()->setValue(
            scrollArea->verticalScrollBar()->maximum()
        );
    }
}

bool ChatListPage::isChattingWith(int userId) const
{
    return ui->m_stack->currentIndex() == 1 && m_otherUserId == userId;
}

void ChatListPage::sendMessage()
{
    QString content = ui->m_inputEdit->text().trimmed();
    if (content.isEmpty() || m_otherUserId < 0) return;

    auto &client = ChatClient::instance();
    client.sendMessage(m_otherUserId, content);

    if (DbManager::instance().isConnected()) {
        QSqlDatabase db = DbManager::instance().database();
        QSqlQuery q(db);
        q.prepare("INSERT INTO messages (sender_id, receiver_id, content, created_at) "
                  "VALUES (?, ?, ?, NOW())");
        q.addBindValue(m_userId);
        q.addBindValue(m_otherUserId);
        q.addBindValue(content);

        if (!q.exec())
            qDebug() << "保存消息失败:" << q.lastError().text();
    }

    ui->m_inputEdit->clear();

    auto *bubble = new ChatBubble(content, true);
    int stretchIdx = m_messagesLayout->count() - 1;
    if (stretchIdx >= 0) {
        QLayoutItem *item = m_messagesLayout->takeAt(stretchIdx);
        if (item->spacerItem())
            delete item;
    }
    m_messagesLayout->addWidget(bubble);
    m_messagesLayout->addStretch();

    auto *scrollArea = ui->m_chatPage->findChild<QScrollArea*>();
    if (scrollArea) {
        scrollArea->verticalScrollBar()->setValue(
            scrollArea->verticalScrollBar()->maximum()
        );
    }
}
