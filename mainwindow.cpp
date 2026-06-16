#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "config.h"
#include "homepage.h"
#include "profilepage.h"
#include "orderlistpage.h"
#include "productspage.h"
#include "publishpage.h"
#include "chatlistpage.h"
#include "searchpage.h"
#include "chatclient.h"
#include "productdetaildialog.h"
#include "chatdialog.h"

#include <QHBoxLayout>
#include <QScrollArea>
#include <QFont>
#include <QDebug>
#include <QMessageBox>
#include <QCloseEvent>
#include <QSqlQuery>
#include <QSqlDatabase>
#include "dbmanager.h"
#include "logwindow.h"

MainWindow::MainWindow(int userId, const QString &username, const QString &sessionToken, QWidget *parent)
    : QWidget(parent)
    , m_userId(userId)
    , m_username(username)
    , m_sessionToken(sessionToken)
    , m_navGroup(new QButtonGroup(this))
    , m_productsPage(nullptr)
    , m_notificationPill(nullptr)
    , m_notificationBtn(nullptr)
    , m_notificationTimer(nullptr)
    , m_notificationFromUserId(-1)
    , m_sessionCheckTimer(new QTimer(this))
    , m_isLoggingOut(false)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setupUI();

    auto &client = ChatClient::instance();
    client.connectToServer(CHAT_HOST, CHAT_PORT);
    client.login(m_userId, m_username);

    connect(&client, &ChatClient::messageReceived, this, &MainWindow::onMessageReceived);

    // 断连时立即检查 session 是否因其他设备登录而被踢
    connect(&client, &ChatClient::disconnected, this, [this]() {
        if (!m_sessionToken.isEmpty()) {
            QTimer::singleShot(200, this, [this]() {
                if (!isSessionValid()) {
                    logoutDueToConflict();
                }
            });
        }
    });

    connect(m_sessionCheckTimer, &QTimer::timeout, this, &MainWindow::checkSessionValidity);
    m_sessionCheckTimer->start(30000);
}

MainWindow::~MainWindow() {
    if (m_sessionCheckTimer) {
        m_sessionCheckTimer->stop();
    }
    delete ui;
}

void MainWindow::setupUI()
{
    setWindowTitle("校园二手交易平台");
    setWindowIcon(QIcon(":/new/prefix1/image/icon.png"));  // 设置窗口图标
    resize(420, 750);
    setMinimumSize(400, 600);

    // ======== 灵动岛通知栏（默认隐藏） ========
    auto *barLayout = new QHBoxLayout(ui->m_notificationBar);
    barLayout->setContentsMargins(30, 6, 30, 6);
    barLayout->setSpacing(0);

    m_notificationBtn = new QPushButton();
    m_notificationBtn->setFixedHeight(44);
    m_notificationBtn->setCursor(Qt::PointingHandCursor);
    m_notificationBtn->setStyleSheet(
        "QPushButton {"
        "  background-color: rgba(30, 30, 30, 230);"
        "  border-radius: 22px;"
        "  border: none;"
        "}"
        "QPushButton:hover {"
        "  background-color: rgba(50, 50, 50, 230);"
        "}"
    );

    auto *pillLayout = new QHBoxLayout(m_notificationBtn);
    pillLayout->setContentsMargins(16, 8, 16, 8);
    pillLayout->setSpacing(10);

    auto *iconLabel = new QLabel("💬");
    iconLabel->setStyleSheet("font-size: 18px; background: transparent;");
    pillLayout->addWidget(iconLabel);

    auto *textLayout = new QVBoxLayout();
    textLayout->setSpacing(1);
    m_notificationNameLabel = new QLabel();
    m_notificationNameLabel->setStyleSheet(
        "color: #ffffff; font-size: 13px; font-weight: bold; background: transparent;"
    );
    textLayout->addWidget(m_notificationNameLabel);

    m_notificationContentLabel = new QLabel();
    m_notificationContentLabel->setStyleSheet(
        "color: rgba(255,255,255,180); font-size: 11px; background: transparent;"
    );
    m_notificationContentLabel->setWordWrap(false);
    textLayout->addWidget(m_notificationContentLabel);

    pillLayout->addLayout(textLayout, 1);
    barLayout->addWidget(m_notificationBtn);

    m_notificationTimer = new QTimer(this);
    m_notificationTimer->setSingleShot(true);
    m_notificationTimer->setInterval(5000);
    connect(m_notificationTimer, &QTimer::timeout, ui->m_notificationBar, &QWidget::hide);

    connect(m_notificationBtn, &QPushButton::clicked, this, &MainWindow::onNotificationClicked);

    ui->m_notificationBar->hide();

    // ======== 页面区域 ========
    ui->m_stackedWidget->addWidget(createHomePage());                  // 0: 首页
    ui->m_stackedWidget->addWidget(createProductsPage());              // 1: 商品
    ui->m_stackedWidget->addWidget(createPublishPage());               // 2: 发布
    ui->m_stackedWidget->addWidget(new SearchPage(m_userId, m_username)); // 3: 搜索
    ui->m_stackedWidget->addWidget(createProfilePage());               // 4: 我的

    auto *orderPublished = new OrderListPage(m_userId, OrderListPage::Published);
    connect(orderPublished, &OrderListPage::goBack, this, [this]() { switchPage(4); });
    ui->m_stackedWidget->addWidget(orderPublished);                    // 5: 我的发布

    auto *orderPurchased = new OrderListPage(m_userId, OrderListPage::Purchased);
    connect(orderPurchased, &OrderListPage::goBack, this, [this]() { switchPage(4); });
    ui->m_stackedWidget->addWidget(orderPurchased);                    // 6: 已购买

    auto *orderPendingShip = new OrderListPage(m_userId, OrderListPage::PendingShip);
    connect(orderPendingShip, &OrderListPage::goBack, this, [this]() { switchPage(4); });
    ui->m_stackedWidget->addWidget(orderPendingShip);                  // 7: 待发货

    auto *orderPendingRecv = new OrderListPage(m_userId, OrderListPage::PendingReceipt);
    connect(orderPendingRecv, &OrderListPage::goBack, this, [this]() { switchPage(4); });
    ui->m_stackedWidget->addWidget(orderPendingRecv);                  // 8: 待收货

    auto *orderPendingConfirm = new OrderListPage(m_userId, OrderListPage::PendingConfirm);
    connect(orderPendingConfirm, &OrderListPage::goBack, this, [this]() { switchPage(4); });
    ui->m_stackedWidget->addWidget(orderPendingConfirm);               // 9: 待确认

    auto *orderReceived = new OrderListPage(m_userId, OrderListPage::Received);
    connect(orderReceived, &OrderListPage::goBack, this, [this]() { switchPage(4); });
    ui->m_stackedWidget->addWidget(orderReceived);                     // 10: 已完成

    ui->m_stackedWidget->addWidget(createChatPage());                  // 11: 聊天

    // 底部导航
    setupBottomNav();

    // 默认显示首页
    switchPage(0);

    setStyleSheet("MainWindow { background-color: #f5f6fa; }");
}

void MainWindow::setupBottomNav()
{
    struct NavItem { QString icon; QString label; };

    QList<NavItem> items = {
        {"🏠", "首页"},
        {"📦", "商品"},
        {"➕", "发布"},
        {"🔍", "搜索"},
        {"👤", "我的"}
    };

    auto *navLayout = new QHBoxLayout(ui->m_navWidget);
    navLayout->setContentsMargins(5, 5, 5, 5);
    navLayout->setSpacing(0);

    for (int i = 0; i < items.size(); ++i) {
        auto *btn = new QPushButton();
        btn->setFixedHeight(55);
        btn->setCheckable(true);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setText(items[i].icon + "\n" + items[i].label);

        QFont btnFont;
        btnFont.setPointSize(9);
        btn->setFont(btnFont);
        btn->setStyleSheet(
            "QPushButton {"
            "  background: transparent;"
            "  border: none;"
            "  color: #999;"
            "  padding: 2px 0;"
            "  font-size: 11px;"
            "}"
            "QPushButton:checked {"
            "  color: #3498db;"
            "}"
        );

        m_navGroup->addButton(btn, i);
        m_navButtons.append(btn);
        navLayout->addWidget(btn, 1);
    }

    connect(m_navGroup, &QButtonGroup::idClicked, this, [this](int id) {
        if (id == 1 && m_productsPage)
            m_productsPage->refreshProducts();
        if (id >= 0 && id <= 4)
            ui->m_stackedWidget->setCurrentIndex(id);
        if (id == 2) {
            auto *pubPage = qobject_cast<PublishPage*>(ui->m_stackedWidget->widget(2));
            if (pubPage) {
                pubPage->refreshList();
                auto *stack = pubPage->findChild<QStackedWidget*>();
                if (stack) stack->setCurrentIndex(0);
            }
        }
    });

    if (!m_navButtons.isEmpty())
        m_navButtons[0]->setChecked(true);
}

QWidget *MainWindow::createHomePage()
{
    auto *page = new HomePage(m_userId, m_username);
    connect(page, &HomePage::productClicked, this, [this](int productId) {
        ProductDetailDialog dlg(productId, m_userId, this);
        dlg.exec();
    });
    return page;
}

QWidget *MainWindow::createProductsPage()
{
    m_productsPage = new ProductsPage(m_userId, m_username);
    return m_productsPage;
}

QWidget *MainWindow::createPublishPage()
{
    auto *page = new PublishPage(m_userId, m_username);
    connect(page, &PublishPage::publishSuccess, this, [this]() {
        if (m_productsPage)
            m_productsPage->refreshProducts();
        auto *homePage = qobject_cast<HomePage*>(ui->m_stackedWidget->widget(0));
        if (homePage) {
            homePage->refreshStats();
            homePage->refreshHotProducts();
        }
        auto *profilePage = qobject_cast<ProfilePage*>(ui->m_stackedWidget->widget(4));
        if (profilePage) profilePage->refreshProfile();
        switchPage(1);
    });
    return page;
}

QWidget *MainWindow::createProfilePage()
{
    auto *page = new ProfilePage(m_userId, m_username);
    connect(page, &ProfilePage::navigateToPublished, this, [this]() { switchPage(2); });
    connect(page, &ProfilePage::navigateToPurchased, this, [this]() {
        ui->m_stackedWidget->setCurrentIndex(6);
    });
    connect(page, &ProfilePage::navigateToPendingShip, this, [this]() {
        ui->m_stackedWidget->setCurrentIndex(7);
    });
    connect(page, &ProfilePage::navigateToPendingReceipt, this, [this]() {
        ui->m_stackedWidget->setCurrentIndex(8);
    });
    connect(page, &ProfilePage::navigateToChat, this, [this]() { switchPage(11); });
    connect(page, &ProfilePage::navigateToReceived, this, [this]() {
        ui->m_stackedWidget->setCurrentIndex(10);
    });
    connect(page, &ProfilePage::navigateToPendingConfirm, this, [this]() {
        ui->m_stackedWidget->setCurrentIndex(9);
    });
    connect(page, &ProfilePage::navigateToLogout, this, [this]() {
        // 设置退出标志，防止重复退出
        m_isLoggingOut = true;
        m_sessionCheckTimer->stop();

        // 清理 session token
        QSqlDatabase db = DbManager::instance().database();
        QSqlQuery q(db);
        q.prepare("UPDATE users SET session_token = NULL WHERE id = ?");
        q.addBindValue(m_userId);
        q.exec();

        // 清空本地 token，防止后续检查误判
        m_sessionToken.clear();

        // 断开聊天连接
        ChatClient::instance().disconnectFromServer();

        // 显示退出成功提示
        QMessageBox::information(this, "退出成功", "您已成功退出登录");

        // 关闭主窗口，回到登录页
        this->close();
        LoginWindow *loginWindow = new LoginWindow();
        loginWindow->show();
    });
    return page;
}

QWidget *MainWindow::createChatPage()
{
    m_chatPage = new ChatListPage(m_userId);
    connect(m_chatPage, &ChatListPage::goBack, this, [this]() { switchPage(4); });
    return m_chatPage;
}

void MainWindow::switchPage(int index)
{
    if (index >= 0 && index < ui->m_stackedWidget->count()) {
        ui->m_stackedWidget->setCurrentIndex(index);
        if (index <= 4 && !m_navButtons.isEmpty())
            m_navButtons[index]->setChecked(true);

        if (index == 0) {
            auto *homePage = qobject_cast<HomePage*>(ui->m_stackedWidget->widget(0));
            if (homePage) {
                homePage->refreshStats();
                homePage->refreshHotProducts();
            }
        }

        if (index == 1 && m_productsPage)
            m_productsPage->refreshProducts();

        if (index == 2) {
            auto *pubPage = qobject_cast<PublishPage*>(ui->m_stackedWidget->widget(2));
            if (pubPage) {
                pubPage->refreshList();
                auto *stack = pubPage->findChild<QStackedWidget*>();
                if (stack) stack->setCurrentIndex(0);
            }
        }

        if (index == 4) {
            auto *profilePage = qobject_cast<ProfilePage*>(ui->m_stackedWidget->widget(4));
            if (profilePage) profilePage->refreshProfile();
            for (int i = 5; i <= 10; ++i) {
                auto *orderPage = qobject_cast<OrderListPage*>(ui->m_stackedWidget->widget(i));
                if (orderPage) orderPage->refreshData();
            }
        }

        if (index >= 5 && index <= 10) {
            auto *orderPage = qobject_cast<OrderListPage*>(ui->m_stackedWidget->widget(index));
            if (orderPage) orderPage->refreshData();
        }

        if (index == 11 && m_chatPage)
            m_chatPage->refreshList();
    }
}

void MainWindow::showNotification(int fromUserId, const QString &fromUsername,
                                  const QString &content)
{
    m_notificationFromUserId = fromUserId;
    m_notificationFromUsername = fromUsername;
    m_notificationNameLabel->setText(fromUsername);

    QString preview = content.length() > 25 ? content.left(25) + "..." : content;
    m_notificationContentLabel->setText(preview);

    ui->m_notificationBar->show();
    m_notificationTimer->start();
}

void MainWindow::onNotificationClicked()
{
    m_notificationTimer->stop();
    ui->m_notificationBar->hide();

    if (m_notificationFromUserId > 0) {
        switchPage(11);
        m_chatPage->openChat(m_notificationFromUserId, m_notificationFromUsername);
    }
}

void MainWindow::onMessageReceived(int fromUserId, const QString &fromUsername,
                                   const QString &content, qint64 timestamp)
{
    Q_UNUSED(timestamp);

    if (isMinimized() || !isVisible())
        return;

    bool hasOpenChat = false;
    QList<QWidget*> children = findChildren<QWidget*>();
    for (QWidget *child : children) {
        ChatDialog *chatDlg = qobject_cast<ChatDialog*>(child);
        if (chatDlg) {
            hasOpenChat = true;
            break;
        }
    }

    if (!hasOpenChat && m_chatPage && m_chatPage->isChattingWith(fromUserId))
        hasOpenChat = true;

    if (!hasOpenChat) {
        showNotification(fromUserId, fromUsername, content);
    }
}

void MainWindow::checkSessionValidity()
{
    if (m_sessionToken.isEmpty()) return;

    if (!isSessionValid()) {
        logoutDueToConflict();
    }
}

bool MainWindow::isSessionValid()
{
    if (m_sessionToken.isEmpty()) return true;

    QSqlDatabase db = DbManager::instance().database();
    QSqlQuery query(db);
    query.prepare("SELECT session_token FROM users WHERE id = ?");
    query.addBindValue(m_userId);

    if (query.exec() && query.next()) {
        QString storedToken = query.value(0).toString();
        return storedToken == m_sessionToken;
    }

    return false;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_isLoggingOut)
    {
        event->accept();
        return;
    }

    int ret = QMessageBox::question(this, "提示", "确定要关闭窗口吗？",
                                    QMessageBox::Yes | QMessageBox::No);
    if (ret == QMessageBox::Yes)
    {
        m_sessionCheckTimer->stop();
        ChatClient::instance().disconnectFromServer();
        event->accept();
    }
    else
    {
        event->ignore();
    }
}

void MainWindow::logoutDueToConflict()
{
    if (m_isLoggingOut)
        return;

    m_isLoggingOut = true;
    m_sessionCheckTimer->stop();

    QMessageBox::warning(this, "登录提示", "您的账号已在其他设备登录，将自动退出");

    ChatClient::instance().disconnectFromServer();

    this->close();
    LoginWindow *loginWindow = new LoginWindow();
    loginWindow->show();
}
