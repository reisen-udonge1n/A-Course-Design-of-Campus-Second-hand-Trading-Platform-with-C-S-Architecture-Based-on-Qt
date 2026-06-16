#ifndef FORGOTPASSWORDDIALOG_H
#define FORGOTPASSWORDDIALOG_H

#include <QDialog>
#include <QTimer>

namespace Ui { class ForgotPasswordDialog; }

class ForgotPasswordDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ForgotPasswordDialog(QWidget *parent = nullptr);
    ~ForgotPasswordDialog();

private slots:
    void onSendCode();
    void onVerifyCode();
    void onResetPassword();
    void onCancel();
    void onCooldownTick();

private:
    QString hashPassword(const QString &password);

    int m_verifiedUserId;
    QTimer *m_cooldownTimer;
    int m_cooldownLeft;
    Ui::ForgotPasswordDialog *ui;
};

#endif // FORGOTPASSWORDDIALOG_H
