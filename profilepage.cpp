#include "profilepage.h"
#include "ui_profilepage.h"
#include "editprofiledialog.h"
#include "dbmanager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QFrame>
#include <QFont>
#include <QScrollArea>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>

ProfilePage::ProfilePage(int userId, const QString &username, QWidget *parent)
    : QWidget(parent)
    , m_userId(userId)
    , m_username(username)
    , ui(new Ui::ProfilePage)
{
    ui->setupUi(this);

    ensureProfileExists();
    setupUI();
    loadProfile();
}

ProfilePage::~ProfilePage()
{
    delete ui;
}

void ProfilePage::ensureProfileExists()
{
    if (!DbManager::instance().isConnected()) return;

    QSqlDatabase db = DbManager::instance().database();
    QSqlQuery q(db);
    q.prepare("INSERT IGNORE INTO user_profiles (user_id) VALUES (?)");
    q.addBindValue(m_userId);
    q.exec();
}

void ProfilePage::setupUI()
{
    auto *outerLayout = qobject_cast<QVBoxLayout*>(layout());
    outerLayout->setContentsMargins(0, 0, 0, 0);

    auto *scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setStyleSheet(
        "QScrollArea { border: none; background: transparent; }"
        "QScrollBar:vertical { width: 4px; }"
        "QScrollBar::handle:vertical { background: #ccc; border-radius: 2px; }"
    );

    auto *page = new QWidget();
    page->setStyleSheet("background-color: #f5f6fa;");
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(20, 30, 20, 20);
    layout->setSpacing(20);

    auto *avatarContainer = new QWidget();
    avatarContainer->setStyleSheet("background: transparent;");
    auto *avatarLayout = new QVBoxLayout(avatarContainer);
    avatarLayout->setAlignment(Qt::AlignCenter);
    avatarLayout->setSpacing(10);

    m_avatarLabel = new QLabel();
    m_avatarLabel->setFixedSize(90, 90);
    m_avatarLabel->setAlignment(Qt::AlignCenter);
    m_avatarLabel->setStyleSheet(
        "background-color: #3498db;"
        "border-radius: 45px;"
        "font-size: 40px;"
        "color: white;"
    );
    m_avatarLabel->setText(QString(m_username.left(1).toUpper()));
    avatarLayout->addWidget(m_avatarLabel);

    m_usernameLabel = new QLabel(m_username);
    m_usernameLabel->setAlignment(Qt::AlignCenter);
    QFont nameFont;
    nameFont.setPointSize(18);
    nameFont.setBold(true);
    m_usernameLabel->setFont(nameFont);
    m_usernameLabel->setStyleSheet("color: #2c3e50; background: transparent;");
    avatarLayout->addWidget(m_usernameLabel);

    m_accountLabel = new QLabel(QString("账号: %1").arg(m_username));
    m_accountLabel->setAlignment(Qt::AlignCenter);
    m_accountLabel->setStyleSheet("color: #95a5a6; font-size: 13px; background: transparent;");
    avatarLayout->addWidget(m_accountLabel);

    layout->addWidget(avatarContainer);

    auto *statsFrame = new QFrame();
    statsFrame->setStyleSheet("QFrame { background-color: #ffffff; border-radius: 12px; }");
    statsFrame->setFixedHeight(90);
    auto *statsLayout = new QHBoxLayout(statsFrame);
    statsLayout->setContentsMargins(5, 10, 5, 10);
    statsLayout->setSpacing(0);

    struct StatItem { QString label; QLabel **countLabel; int navIdx; };

    QList<StatItem> stats = {
        {"发布", &m_publishedCount, 0},
        {"购买", &m_purchasedCount, 1},
        {"待确", &m_pendingConfirmCount, 4},
        {"待发", &m_pendingShipCount, 2},
        {"待收", &m_pendingReceiptCount, 3},
        {"完成", &m_receivedCount, 5},
    };

    for (int i = 0; i < stats.size(); ++i) {
        if (i > 0) {
            auto *sep = new QFrame();
            sep->setFixedWidth(1);
            sep->setFixedHeight(35);
            sep->setStyleSheet("background-color: #e0e0e0;");
            statsLayout->addWidget(sep);
        }

        auto *item = new QPushButton();
        item->setCursor(Qt::PointingHandCursor);
        item->setFixedHeight(70);
        item->setStyleSheet(
            "QPushButton { background: transparent; border: none; }"
            "QPushButton:hover { background-color: #f8f9fa; border-radius: 8px; }"
        );
        auto *itemLayout = new QVBoxLayout(item);
        itemLayout->setAlignment(Qt::AlignCenter);
        itemLayout->setSpacing(4);

        *stats[i].countLabel = new QLabel("0");
        (*stats[i].countLabel)->setAlignment(Qt::AlignCenter);
        QFont countFont;
        countFont.setPointSize(18);
        countFont.setBold(true);
        (*stats[i].countLabel)->setFont(countFont);
        (*stats[i].countLabel)->setStyleSheet("color: #2c3e50; background: transparent;");
        itemLayout->addWidget(*stats[i].countLabel);

        auto *nameLabel = new QLabel(stats[i].label);
        nameLabel->setAlignment(Qt::AlignCenter);
        nameLabel->setStyleSheet("color: #7f8c8d; font-size: 12px; background: transparent;");
        itemLayout->addWidget(nameLabel);

        statsLayout->addWidget(item, 1);

        int navIdx = stats[i].navIdx;
        connect(item, &QPushButton::clicked, this, [this, navIdx]() {
            switch (navIdx) {
                case 0: emit navigateToPublished(); break;
                case 1: emit navigateToPurchased(); break;
                case 2: emit navigateToPendingShip(); break;
                case 3: emit navigateToPendingReceipt(); break;
                case 4: emit navigateToPendingConfirm(); break;
                case 5: emit navigateToReceived(); break;
            }
        });
    }

    layout->addWidget(statsFrame);

    struct MenuItem { QString icon; QString text; QString type; };

    QList<MenuItem> menus = {
        {"✏️", "编辑个人资料", "edit"},
        {"💬", "我的消息", "chat"},
        {"📦", "我的发布", "published"},
        {"📋", "我的订单", "orders"},
        {"🚪", "退出登录", "logout"},
    };

    QString btnBaseStyle =
        "QPushButton {"
        "  background-color: #ffffff; border-radius: 10px;"
        "  text-align: left; padding: 0 18px;"
        "}"
        "QPushButton:hover { background-color: #f8f9fa; }";

    for (const auto &menu : menus) {
        auto *menuBtn = new QPushButton();
        menuBtn->setFixedHeight(55);
        menuBtn->setCursor(Qt::PointingHandCursor);
        menuBtn->setStyleSheet(btnBaseStyle);

        auto *innerWidget = new QWidget();
        innerWidget->setStyleSheet("background: transparent;");
        auto *innerLayout = new QHBoxLayout(innerWidget);
        innerLayout->setContentsMargins(0, 0, 0, 0);

        auto *iconLabel = new QLabel(menu.icon);
        iconLabel->setStyleSheet("font-size: 20px; background: transparent;");
        innerLayout->addWidget(iconLabel);

        auto *textLabel = new QLabel(menu.text);
        QFont textFont;
        textFont.setPointSize(14);
        textLabel->setFont(textFont);
        textLabel->setStyleSheet("color: #2c3e50; background: transparent;");
        innerLayout->addWidget(textLabel, 1);

        auto *arrowLabel = new QLabel("›");
        arrowLabel->setStyleSheet("font-size: 22px; color: #bdc3c7; background: transparent;");
        innerLayout->addWidget(arrowLabel);

        menuBtn->setLayout(innerLayout);
        layout->addWidget(menuBtn);

        if (menu.type == "edit") {
            connect(menuBtn, &QPushButton::clicked, this, [this]() {
                EditProfileDialog dlg(m_userId, this);
                if (dlg.exec() == QDialog::Accepted) {
                    refreshProfile();
                }
            });
        } else if (menu.type == "chat") {
            connect(menuBtn, &QPushButton::clicked, this, [this]() { emit navigateToChat(); });
        } else if (menu.type == "published") {
            connect(menuBtn, &QPushButton::clicked, this, [this]() { emit navigateToPublished(); });
        } else if (menu.type == "orders") {
            connect(menuBtn, &QPushButton::clicked, this, [this]() { emit navigateToPurchased(); });
        } else if (menu.type == "logout") {
            connect(menuBtn, &QPushButton::clicked, this, [this]() { emit navigateToLogout(); });
        }
    }

    layout->addStretch();
    scrollArea->setWidget(page);
    outerLayout->addWidget(scrollArea);
}

void ProfilePage::loadProfile()
{
    if (!DbManager::instance().isConnected()) return;

    auto formatCount = [](int n) -> QString {
        return n > 99 ? "99+" : QString::number(n);
    };

    QSqlDatabase db = DbManager::instance().database();
    QSqlQuery q(db);

    q.prepare("SELECT COUNT(*) FROM (SELECT 1 FROM products WHERE seller_id = ? GROUP BY title) t");
    q.addBindValue(m_userId);
    if (q.exec() && q.next())
        m_publishedCount->setText(formatCount(q.value(0).toInt()));

    q.prepare("SELECT COUNT(*) FROM (SELECT 1 FROM orders o JOIN products p ON o.product_id = p.id WHERE o.buyer_id = ? GROUP BY p.title) t");
    q.addBindValue(m_userId);
    if (q.exec() && q.next())
        m_purchasedCount->setText(formatCount(q.value(0).toInt()));

    q.prepare("SELECT COUNT(*) FROM (SELECT 1 FROM orders o JOIN products p ON o.product_id = p.id WHERE o.seller_id = ? AND o.status = 0 GROUP BY p.title) t");
    q.addBindValue(m_userId);
    if (q.exec() && q.next())
        m_pendingConfirmCount->setText(formatCount(q.value(0).toInt()));

    q.prepare("SELECT COUNT(*) FROM (SELECT 1 FROM orders o JOIN products p ON o.product_id = p.id WHERE o.seller_id = ? AND o.status = 2 GROUP BY p.title) t");
    q.addBindValue(m_userId);
    if (q.exec() && q.next())
        m_pendingShipCount->setText(formatCount(q.value(0).toInt()));

    q.prepare("SELECT COUNT(*) FROM (SELECT 1 FROM orders o JOIN products p ON o.product_id = p.id WHERE o.buyer_id = ? AND o.status = 5 GROUP BY p.title) t");
    q.addBindValue(m_userId);
    if (q.exec() && q.next())
        m_pendingReceiptCount->setText(formatCount(q.value(0).toInt()));

    q.prepare("SELECT COUNT(*) FROM (SELECT 1 FROM orders o JOIN products p ON o.product_id = p.id WHERE o.buyer_id = ? AND o.status = 3 GROUP BY p.title) t");
    q.addBindValue(m_userId);
    if (q.exec() && q.next())
        m_receivedCount->setText(formatCount(q.value(0).toInt()));
}

void ProfilePage::refreshProfile()
{
    loadProfile();
}
