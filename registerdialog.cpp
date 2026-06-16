#include "registerdialog.h"
#include "ui_registerdialog.h"
#include "config.h"
#include "smtpsender.h"
#include "verificationcodemanager.h"
#include "dbmanager.h"

#include <QMessageBox>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QCryptographicHash>
#include <QDebug>

RegisterDialog::RegisterDialog(QWidget *parent)
    : QDialog(parent)
    , m_cooldownTimer(new QTimer(this))
    , m_cooldownLeft(0)
    , ui(new Ui::RegisterDialog)
{
    ui->setupUi(this);

    setFixedSize(420, 700);

    connect(ui->m_registerBtn, &QPushButton::clicked, this, &RegisterDialog::onRegisterClicked);
    connect(ui->m_cancelBtn, &QPushButton::clicked, this, &RegisterDialog::onCancelClicked);
    connect(ui->m_confirmPwdEdit, &QLineEdit::returnPressed, this, &RegisterDialog::onRegisterClicked);
    connect(ui->m_sendCodeBtn, &QPushButton::clicked, this, &RegisterDialog::onSendCode);
    connect(m_cooldownTimer, &QTimer::timeout, this, &RegisterDialog::onCooldownTick);
}

RegisterDialog::~RegisterDialog()
{
    delete ui;
}

void RegisterDialog::onSendCode()
{
    ui->m_errorLabel->setVisible(false);
    QString email = ui->m_emailEdit->text().trimmed();

    if (email.isEmpty()) {
        ui->m_errorLabel->setText("请输入电子邮箱");
        ui->m_errorLabel->setVisible(true);
        ui->m_emailEdit->setFocus();
        return;
    }

    if (!email.contains('@') || !email.contains('.')) {
        ui->m_errorLabel->setText("电子邮箱格式不正确");
        ui->m_errorLabel->setVisible(true);
        ui->m_emailEdit->setFocus();
        return;
    }

    if (!DbManager::instance().isConnected()) {
        ui->m_errorLabel->setText("数据库连接失败！");
        ui->m_errorLabel->setVisible(true);
        return;
    }

    QSqlDatabase db = DbManager::instance().database();
    QSqlQuery checkQuery(db);
    checkQuery.prepare("SELECT id FROM users WHERE email = ?");
    checkQuery.addBindValue(email);
    if (checkQuery.exec() && checkQuery.next()) {
        ui->m_errorLabel->setText("该邮箱已被注册");
        ui->m_errorLabel->setVisible(true);
        return;
    }

    ui->m_sendCodeBtn->setEnabled(false);
    ui->m_sendCodeBtn->setText("发送中...");

    QString code = VerificationCodeManager::instance().generateCode(email);

    auto *sender = new SmtpSender(SMTP_USER, SMTP_PASSWORD,
                                  SMTP_HOST, SMTP_PORT, this);
    connect(sender, &SmtpSender::finished, this, [this, sender, email](bool ok, const QString &err) {
        sender->deleteLater();
        if (ok) {
            ui->m_errorLabel->setStyleSheet(
                "color: #27ae60; font-size: 13px; font-weight: bold;"
                "background-color: #e8f8f0; border-radius: 6px; padding: 8px;"
            );
            ui->m_errorLabel->setText("验证码已发送到您的邮箱，请查收");
            ui->m_errorLabel->setVisible(true);

            m_cooldownLeft = 60;
            m_cooldownTimer->start(1000);
            ui->m_sendCodeBtn->setText(QString("重新发送(%1s)").arg(m_cooldownLeft));
        } else {
            VerificationCodeManager::instance().removeCode(email);
            ui->m_errorLabel->setStyleSheet(
                "color: #e74c3c; font-size: 13px; font-weight: bold;"
                "background-color: #fdf0ef; border-radius: 6px; padding: 8px;"
            );
            ui->m_errorLabel->setText("验证码发送失败：" + err);
            ui->m_errorLabel->setVisible(true);
            ui->m_sendCodeBtn->setEnabled(true);
            ui->m_sendCodeBtn->setText("发送验证码");
        }
    });
    sender->sendVerificationCode(email, code);
}

void RegisterDialog::onCooldownTick()
{
    m_cooldownLeft--;
    if (m_cooldownLeft <= 0) {
        m_cooldownTimer->stop();
        ui->m_sendCodeBtn->setEnabled(true);
        ui->m_sendCodeBtn->setText("发送验证码");
    } else {
        ui->m_sendCodeBtn->setText(QString("重新发送(%1s)").arg(m_cooldownLeft));
    }
}

void RegisterDialog::onRegisterClicked()
{
    ui->m_errorLabel->setVisible(false);
    ui->m_errorLabel->setStyleSheet(
        "color: #e74c3c; font-size: 13px; font-weight: bold;"
        "background-color: #fdf0ef; border-radius: 6px; padding: 8px;"
    );

    QString username = ui->m_usernameEdit->text().trimmed();
    QString email = ui->m_emailEdit->text().trimmed();
    QString code = ui->m_codeEdit->text().trimmed();
    QString password = ui->m_passwordEdit->text();
    QString confirmPwd = ui->m_confirmPwdEdit->text();

    if (username.isEmpty()) {
        ui->m_errorLabel->setText("请输入用户名");
        ui->m_errorLabel->setVisible(true);
        ui->m_usernameEdit->setFocus();
        return;
    }
    if (email.isEmpty()) {
        ui->m_errorLabel->setText("请输入电子邮箱");
        ui->m_errorLabel->setVisible(true);
        ui->m_emailEdit->setFocus();
        return;
    }
    if (code.isEmpty()) {
        ui->m_errorLabel->setText("请输入验证码");
        ui->m_errorLabel->setVisible(true);
        ui->m_codeEdit->setFocus();
        return;
    }
    if (password.isEmpty() || confirmPwd.isEmpty()) {
        ui->m_errorLabel->setText("请输入密码");
        ui->m_errorLabel->setVisible(true);
        ui->m_passwordEdit->setFocus();
        return;
    }
    if (password != confirmPwd) {
        ui->m_errorLabel->setText("两次密码不一致");
        ui->m_errorLabel->setVisible(true);
        ui->m_confirmPwdEdit->clear();
        ui->m_confirmPwdEdit->setFocus();
        return;
    }

    if (!VerificationCodeManager::instance().verifyCode(email, code)) {
        ui->m_errorLabel->setText("验证码错误或已过期，请重新获取");
        ui->m_errorLabel->setVisible(true);
        ui->m_codeEdit->clear();
        ui->m_codeEdit->setFocus();
        return;
    }

    if (!DbManager::instance().isConnected()) {
        ui->m_errorLabel->setText("数据库连接失败！");
        ui->m_errorLabel->setVisible(true);
        return;
    }

    QSqlDatabase db = DbManager::instance().database();

    QSqlQuery checkQuery(db);
    checkQuery.prepare("SELECT id FROM users WHERE username = ?");
    checkQuery.addBindValue(username);
    if (!checkQuery.exec()) {
        ui->m_errorLabel->setText("查询失败：" + checkQuery.lastError().text());
        ui->m_errorLabel->setVisible(true);
        return;
    }
    if (checkQuery.next()) {
        ui->m_errorLabel->setText("用户名已存在");
        ui->m_errorLabel->setVisible(true);
        return;
    }

    checkQuery.prepare("SELECT id FROM users WHERE email = ?");
    checkQuery.addBindValue(email);
    if (checkQuery.exec() && checkQuery.next()) {
        ui->m_errorLabel->setText("邮箱已被注册");
        ui->m_errorLabel->setVisible(true);
        return;
    }

    QSqlQuery insertQuery(db);
    insertQuery.prepare("INSERT INTO users (username, email, password_hash) VALUES (?, ?, ?)");
    insertQuery.addBindValue(username);
    insertQuery.addBindValue(email);
    insertQuery.addBindValue(hashPassword(password));

    if (insertQuery.exec()) {
        QMessageBox::information(this, "成功", "注册成功！");
        accept();
    } else {
        ui->m_errorLabel->setText("注册失败：" + insertQuery.lastError().text());
        ui->m_errorLabel->setVisible(true);
    }
}

void RegisterDialog::onCancelClicked()
{
    reject();
}

QString RegisterDialog::hashPassword(const QString &password)
{
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(password.toUtf8());
    return hash.result().toHex();
}
