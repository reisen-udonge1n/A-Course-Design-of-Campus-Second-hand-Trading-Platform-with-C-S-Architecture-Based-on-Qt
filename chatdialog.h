#ifndef CHATDIALOG_H
#define CHATDIALOG_H

#include <QDialog>
#include <QLabel>
#include <QVBoxLayout>
#include <QScrollArea>

namespace Ui { class ChatDialog; }

class ChatDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ChatDialog(int myUserId, const QString &myUsername,
                        int otherUserId, const QString &otherUsername,
                        QWidget *parent = nullptr,
                        const QString &autoSendMsg = QString());
    ~ChatDialog();

protected:
    void showEvent(QShowEvent *event) override;
    void reject() override;

private slots:
    void sendMessage();
    void onMessageReceived(int fromUserId, const QString &fromUsername,
                           const QString &content, qint64 timestamp);
    void onClientDisconnected();
    void onFollowBtnClicked();

private:
    void addBubble(const QString &text, bool isSelf, qint64 timestamp);
    void scrollToBottom();
    void ensureConnected();
    void updateOtherUserStatus();
    void sendSystemMessage(const QString &msg);
    void loadMessageHistory();
    bool isFollowing();
    void updateFollowButton();

    int m_myUserId;
    QString m_myUsername;
    int m_otherUserId;
    QString m_otherUsername;
    QString m_autoSendMsg;

    QVBoxLayout *m_messagesLayout;

    Ui::ChatDialog *ui;
};

#endif // CHATDIALOG_H
