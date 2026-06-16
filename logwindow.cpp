#include "logwindow.h"               // 登录窗口头文件
#include "ui_logwindow.h"            // 从 .ui 文件自动生成的 UI 类
#include "registerdialog.h"          // 注册对话框
#include "forgotpassworddialog.h"    // 找回密码对话框
#include "mainwindow.h"              // 登录成功后的主窗口
#include "dbmanager.h"               // 数据库管理器 Singleton

#include <QMessageBox>               // 消息弹窗
#include <QSqlQuery>                 // SQL 查询
#include <QSqlError>                 // SQL 错误信息
#include <QCryptographicHash>        // SHA-256 哈希
#include <QDebug>                    // 调试输出
#include <QRandomGenerator>          // 随机数生成器

// 构造函数：初始化 UI、连接信号
LoginWindow::LoginWindow(QWidget *parent)
    : QWidget(parent)                // 调用基类构造
    , ui(new Ui::LoginWindow)        // 创建 UI 对象（来自 .ui 文件）
    , m_loginAttempts(0)             // 初始化登录尝试次数为 0
{
    ui->setupUi(this);               // 加载 .ui 布局到当前窗口

    setWindowTitle("校园二手交易平台 - 登录");  // 设置窗口标题
    setWindowIcon(QIcon(":/new/prefix1/image/icon.png"));  // 设置窗口图标

    // 密码框按回车键触发登录
    connect(ui->passwordEdit, &QLineEdit::returnPressed,
            this, &LoginWindow::onLoginClicked);

    // 点击登录按钮触发登录
    connect(ui->loginBtn, &QPushButton::clicked,
            this, &LoginWindow::onLoginClicked);
    // 点击注册按钮打开注册页
    connect(ui->registerBtn, &QPushButton::clicked,
            this, &LoginWindow::onRegisterClicked);
    // 点击"忘记密码"打开找回密码页
    connect(ui->forgotPwdBtn, &QPushButton::clicked,
            this, &LoginWindow::onForgotPasswordClicked);
}

// 析构函数：释放 UI 对象
LoginWindow::~LoginWindow()
{
    delete ui;                       // 清理 UI 资源
}

// SHA-256 密码哈希
// 注意：没有加盐，生产环境建议使用 bcrypt 或 Argon2
QString LoginWindow::hashPassword(const QString &password)
{
    QCryptographicHash hash(QCryptographicHash::Sha256);  // 创建 SHA-256 哈希器
    hash.addData(password.toUtf8());                       // 将密码转为 UTF-8 并计算哈希
    return hash.result().toHex();                          // 返回十六进制字符串
}

// 生成唯一的会话令牌（64位随机字符串）
QString LoginWindow::generateSessionToken()
{
    QString token;
    const QString chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    
    QRandomGenerator *generator = QRandomGenerator::global();
    for (int i = 0; i < 64; ++i) {
        token += chars.at(generator->bounded(chars.length()));
    }
    
    return token;
}

// 登录按钮点击处理
void LoginWindow::onLoginClicked()
{
    // 获取用户输入并去除首尾空格
    QString account = ui->usernameEdit->text().trimmed();  // 学号或邮箱
    QString password = ui->passwordEdit->text();            // 密码（不清除空格，支持空格密码）

    // 空字段校验：依次检查账号和密码
    if (account.isEmpty()) {
        QMessageBox::warning(this, "提示", "请输入学号或邮箱");
        ui->usernameEdit->setFocus();  // 焦点回到输入框
        return;
    }
    if (password.isEmpty()) {
        QMessageBox::warning(this, "提示", "请输入密码");
        ui->passwordEdit->setFocus();
        return;
    }

    // 数据库连接检查
    if (!DbManager::instance().isConnected()) {
        QMessageBox::warning(this, "提示", "数据库未连接");
        return;
    }

    QSqlDatabase db = DbManager::instance().database();  // 获取数据库连接
    QSqlQuery query(db);                                  // 创建查询对象
    // 使用参数化查询防止 SQL 注入——支持用户名或邮箱两种登录方式
    query.prepare("SELECT id, username, password_hash FROM users WHERE username = ? OR email = ?");
    query.addBindValue(account);  // 绑定用户名参数
    query.addBindValue(account);  // 绑定邮箱参数

    // 执行查询
    if (!query.exec()) {
        QMessageBox::warning(this, "错误", "查询失败：" + query.lastError().text());
        return;
    }

    // 如果查询到结果（账号存在）
    if (query.next()) {
        QString storedHash = query.value("password_hash").toString();  // 数据库中存储的哈希
        QString inputHash = hashPassword(password);                     // 用户输入的哈希
        int userId = query.value("id").toInt();                        // 获取用户 ID
        QString username = query.value("username").toString();         // 获取用户名

        // 密码比对
        if (storedHash == inputHash) {
            // 登录成功：重置尝试次数
            m_loginAttempts = 0;

            // 生成新的会话令牌
            QString sessionToken = generateSessionToken();

            // 更新数据库中的会话令牌（新登录会覆盖旧令牌，实现单设备登录）
            QSqlQuery updateQuery(db);
            updateQuery.prepare("UPDATE users SET session_token = ? WHERE id = ?");
            updateQuery.addBindValue(sessionToken);
            updateQuery.addBindValue(userId);
            
            if (updateQuery.exec()) {
                qDebug() << "用户" << username << "登录成功，会话令牌已更新";

                // 创建主窗口并显示（传递会话令牌）
                MainWindow *mainWindow = new MainWindow(userId, username, sessionToken);
                mainWindow->show();

                this->close();  // 关闭登录窗口
            } else {
                qDebug() << "会话令牌更新失败:" << updateQuery.lastError().text();
                QMessageBox::warning(this, "错误", "登录失败：无法更新会话状态\n" + updateQuery.lastError().text());
                return;  // 确保更新失败后不会继续执行
            }
        } else {
            // 密码错误：递增尝试次数
            m_loginAttempts++;
            int remaining = 5 - m_loginAttempts;  // 计算剩余尝试次数

            if (m_loginAttempts >= 5) {
                // 达到 5 次上限：询问是否找回密码
                QMessageBox::StandardButton reply = QMessageBox::question(
                    this, "登录失败",
                    "您已连续 5 次输入错误密码，是否要找回密码？",
                    QMessageBox::Yes | QMessageBox::No
                );
                if (reply == QMessageBox::Yes) {
                    onForgotPasswordClicked();  // 打开找回密码页
                }
                m_loginAttempts = 0;              // 重置计数器
                ui->passwordEdit->clear();         // 清空密码框
                ui->passwordEdit->setFocus();      // 焦点回到密码框
            } else {
                QMessageBox::warning(this, "密码错误",
                    QString("密码错误，还剩 %1 次机会").arg(remaining));
                ui->passwordEdit->clear();
                ui->passwordEdit->setFocus();
            }
        }
    } else {
        // 账号不存在：递增尝试次数（针对同一账号的暴力枚举防护）
        m_loginAttempts++;
        int remaining = 5 - m_loginAttempts;

        if (m_loginAttempts >= 5) {
            // 达到上限询问找回密码
            QMessageBox::StandardButton reply = QMessageBox::question(
                this, "登录失败",
                "您已连续 5 次输入错误，是否要找回密码？",
                QMessageBox::Yes | QMessageBox::No
            );
            if (reply == QMessageBox::Yes) {
                onForgotPasswordClicked();
            }
            m_loginAttempts = 0;
        } else {
            QMessageBox::warning(this, "登录失败",
                QString("账号不存在，还剩 %1 次机会").arg(remaining));
        }
        ui->passwordEdit->clear();     // 清空密码
        ui->usernameEdit->setFocus();  // 焦点回到账号输入框
    }
}

// 点击"注册"按钮：打开注册对话框
void LoginWindow::onRegisterClicked()
{
    RegisterDialog dlg(this);       // 创建注册对话框
    if (dlg.exec() == QDialog::Accepted) {  // 模态运行，等待用户完成
        QMessageBox::information(this, "提示", "注册成功，请登录");
    }
}

// 点击"忘记密码"按钮：打开找回密码对话框
void LoginWindow::onForgotPasswordClicked()
{
    ForgotPasswordDialog dlg(this);  // 创建找回密码对话框
    dlg.exec();                      // 模态运行
}
