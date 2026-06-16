#ifndef LOGINWINDOW_H
#define LOGINWINDOW_H

// ============================================================
// LoginWindow — 登录入口
//
// 唯一使用了 Qt Designer .ui 文件的页面（logwindow.ui）。
// 使用 Ui::LoginWindow 命名空间中的自动生成代码。
//
// 安全特性：
//   1. SHA-256 密码哈希（非加盐——改进空间）
//   2. 5 次失败尝试锁定（进程内存计数器，重置密码后清零）
//   3. 支持用户名或邮箱两种登录方式
//
// 流程：LoginWindow → MainWindow（成功时，emit loginSuccess）
//       LoginWindow → RegisterDialog / ForgotPasswordDialog
// ============================================================

#include <QWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QSqlDatabase>

QT_BEGIN_NAMESPACE
namespace Ui { class LoginWindow; }
QT_END_NAMESPACE

class LoginWindow : public QWidget
{
    Q_OBJECT

public:
    explicit LoginWindow(QWidget *parent = nullptr);
    ~LoginWindow();

private slots:
    void onLoginClicked();
    void onRegisterClicked();
    void onForgotPasswordClicked();

private:
    QString hashPassword(const QString &password);
    QString generateSessionToken();

    Ui::LoginWindow *ui;
    int m_loginAttempts;
};

#endif // LOGINWINDOW_H
