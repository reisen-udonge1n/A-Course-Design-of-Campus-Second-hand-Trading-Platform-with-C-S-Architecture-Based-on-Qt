#ifndef CHATLISTPAGE_H
#define CHATLISTPAGE_H

#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QDateTime>

namespace Ui { class ChatListPage; }

class ChatListPage : public QWidget
{
    Q_OBJECT

public:
    explicit ChatListPage(int userId, QWidget *parent = nullptr);
    ~ChatListPage();

signals:
    void goBack();

public slots:
    void refreshList();
    void openChat(int otherUserId, const QString &otherUsername);
    bool isChattingWith(int userId) const;
    void sendMessage();
    void onMessageReceived(int fromUserId, const QString &fromUsername,
                           const QString &content, qint64 timestamp);

private:
    void loadConversations();
    void loadMessages();

    int m_userId;

    QVBoxLayout *m_listLayout;
    QVBoxLayout *m_messagesLayout;
    int m_otherUserId;
    QString m_otherUsername;

    Ui::ChatListPage *ui;
};

#endif // CHATLISTPAGE_H
