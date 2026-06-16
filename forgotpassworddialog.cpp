#include "forgotpassworddialog.h"
#include "ui_forgotpassworddialog.h"
#include "config.h"
#include "registerdialog.h"
#include "smtpsender.h"
#include "verificationcodemanager.h"
#include "dbmanager.h"

#include <QMessageBox>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QCryptographicHash>
#include <QDebug>

ForgotPasswordDialog::ForgotPasswordDialog(QWidget *parent)
    : QDialog(parent)
    , m_verifiedUserId(-1)
    , m_cooldownTimer(new QTimer(this))
    , m_cooldownLeft(0)
    , ui(new Ui::ForgotPasswordDialog)
{
    ui->setupUi(this);

    setFixedSize(420, 500);

    connect(ui->m_sendCodeBtn, &QPushButton::clicked, this, &ForgotPasswordDialog::onSendCode);
    connect(ui->m_verifyCodeBtn, &QPushButton::clicked, this, &ForgotPasswordDialog::onVerifyCode);
    connect(ui->m_codeEdit, &QLineEdit::returnPressed, this, &ForgotPasswordDialog::onVerifyCode);
    connect(ui->m_resetBtn, &QPushButton::clicked, this, &ForgotPasswordDialog::onResetPassword);
    connect(ui->m_confirmPwdEdit, &QLineEdit::returnPressed, this, &ForgotPasswordDialog::onResetPassword);
    connect(ui->m_cancelBtn, &QPushButton::clicked, this, &ForgotPasswordDialog::onCancel);
    connect(m_cooldownTimer, &QTimer::timeout, this, &ForgotPasswordDialog::onCooldownTick);
}

ForgotPasswordDialog::~ForgotPasswordDialog()
{
    delete ui;
}

QString ForgotPasswordDialog::hashPassword(const QString &password)
{
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(password.toUtf8());
    return hash.result().toHex();
}

void ForgotPasswordDialog::onSendCode()
{
    ui->m_step1Error->setVisible(false);
    QString email = ui->m_emailEdit->text().trimmed();

    if (email.isEmpty()) {
        ui->m_step1Error->setText("请输入电子邮箱");
        ui->m_step1Error->setVisible(true);
        ui->m_emailEdit->setFocus();
        return;
    }

    if (!DbManager::instance().isConnected()) {
        ui->m_step1Error->setText("数据库连接失败");
        ui->m_step1Error->setVisible(true);
        return;
    }

    QSqlDatabase db = DbManager::instance().database();
    QSqlQuery query(db);
    query.prepare("SELECT id, username FROM users WHERE email = ?");
    query.addBindValue(email);

    if (!query.exec()) {
        ui->m_step1Error->setText("查询失败：" + query.lastError().text());
        ui->m_step1Error->setVisible(true);
        return;
    }

    if (!query.next()) {
        QMessageBox::StandardButton reply = QMessageBox::question(
            this, "邮箱未注册",
            "该邮箱尚未注册，是否前往注册？",
            QMessageBox::Yes | QMessageBox::No
        );
        if (reply == QMessageBox::Yes) {
            RegisterDialog dlg(this);
            if (dlg.exec() == QDialog::Accepted) {
                QMessageBox::information(this, "提示", "注册成功，请返回登录");
                accept();
            }
        }
        return;
    }

    m_verifiedUserId = query.value("id").toInt();

    ui->m_sendCodeBtn->setEnabled(false);
    ui->m_sendCodeBtn->setText("发送中...");

    QString code = VerificationCodeManager::instance().generateCode(email);

    auto *sender = new SmtpSender(SMTP_USER, SMTP_PASSWORD,
                                  SMTP_HOST, SMTP_PORT, this);
    connect(sender, &SmtpSender::finished, this, [this, sender](bool ok, const QString &err) {
        sender->deleteLater();
        if (ok) {
            ui->m_step1Error->setStyleSheet(
                "color: #27ae60; font-size: 13px; font-weight: bold;"
                "background-color: #e8f8f0; border-radius: 6px; padding: 8px;"
            );
            ui->m_step1Error->setText("验证码已发送到您的邮箱，请查收");
            ui->m_step1Error->setVisible(true);

            m_cooldownLeft = 60;
            m_cooldownTimer->start(1000);
            ui->m_sendCodeBtn->setText(QString("重新发送(%1s)").arg(m_cooldownLeft));
        } else {
            ui->m_step1Error->setStyleSheet(
                "color: #e74c3c; font-size: 13px; font-weight: bold;"
                "background-color: #fdf0ef; border-radius: 6px; padding: 8px;"
            );
            ui->m_step1Error->setText("验证码发送失败：" + err);
            ui->m_step1Error->setVisible(true);
            ui->m_sendCodeBtn->setEnabled(true);
            ui->m_sendCodeBtn->setText("发送验证码");
        }
    });
    sender->sendVerificationCode(email, code);
}

void ForgotPasswordDialog::onVerifyCode()
{
    ui->m_step1Error->setVisible(false);
    QString email = ui->m_emailEdit->text().trimmed();
    QString code = ui->m_codeEdit->text().trimmed();

    if (code.isEmpty()) {
        ui->m_step1Error->setText("请输入验证码");
        ui->m_step1Error->setVisible(true);
        ui->m_codeEdit->setFocus();
        return;
    }

    if (VerificationCodeManager::instance().verifyCode(email, code)) {
        ui->m_stack->setCurrentIndex(1);
        ui->m_newPwdEdit->setFocus();
    } else {
        ui->m_step1Error->setStyleSheet(
            "color: #e74c3c; font-size: 13px; font-weight: bold;"
            "background-color: #fdf0ef; border-radius: 6px; padding: 8px;"
        );
        ui->m_step1Error->setText("验证码错误或已过期，请重新获取");
        ui->m_step1Error->setVisible(true);
        ui->m_codeEdit->clear();
        ui->m_codeEdit->setFocus();
    }
}

void ForgotPasswordDialog::onResetPassword()
{
    ui->m_step2Error->setVisible(false);

    QString newPwd = ui->m_newPwdEdit->text();
    QString confirmPwd = ui->m_confirmPwdEdit->text();

    if (newPwd.isEmpty()) {
        ui->m_step2Error->setText("请输入新密码");
        ui->m_step2Error->setVisible(true);
        ui->m_newPwdEdit->setFocus();
        return;
    }
    if (newPwd.length() < 6) {
        ui->m_step2Error->setText("密码至少6位");
        ui->m_step2Error->setVisible(true);
        ui->m_newPwdEdit->setFocus();
        return;
    }
    if (confirmPwd.isEmpty()) {
        ui->m_step2Error->setText("请再次输入新密码");
        ui->m_step2Error->setVisible(true);
        ui->m_confirmPwdEdit->setFocus();
        return;
    }
    if (newPwd != confirmPwd) {
        ui->m_step2Error->setText("两次输入的密码不一致");
        ui->m_step2Error->setVisible(true);
        ui->m_confirmPwdEdit->clear();
        ui->m_confirmPwdEdit->setFocus();
        return;
    }

    if (!DbManager::instance().isConnected()) {
        ui->m_step2Error->setText("数据库连接失败");
        ui->m_step2Error->setVisible(true);
        return;
    }

    QSqlDatabase db = DbManager::instance().database();
    QSqlQuery query(db);
    query.prepare("UPDATE users SET password_hash = ? WHERE id = ?");
    query.addBindValue(hashPassword(newPwd));
    query.addBindValue(m_verifiedUserId);

    if (query.exec()) {
        QMessageBox::information(this, "成功", "密码重置成功，请使用新密码登录");
        accept();
    } else {
        ui->m_step2Error->setText("重置失败：" + query.lastError().text());
        ui->m_step2Error->setVisible(true);
    }
}

void ForgotPasswordDialog::onCancel()
{
    reject();
}

void ForgotPasswordDialog::onCooldownTick()
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
