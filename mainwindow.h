#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QWidget>
#include <QPushButton>
#include <QButtonGroup>
#include <QVBoxLayout>
#include <QLabel>
#include <QTimer>
#include <QFrame>

namespace Ui { class MainWindow; }

class ProductsPage;
class PublishPage;
class ChatListPage;

class MainWindow : public QWidget
{
    Q_OBJECT

public:
    explicit MainWindow(int userId, const QString &username, const QString &sessionToken = "", QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onMessageReceived(int fromUserId, const QString &fromUsername,
                          const QString &content, qint64 timestamp);
    void onNotificationClicked();
    void checkSessionValidity();

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void setupUI();
    void setupBottomNav();
    QWidget *createHomePage();
    QWidget *createProductsPage();
    QWidget *createPublishPage();
    QWidget *createProfilePage();
    QWidget *createChatPage();
    void switchPage(int index);
    void showNotification(int fromUserId, const QString &fromUsername, const QString &content);
    bool isSessionValid();
    void logoutDueToConflict();

    int m_userId;
    QString m_username;
    QString m_sessionToken;
    QButtonGroup *m_navGroup;
    QList<QPushButton*> m_navButtons;
    ProductsPage *m_productsPage;
    ChatListPage *m_chatPage;

    // 灵动岛通知栏
    QWidget *m_notificationPill;
    QLabel *m_notificationNameLabel;
    QLabel *m_notificationContentLabel;
    QPushButton *m_notificationBtn;
    QTimer *m_notificationTimer;
    int m_notificationFromUserId;
    QString m_notificationFromUsername;

    // 会话检查定时器
    QTimer *m_sessionCheckTimer;

    // 防止重复退出的标志
    bool m_isLoggingOut;

    Ui::MainWindow *ui;
};

#endif // MAINWINDOW_H
