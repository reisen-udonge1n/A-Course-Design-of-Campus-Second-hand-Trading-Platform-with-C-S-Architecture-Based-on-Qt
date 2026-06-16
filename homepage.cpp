#include "homepage.h"
#include "ui_homepage.h"
#include "dbmanager.h"
#include "util.h"
#include "followersdialog.h"
#include "userproductsdialog.h"
#include <QShowEvent>

#include <QLabel>
#include <QGridLayout>
#include <QFrame>
#include <QScrollArea>
#include <QFont>
#include <QHBoxLayout>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QPushButton>
#include <QScrollBar>
#include <QPixmap>
#include <QDebug>

HomePage::HomePage(int userId, const QString &username, QWidget *parent)
    : QWidget(parent)
    , m_userId(userId)
    , m_username(username)
    , ui(new Ui::HomePage)
{
    ui->setupUi(this);
    setupUI();
}

HomePage::~HomePage()
{
    delete ui;
}

void HomePage::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    if (!m_loaded) {
        m_loaded = true;
        loadStats();
        refreshHotProducts();
    }
}

void HomePage::setupUI()
{
    auto *mainLayout = qobject_cast<QVBoxLayout*>(layout());
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

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
    auto *pageLayout = new QVBoxLayout(page);
    pageLayout->setContentsMargins(15, 15, 15, 15);
    pageLayout->setSpacing(15);

    auto *headerLabel = new QLabel(QString("👋 欢迎回来，%1").arg(m_username));
    QFont headerFont;
    headerFont.setPointSize(18);
    headerFont.setBold(true);
    headerLabel->setFont(headerFont);
    headerLabel->setStyleSheet("color: #2c3e50;");
    pageLayout->addWidget(headerLabel);

    auto *statsFrame = new QFrame();
    statsFrame->setStyleSheet("QFrame { background-color: #ffffff; border-radius: 12px; }");
    statsFrame->setFixedHeight(75);
    auto *statsLayout = new QHBoxLayout(statsFrame);
    statsLayout->setContentsMargins(5, 8, 5, 8);
    statsLayout->setSpacing(0);

    struct StatInfo { QString icon; QString label; QLabel **countLabel; };
    QList<StatInfo> stats = {
        {"📦", "已发布", &m_publishedCount},
        {"🛒", "已购买", &m_purchasedCount},
        {"👥", "关注", &m_followingCount},
    };

    for (int i = 0; i < stats.size(); ++i) {
        if (i > 0) {
            auto *sep = new QFrame();
            sep->setFixedWidth(1);
            sep->setFixedHeight(35);
            sep->setStyleSheet("background-color: #e0e0e0;");
            statsLayout->addWidget(sep);
        }

        auto *item = new QWidget();
        item->setStyleSheet("background: transparent;");
        auto *itemLayout = new QVBoxLayout(item);
        itemLayout->setAlignment(Qt::AlignCenter);
        itemLayout->setSpacing(2);

        auto *iconLabel = new QLabel(stats[i].icon);
        iconLabel->setAlignment(Qt::AlignCenter);
        iconLabel->setStyleSheet("font-size: 18px; background: transparent;");
        itemLayout->addWidget(iconLabel);

        *stats[i].countLabel = new QLabel("0");
        (*stats[i].countLabel)->setAlignment(Qt::AlignCenter);
        QFont countFont;
        countFont.setPointSize(15);
        countFont.setBold(true);
        (*stats[i].countLabel)->setFont(countFont);
        (*stats[i].countLabel)->setStyleSheet("color: #2c3e50; background: transparent;");
        itemLayout->addWidget(*stats[i].countLabel);

        auto *nameLabel = new QLabel(stats[i].label);
        nameLabel->setAlignment(Qt::AlignCenter);
        nameLabel->setStyleSheet("color: #7f8c8d; font-size: 11px; background: transparent;");
        itemLayout->addWidget(nameLabel);

        statsLayout->addWidget(item, 1);
    }

    pageLayout->addWidget(statsFrame);

    auto *categoryFrame = new QFrame();
    categoryFrame->setStyleSheet("QFrame { background-color: #ffffff; border-radius: 12px; }");
    auto *categoryLayout = new QHBoxLayout(categoryFrame);
    categoryLayout->setContentsMargins(15, 12, 15, 12);
    categoryLayout->setSpacing(10);

    auto *followBtn = new QPushButton("👥 关注");
    followBtn->setCursor(Qt::PointingHandCursor);
    followBtn->setFixedSize(65, 36);
    followBtn->setStyleSheet(
        "QPushButton {"
        "  background-color: #3498db; color: white; border: none;"
        "  border-radius: 18px; font-size: 12px; font-weight: bold;"
        "}"
        "QPushButton:hover { background-color: #2980b9; }"
    );
    categoryLayout->addWidget(followBtn);
    connect(followBtn, &QPushButton::clicked, this, &HomePage::onFollowBtnClicked);

    auto *categoryScroll = new QScrollArea();
    categoryScroll->setWidgetResizable(false);
    categoryScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    categoryScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    categoryScroll->setStyleSheet(
        "QScrollArea { border: none; background: transparent; }"
        "QScrollBar:horizontal {"
        "  height: 4px; background: transparent;"
        "}"
        "QScrollBar::handle:horizontal {"
        "  background: #ccc; border-radius: 2px; min-width: 20px;"
        "}"
        "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0px; }"
    );
    categoryScroll->setFrameShape(QFrame::NoFrame);

    auto *categoryWidget = new QWidget();
    auto *categoryScrollLayout = new QHBoxLayout(categoryWidget);
    categoryScrollLayout->setContentsMargins(0, 0, 0, 0);
    categoryScrollLayout->setSpacing(10);

    QStringList categories = {
        "全部", "数码", "穿搭", "家电", "图书",
        "美妆", "运动", "食品", "宠物", "家具",
        "文具", "乐器", "其他"
    };

    for (int i = 0; i < categories.size(); ++i) {
        auto *tagBtn = new QPushButton(categories[i]);
        tagBtn->setCursor(Qt::PointingHandCursor);
        tagBtn->setStyleSheet(
            "QPushButton {"
            "  background-color: #f0f0f0; color: #555; border: none;"
            "  border-radius: 20px; font-size: 13px; padding: 6px 16px;"
            "}"
            "QPushButton:hover { background-color: #e0e0e0; }"
            "QPushButton:checked {"
            "  background-color: #e74c3c; color: white;"
            "}"
        );
        tagBtn->setCheckable(true);
        tagBtn->setMinimumWidth(60);
        tagBtn->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
        if (i == 0) tagBtn->setChecked(true);
        m_categoryButtons.append(tagBtn);
        categoryScrollLayout->addWidget(tagBtn);

        QString categoryName = categories[i];
        connect(tagBtn, &QPushButton::clicked, this, [this, i, categoryName]() {
            for (int j = 0; j < m_categoryButtons.size(); ++j) {
                m_categoryButtons[j]->setChecked(j == i);
            }
            m_currentCategory = categoryName;
            refreshHotProducts();
        });
    }

    categoryWidget->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    categoryWidget->adjustSize();
    categoryScroll->setWidget(categoryWidget);
    categoryScroll->setFixedHeight(36);
    categoryScroll->setObjectName("categoryScroll");
    categoryLayout->addWidget(categoryScroll, 1);

    m_categoryScroll = categoryScroll;

    pageLayout->addWidget(categoryFrame);

    auto *sectionLabel = new QLabel("🔥 热门推荐");
    QFont sectionFont;
    sectionFont.setPointSize(15);
    sectionFont.setBold(true);
    sectionLabel->setFont(sectionFont);
    sectionLabel->setStyleSheet("color: #34495e; margin-top: 5px;");
    pageLayout->addWidget(sectionLabel);

    m_gridWidget = new QWidget();
    m_gridWidget->setStyleSheet("background: transparent;");
    m_gridLayout = new QGridLayout(m_gridWidget);
    m_gridLayout->setContentsMargins(0, 0, 0, 0);
    m_gridLayout->setSpacing(12);

    pageLayout->addWidget(m_gridWidget);
    pageLayout->addStretch();

    scrollArea->setWidget(page);
    mainLayout->addWidget(scrollArea, 1);

    m_mainScrollArea = scrollArea;
    connect(scrollArea->verticalScrollBar(), &QScrollBar::valueChanged,
            this, [this](int value) {
        QScrollBar *sb = m_mainScrollArea->verticalScrollBar();
        if (!m_loading && m_hasMore && sb->maximum() - value < 400)
            loadMoreProducts();
    });
}

void HomePage::loadStats()
{
    if (!DbManager::instance().isConnected()) return;

    auto fmt = [](int n) { return n > 99 ? "99+" : QString::number(n); };

    QSqlDatabase db = DbManager::instance().database();
    QSqlQuery q(db);

    q.prepare("SELECT COUNT(*) FROM products WHERE seller_id = ?");
    q.addBindValue(m_userId);
    if (q.exec() && q.next())
        m_publishedCount->setText(fmt(q.value(0).toInt()));

    q.prepare("SELECT COUNT(*) FROM orders WHERE buyer_id = ?");
    q.addBindValue(m_userId);
    if (q.exec() && q.next())
        m_purchasedCount->setText(fmt(q.value(0).toInt()));

    q.prepare("SELECT COUNT(*) FROM followers WHERE follower_id = ?");
    q.addBindValue(m_userId);
    if (q.exec() && q.next())
        m_followingCount->setText(fmt(q.value(0).toInt()));
}

void HomePage::refreshStats()
{
    loadStats();
}

void HomePage::refreshHotProducts()
{
    QLayoutItem *item;
    while ((item = m_gridLayout->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }

    m_offset = 0;
    m_hasMore = true;
    m_loading = false;
    loadMoreProducts();
}

void HomePage::loadMoreProducts()
{
    if (m_loading || !m_hasMore) return;
    m_loading = true;

    if (!DbManager::instance().isConnected()) {
        if (m_offset == 0) {
            auto *empty = new QLabel("暂无商品\n\n快去发布一件商品吧！");
            empty->setAlignment(Qt::AlignCenter);
            empty->setStyleSheet("color: #bdc3c7; font-size: 16px; padding: 60px 0; background: transparent;");
            m_gridLayout->addWidget(empty, 0, 0, 1, 2);
        }
        m_loading = false;
        return;
    }

    QSqlDatabase db = DbManager::instance().database();
    QSqlQuery q(db);

    if (m_currentCategory == QStringLiteral("全部")) {
        q.prepare("SELECT id, title, price, category, images FROM products "
                  "WHERE status = 0 ORDER BY created_at DESC LIMIT ? OFFSET ?");
    } else {
        q.prepare("SELECT id, title, price, category, images FROM products "
                  "WHERE status = 0 AND category = ? ORDER BY created_at DESC LIMIT ? OFFSET ?");
        q.addBindValue(m_currentCategory);
    }
    q.addBindValue(PAGE_SIZE);
    q.addBindValue(m_offset);

    if (!q.exec()) {
        qDebug() << "查询商品失败:" << q.lastError().text();
        m_loading = false;
        return;
    }

    int row = m_offset / 2;
    int col = m_offset % 2;
    int count = 0;

    while (q.next()) {
        count++;
        int id = q.value(0).toInt();
        QString title = q.value(1).toString();
        double price = q.value(2).toDouble();
        QString category = q.value(3).toString();
        QString imagesStr = q.value(4).toString();

        QStringList images;
        if (!imagesStr.isEmpty())
            images = imagesStr.split(",", Qt::SkipEmptyParts);

        auto *card = createProductCard(id, title, price, category, images);
        card->setProperty("productId", id);
        card->installEventFilter(this);

        m_gridLayout->addWidget(card, row, col);

        col++;
        if (col >= 2) { col = 0; row++; }
    }

    m_offset += count;
    m_hasMore = (count == PAGE_SIZE);

    if (m_offset == 0) {
        auto *empty = new QLabel("暂无商品\n\n快去发布一件商品吧！");
        empty->setAlignment(Qt::AlignCenter);
        empty->setStyleSheet("color: #bdc3c7; font-size: 16px; padding: 60px 0; background: transparent;");
        m_gridLayout->addWidget(empty, 0, 0, 1, 2);
    }

    m_gridLayout->setRowStretch(row, 1);
    m_loading = false;
}

QWidget *HomePage::createProductCard(int id, const QString &title, double price,
                                     const QString &category, const QStringList &images)
{
    Q_UNUSED(id);

    auto *card = new QFrame();
    card->setFixedSize(175, 220);
    card->setStyleSheet(
        "QFrame { background-color: #ffffff; border-radius: 12px; }"
        "QFrame:hover { background-color: #f8f9fa; }"
    );
    card->setCursor(Qt::PointingHandCursor);

    auto *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(0, 0, 0, 12);
    cardLayout->setSpacing(6);
    cardLayout->setAlignment(Qt::AlignCenter);

    auto *imageLabel = new QLabel();
    imageLabel->setAlignment(Qt::AlignCenter);
    imageLabel->setFixedSize(175, 130);
    imageLabel->setStyleSheet("background-color: #ecf0f1; border-radius: 12px 12px 0 0;");

    if (!images.isEmpty()) {
        QPixmap pix = loadPixmap(images[0]);
        if (!pix.isNull()) {
            imageLabel->setPixmap(pix.scaled(175, 130, Qt::KeepAspectRatio, Qt::FastTransformation));
        } else {
            imageLabel->setText("📷");
        }
    } else {
        imageLabel->setText("📷");
    }
    imageLabel->setStyleSheet("background-color: #ecf0f1; border-radius: 12px 12px 0 0; font-size: 40px;");
    cardLayout->addWidget(imageLabel);

    if (!category.isEmpty()) {
        auto *tagLabel = new QLabel(category);
        tagLabel->setStyleSheet(
            "color: white; background-color: #e74c3c; border-radius: 4px;"
            "padding: 2px 8px; font-size: 11px; max-width: 50px;"
        );
        tagLabel->setAlignment(Qt::AlignCenter);
        auto *tagLayout = new QHBoxLayout();
        tagLayout->setContentsMargins(12, 0, 12, 0);
        tagLayout->addWidget(tagLabel);
        tagLayout->addStretch();
        cardLayout->addLayout(tagLayout);
    }

    auto *titleLabel = new QLabel(title);
    titleLabel->setStyleSheet("font-size: 14px; font-weight: bold; color: #2c3e50; background: transparent;");
    titleLabel->setContentsMargins(12, 0, 12, 0);
    titleLabel->setWordWrap(true);
    cardLayout->addWidget(titleLabel);

    auto *priceLabel = new QLabel(QString("¥ %1").arg(price, 0, 'f', 2));
    priceLabel->setStyleSheet("font-size: 16px; font-weight: bold; color: #e74c3c; background: transparent;");
    priceLabel->setContentsMargins(12, 0, 12, 0);
    cardLayout->addWidget(priceLabel);

    return card;
}

void HomePage::onFollowBtnClicked()
{
    FollowersDialog dlg(m_userId, m_username, this);
    connect(&dlg, &FollowersDialog::followChanged, this, &HomePage::loadStats);
    connect(&dlg, &FollowersDialog::viewUserProducts, this, [this](int userId, const QString &username) {
        auto *productsDlg = new UserProductsDialog(userId, username, m_userId, this);
        productsDlg->exec();
        delete productsDlg;
    });
    dlg.exec();
}

bool HomePage::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonRelease) {
        QWidget *w = qobject_cast<QWidget*>(obj);
        if (w) {
            int id = w->property("productId").toInt();
            if (id > 0) {
                emit productClicked(id);
            }
        }
    }
    return QWidget::eventFilter(obj, event);
}
