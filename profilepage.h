#ifndef PROFILEPAGE_H
#define PROFILEPAGE_H

#include <QWidget>
#include <QLabel>

namespace Ui { class ProfilePage; }

class ProfilePage : public QWidget
{
    Q_OBJECT

public:
    explicit ProfilePage(int userId, const QString &username, QWidget *parent = nullptr);
    ~ProfilePage();

signals:
    void navigateToPublished();
    void navigateToPurchased();
    void navigateToPendingShip();
    void navigateToPendingReceipt();
    void navigateToPendingConfirm();
    void navigateToReceived();
    void navigateToChat();
    void navigateToLogout();

public slots:
    void refreshProfile();

private:
    void setupUI();
    void loadProfile();
    void ensureProfileExists();

    int m_userId;
    QString m_username;
    QLabel *m_avatarLabel;
    QLabel *m_usernameLabel;
    QLabel *m_accountLabel;
    QLabel *m_publishedCount;
    QLabel *m_purchasedCount;
    QLabel *m_pendingShipCount;
    QLabel *m_pendingReceiptCount;
    QLabel *m_pendingConfirmCount;
    QLabel *m_receivedCount;

    Ui::ProfilePage *ui;
};

#endif // PROFILEPAGE_H
