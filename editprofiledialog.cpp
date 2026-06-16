#include "editprofiledialog.h"
#include "ui_editprofiledialog.h"
#include "dbmanager.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>

EditProfileDialog::EditProfileDialog(int userId, QWidget *parent)
    : QDialog(parent)
    , m_userId(userId)
    , ui(new Ui::EditProfileDialog)
{
    ui->setupUi(this);

    setFixedSize(420, 540);

    connect(ui->m_saveBtn, &QPushButton::clicked, this, &EditProfileDialog::onSave);
    connect(ui->m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

    loadProfile();
}

EditProfileDialog::~EditProfileDialog()
{
    delete ui;
}

bool EditProfileDialog::loadProfile()
{
    if (!DbManager::instance().isConnected()) return false;

    QSqlDatabase db = DbManager::instance().database();
    QSqlQuery q(db);
    q.prepare("SELECT gender, address, shipping_address, receiving_address "
              "FROM user_profiles WHERE user_id = ?");
    q.addBindValue(m_userId);

    if (q.exec() && q.next()) {
        QString gender = q.value(0).toString();
        int genderIdx = ui->m_genderCombo->findText(gender);
        if (genderIdx >= 0) ui->m_genderCombo->setCurrentIndex(genderIdx);

        ui->m_addressEdit->setText(q.value(1).toString());
        ui->m_shippingEdit->setText(q.value(2).toString());
        ui->m_receivingEdit->setText(q.value(3).toString());
    }
    return true;
}

void EditProfileDialog::onSave()
{
    ui->m_errorLabel->setVisible(false);

    if (!DbManager::instance().isConnected()) {
        ui->m_errorLabel->setText("数据库连接失败");
        ui->m_errorLabel->setVisible(true);
        return;
    }

    QSqlDatabase db = DbManager::instance().database();
    QSqlQuery q(db);
    q.prepare("UPDATE user_profiles SET gender = ?, address = ?, "
              "shipping_address = ?, receiving_address = ? WHERE user_id = ?");
    q.addBindValue(ui->m_genderCombo->currentText());
    q.addBindValue(ui->m_addressEdit->text().trimmed());
    q.addBindValue(ui->m_shippingEdit->text().trimmed());
    q.addBindValue(ui->m_receivingEdit->text().trimmed());
    q.addBindValue(m_userId);

    if (q.exec()) {
        accept();
    } else {
        ui->m_errorLabel->setText("保存失败：" + q.lastError().text());
        ui->m_errorLabel->setVisible(true);
    }
}
