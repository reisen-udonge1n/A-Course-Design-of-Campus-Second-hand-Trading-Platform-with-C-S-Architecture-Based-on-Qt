#include "followersdialog.h"
#include "ui_followersdialog.h"
#include "dbmanager.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QMessageBox>
#include <QDebug>

FollowersDialog::FollowersDialog(int userId, const QString &username, QWidget *parent)
    : QDialog(parent)
    , m_userId(userId)
    , m_username(username)
    , m_currentTab(Following)
    , ui(new Ui::FollowersDialog)
{
    ui->setupUi(this);

    setFixedSize(360, 500);

    connect(ui->m_followingTab, &QPushButton::clicked, this, [this]() {
        ui->m_followingTab->setStyleSheet(
            "QPushButton {"
            "  background-color: #3498db; color: white; border: none;"
            "  border-radius: 8px; font-size: 14px; font-weight: bold; padding: 6px 16px;"
            "}"
        );
        ui->m_discoverTab->setStyleSheet(
            "QPushButton {"
            "  background-color: #f0f0f0; color: #555; border: none;"
            "  border-radius: 8px; font-size: 14px; font-weight: bold; padding: 6px 16px;"
            "}"
            "QPushButton:hover { background-color: #e0e0e0; }"
        );
        m_currentTab = Following;
        ui->m_actionBtn->setText("取消关注");
        ui->m_actionBtn->setStyleSheet(
            "QPushButton {"
            "  background-color: #e74c3c; color: white; border: none;"
            "  border-radius: 10px; font-size: 16px; font-weight: bold;"
            "}"
            "QPushButton:hover { background-color: #c0392b; }"
            "QPushButton:disabled { background-color: #bdc3c7; }"
        );
        disconnect(ui->m_actionBtn, nullptr, nullptr, nullptr);
        connect(ui->m_actionBtn, &QPushButton::clicked, this, &FollowersDialog::onUnfollowUser);
        refreshList();
    });

    connect(ui->m_discoverTab, &QPushButton::clicked, this, [this]() {
        ui->m_discoverTab->setStyleSheet(
            "QPushButton {"
            "  background-color: #3498db; color: white; border: none;"
            "  border-radius: 8px; font-size: 14px; font-weight: bold; padding: 6px 16px;"
            "}"
        );
        ui->m_followingTab->setStyleSheet(
            "QPushButton {"
            "  background-color: #f0f0f0; color: #555; border: none;"
            "  border-radius: 8px; font-size: 14px; font-weight: bold; padding: 6px 16px;"
            "}"
            "QPushButton:hover { background-color: #e0e0e0; }"
        );
        m_currentTab = Discover;
        ui->m_actionBtn->setText("关注");
        ui->m_actionBtn->setStyleSheet(
            "QPushButton {"
            "  background-color: #3498db; color: white; border: none;"
            "  border-radius: 10px; font-size: 16px; font-weight: bold;"
            "}"
            "QPushButton:hover { background-color: #2980b9; }"
            "QPushButton:disabled { background-color: #bdc3c7; }"
        );
        disconnect(ui->m_actionBtn, nullptr, nullptr, nullptr);
        connect(ui->m_actionBtn, &QPushButton::clicked, this, &FollowersDialog::onFollowUser);
        refreshList();
    });

    connect(ui->m_searchEdit, &QLineEdit::textChanged, this, &FollowersDialog::refreshList);
    connect(ui->m_listWidget, &QListWidget::itemDoubleClicked, this, &FollowersDialog::onViewUserProducts);
    connect(ui->m_viewProductsBtn, &QPushButton::clicked, this, &FollowersDialog::onViewUserProducts);
    connect(ui->m_listWidget, &QListWidget::itemSelectionChanged, this, [this]() {
        bool hasSelection = !ui->m_listWidget->selectedItems().isEmpty();
        if (hasSelection) {
            QListWidgetItem *item = ui->m_listWidget->selectedItems().first();
            if (m_currentTab == Discover) {
                QString text = item->text();
                if (text.startsWith("✅")) {
                    ui->m_actionBtn->setEnabled(false);
                    ui->m_viewProductsBtn->setEnabled(true);
                    return;
                }
            }
        }
        ui->m_actionBtn->setEnabled(hasSelection);
        ui->m_viewProductsBtn->setEnabled(hasSelection);
    });

    refreshList();
}

FollowersDialog::~FollowersDialog()
{
    delete ui;
}

void FollowersDialog::refreshList()
{
    ui->m_listWidget->clear();
    if (m_currentTab == Following) {
        loadFollowers();
    } else {
        loadUsers();
    }
}

void FollowersDialog::loadFollowers()
{
    if (!DbManager::instance().isConnected()) return;

    QString searchText = ui->m_searchEdit->text().trimmed();

    QSqlDatabase db = DbManager::instance().database();
    QSqlQuery q(db);

    if (searchText.isEmpty()) {
        q.prepare("SELECT u.id, u.username FROM followers f "
                  "JOIN users u ON f.followed_id = u.id "
                  "WHERE f.follower_id = ? ORDER BY f.created_at DESC");
        q.addBindValue(m_userId);
    } else {
        q.prepare("SELECT u.id, u.username FROM followers f "
                  "JOIN users u ON f.followed_id = u.id "
                  "WHERE f.follower_id = ? AND u.username LIKE ? ORDER BY f.created_at DESC");
        q.addBindValue(m_userId);
        q.addBindValue(QString("%%1%").arg(searchText));
    }

    if (!q.exec()) {
        qDebug() << "查询关注列表失败:" << q.lastError().text();
        return;
    }

    while (q.next()) {
        int userId = q.value(0).toInt();
        QString username = q.value(1).toString();
        auto *item = new QListWidgetItem(QString("👤 %1").arg(username));
        item->setData(Qt::UserRole, userId);
        ui->m_listWidget->addItem(item);
    }

    if (ui->m_listWidget->count() == 0) {
        ui->m_listWidget->addItem(searchText.isEmpty() ? "暂无关注用户" : "未找到匹配的关注用户");
    }
}

void FollowersDialog::loadUsers()
{
    if (!DbManager::instance().isConnected()) return;

    QString searchText = ui->m_searchEdit->text().trimmed();

    QSqlDatabase db = DbManager::instance().database();
    QSqlQuery q(db);

    if (searchText.isEmpty()) {
        q.prepare("SELECT id, username FROM users WHERE id != ? ORDER BY username");
        q.addBindValue(m_userId);
    } else {
        q.prepare("SELECT id, username FROM users WHERE id != ? AND username LIKE ? ORDER BY username");
        q.addBindValue(m_userId);
        q.addBindValue(QString("%%1%").arg(searchText));
    }

    if (!q.exec()) {
        qDebug() << "查询用户列表失败:" << q.lastError().text();
        return;
    }

    while (q.next()) {
        int userId = q.value(0).toInt();
        QString username = q.value(1).toString();

        QSqlQuery fq(db);
        fq.prepare("SELECT COUNT(*) FROM followers WHERE follower_id = ? AND followed_id = ?");
        fq.addBindValue(m_userId);
        fq.addBindValue(userId);
        if (fq.exec() && fq.next() && fq.value(0).toInt() > 0) {
            auto *item = new QListWidgetItem(QString("✅ %1 (已关注)").arg(username));
            item->setData(Qt::UserRole, userId);
            item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
            item->setForeground(Qt::gray);
            ui->m_listWidget->addItem(item);
        } else {
            auto *item = new QListWidgetItem(QString("👤 %1").arg(username));
            item->setData(Qt::UserRole, userId);
            ui->m_listWidget->addItem(item);
        }
    }

    if (ui->m_listWidget->count() == 0) {
        ui->m_listWidget->addItem(searchText.isEmpty() ? "暂无用户" : "未找到匹配的用户");
    }
}

void FollowersDialog::onFollowUser()
{
    auto items = ui->m_listWidget->selectedItems();
    if (items.isEmpty()) return;

    QListWidgetItem *item = items.first();
    int targetId = item->data(Qt::UserRole).toInt();
    QString targetName = item->text().mid(2);

    if (!DbManager::instance().isConnected()) return;

    QSqlDatabase db = DbManager::instance().database();
    QSqlQuery q(db);
    q.prepare("INSERT INTO followers (follower_id, followed_id) VALUES (?, ?)");
    q.addBindValue(m_userId);
    q.addBindValue(targetId);

    if (q.exec()) {
        QMessageBox::information(this, "成功", QString("已关注 %1").arg(targetName));
        item->setText(QString("✅ %1 (已关注)").arg(targetName));
        item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
        item->setForeground(Qt::gray);
        ui->m_actionBtn->setEnabled(false);
        emit followChanged();
    } else {
        QMessageBox::warning(this, "失败", "关注失败：" + q.lastError().text());
    }
}

void FollowersDialog::onUnfollowUser()
{
    auto items = ui->m_listWidget->selectedItems();
    if (items.isEmpty()) return;

    QListWidgetItem *item = items.first();
    int targetId = item->data(Qt::UserRole).toInt();
    QString targetName = item->text().mid(2);

    if (QMessageBox::question(this, "确认取消关注", QString("确定要取消关注 %1 吗？").arg(targetName),
                              QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes) {
        return;
    }

    if (!DbManager::instance().isConnected()) return;

    QSqlDatabase db = DbManager::instance().database();
    QSqlQuery q(db);
    q.prepare("DELETE FROM followers WHERE follower_id = ? AND followed_id = ?");
    q.addBindValue(m_userId);
    q.addBindValue(targetId);

    if (q.exec()) {
        QMessageBox::information(this, "成功", QString("已取消关注 %1").arg(targetName));
        item->setText(QString("👤 %1").arg(targetName));
        item->setFlags(item->flags() | Qt::ItemIsSelectable);
        item->setForeground(Qt::black);
        emit followChanged();
    } else {
        QMessageBox::warning(this, "失败", "取消关注失败：" + q.lastError().text());
    }
}

void FollowersDialog::onViewUserProducts()
{
    auto items = ui->m_listWidget->selectedItems();
    if (items.isEmpty()) return;

    int targetId = items.first()->data(Qt::UserRole).toInt();
    QString targetName = items.first()->text().mid(2);

    if (targetName.endsWith(" (已关注)")) {
        targetName = targetName.left(targetName.length() - 5);
    }

    emit viewUserProducts(targetId, targetName);
}
