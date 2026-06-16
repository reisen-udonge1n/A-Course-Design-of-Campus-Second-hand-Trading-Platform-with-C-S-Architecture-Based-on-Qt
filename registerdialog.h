#ifndef REGISTERDIALOG_H
#define REGISTERDIALOG_H

#include <QDialog>
#include <QTimer>

namespace Ui { class RegisterDialog; }

class RegisterDialog : public QDialog
{
    Q_OBJECT

public:
    explicit RegisterDialog(QWidget *parent = nullptr);
    ~RegisterDialog();

private slots:
    void onRegisterClicked();
    void onCancelClicked();
    void onSendCode();
    void onCooldownTick();

private:
    QString hashPassword(const QString &password);

    QTimer *m_cooldownTimer;
    int m_cooldownLeft;

    Ui::RegisterDialog *ui;
};

#endif // REGISTERDIALOG_H
