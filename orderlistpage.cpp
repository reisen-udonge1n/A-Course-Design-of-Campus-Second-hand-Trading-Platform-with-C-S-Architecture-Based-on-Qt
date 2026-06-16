#include "orderlistpage.h"
#include "ui_orderlistpage.h"
#include "dbmanager.h"
#include "chatdialog.h"

#include <QHBoxLayout>
#include <QScrollArea>
#include <QPushButton>
#include <QFrame>
#include <QFont>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include <QDateTime>
#include <QMessageBox>
#include <QInputDialog>

OrderListPage::OrderListPage(int userId, Mode mode, QWidget *parent)
    : QWidget(parent)
    , m_userId(userId)
    , m_mode(mode)
    , ui(new Ui::OrderListPage)
{
    ui->setupUi(this);
    setupUI();
}

OrderListPage::~OrderListPage()
{
    delete ui;
}

QString OrderListPage::modeTitle() const
{
    switch (m_mode) {
        case Published: return "我的发布";
        case Purchased: return "已购买";
        case PendingShip: return "待发货";
        case PendingReceipt: return "待收货";
        case PendingConfirm: return "待确认";
        case Received: return "已完成";
    }
    return "";
}

QString OrderListPage::modeQuery() const
{
    switch (m_mode) {
        case Published:
            return "SELECT id, title, price, status, created_at FROM products "
                   "WHERE seller_id = ? AND id IN ("
                   "  SELECT MAX(id) FROM products WHERE seller_id = ? GROUP BY title"
                   ") ORDER BY created_at DESC";
        case Purchased:
            return "SELECT p.id, p.title, p.price, o.status, o.created_at "
                   "FROM orders o JOIN products p ON o.product_id = p.id "
                   "WHERE o.buyer_id = ? AND o.id IN ("
                   "  SELECT MAX(o2.id) FROM orders o2"
                   "    JOIN products p2 ON o2.product_id = p2.id"
                   "  WHERE o2.buyer_id = ? GROUP BY p2.title"
                   ") ORDER BY o.created_at DESC";
        case PendingShip:
            return "SELECT p.id, p.title, p.price, o.status, o.created_at, "
                   "o.buyer_id, o.id "
                   "FROM orders o JOIN products p ON o.product_id = p.id "
                   "WHERE o.seller_id = ? AND o.status = 2 ORDER BY o.created_at DESC";
        case PendingReceipt:
            return "SELECT p.id, p.title, p.price, o.status, o.created_at, "
                   "o.seller_id, o.id "
                   "FROM orders o JOIN products p ON o.product_id = p.id "
                   "WHERE o.buyer_id = ? AND o.status = 5 ORDER BY o.created_at DESC";
        case PendingConfirm:
            return "SELECT o.id, p.title, p.price, o.status, o.created_at, "
                   "u.username AS buyer_name, o.buyer_id, o.offer_price, p.id "
                   "FROM orders o "
                   "JOIN products p ON o.product_id = p.id "
                   "JOIN users u ON o.buyer_id = u.id "
                   "WHERE o.seller_id = ? AND o.status = 0 ORDER BY o.created_at DESC";
        case Received:
            return "SELECT p.id, p.title, p.price, o.status, o.created_at, "
                   "o.seller_id, o.id "
                   "FROM orders o JOIN products p ON o.product_id = p.id "
                   "WHERE o.buyer_id = ? AND o.status = 3 ORDER BY o.created_at DESC";
    }
    return "";
}

void OrderListPage::setupUI()
{
    ui->m_titleLabel->setText(modeTitle());

    // Set up the list layout inside the scroll area
    auto *scrollArea = ui->scrollArea;
    auto *content = qobject_cast<QWidget*>(scrollArea->widget());
    if (content) {
        auto *contentLayout = qobject_cast<QVBoxLayout*>(content->layout());
        m_listLayout = new QVBoxLayout();
        m_listLayout->setSpacing(12);
        contentLayout->insertLayout(0, m_listLayout);

        // Move empty label after list layout
        contentLayout->insertWidget(1, ui->m_emptyLabel);
    }

    connect(ui->m_backBtn, &QPushButton::clicked, this, &OrderListPage::goBack);
}

void OrderListPage::refreshData()
{
    loadData();
}

void OrderListPage::loadData()
{
    if (!DbManager::instance().isConnected()) return;

    QSqlDatabase db = DbManager::instance().database();
    QSqlQuery query(db);
    query.prepare(modeQuery());
    query.addBindValue(m_userId);
    if (m_mode == Published || m_mode == Purchased)
        query.addBindValue(m_userId);

    if (!query.exec()) {
        qDebug() << "查询" << modeTitle() << "失败:" << query.lastError().text();
        return;
    }

    bool hasData = false;

    while (query.next()) {
        hasData = true;

        if (m_mode == PendingConfirm) {
            int orderId = query.value(0).toInt();
            QString title = query.value(1).toString();
            double price = query.value(2).toDouble();
            int status = query.value(3).toInt();
            QString createdAt = query.value(4).toString();
            QString buyerName = query.value(5).toString();
            int buyerId = query.value(6).toInt();
            double offerPrice = query.value(7).toDouble();
            int productId = query.value(8).toInt();

            auto *card = new QFrame();
            card->setStyleSheet("QFrame { background-color: #ffffff; border-radius: 10px; }");
            auto *cardLayout = new QVBoxLayout(card);
            cardLayout->setContentsMargins(15, 12, 15, 12);
            cardLayout->setSpacing(8);

            auto *titleRow = new QHBoxLayout();
            auto *titleLbl = new QLabel(title);
            QFont tf;
            tf.setPointSize(14);
            tf.setBold(true);
            titleLbl->setFont(tf);
            titleLbl->setStyleSheet("color: #2c3e50; background: transparent;");
            titleRow->addWidget(titleLbl, 1);

            auto *priceLbl = new QLabel(QString("¥ %1").arg(price, 0, 'f', 2));
            priceLbl->setStyleSheet("color: #e74c3c; font-size: 15px; font-weight: bold; background: transparent;");
            titleRow->addWidget(priceLbl);
            cardLayout->addLayout(titleRow);

            auto *offerLbl = new QLabel(QString("买家出价：¥%1").arg(offerPrice, 0, 'f', 2));
            offerLbl->setStyleSheet("color: #e67e22; font-size: 13px; font-weight: bold; background: transparent;");
            cardLayout->addWidget(offerLbl);

            auto *infoLbl = new QLabel(QString("买家：%1  |  %2")
                .arg(buyerName, QDateTime::fromString(createdAt, "yyyy-MM-dd hh:mm:ss")
                    .toString("MM-dd hh:mm")));
            infoLbl->setStyleSheet("color: #7f8c8d; font-size: 12px; background: transparent;");
            cardLayout->addWidget(infoLbl);

            auto *btnRow = new QHBoxLayout();
            btnRow->setSpacing(12);

            auto *acceptBtn = new QPushButton("✓ 允许");
            acceptBtn->setCursor(Qt::PointingHandCursor);
            acceptBtn->setFixedHeight(36);
            acceptBtn->setStyleSheet(
                "QPushButton { background-color: #27ae60; color: white; border: none;"
                "  border-radius: 8px; font-size: 14px; font-weight: bold; }"
                "QPushButton:hover { background-color: #219a52; }"
            );
            btnRow->addWidget(acceptBtn, 1);

            auto *rejectBtn = new QPushButton("✗ 拒绝");
            rejectBtn->setCursor(Qt::PointingHandCursor);
            rejectBtn->setFixedHeight(36);
            rejectBtn->setStyleSheet(
                "QPushButton { background-color: #e74c3c; color: white; border: none;"
                "  border-radius: 8px; font-size: 14px; font-weight: bold; }"
                "QPushButton:hover { background-color: #c0392b; }"
            );
            btnRow->addWidget(rejectBtn, 1);

            cardLayout->addLayout(btnRow);

            m_listLayout->addWidget(card);

            QString sellerName;
            QSqlQuery sq(db);
            sq.prepare("SELECT username FROM users WHERE id = ?");
            sq.addBindValue(m_userId);
            if (sq.exec() && sq.next())
                sellerName = sq.value(0).toString();

            connect(acceptBtn, &QPushButton::clicked, this, [this, orderId, productId, buyerId, buyerName, sellerName, offerPrice]() {
                QMessageBox::StandardButton reply = QMessageBox::question(
                    nullptr, "确认报价",
                    QString("买家报价 ¥%1\n\n是否将商品价格修改为此价格？\n\n"
                            "选择「是」→ 弹出修改价格窗口\n"
                            "选择「否」→ 按买家报价成交")
                        .arg(offerPrice, 0, 'f', 2),
                    QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel,
                    QMessageBox::No);

                if (reply == QMessageBox::Cancel) return;

                QSqlDatabase db = DbManager::instance().database();

                if (reply == QMessageBox::Yes) {
                    bool ok;
                    double newPrice = QInputDialog::getDouble(
                        nullptr, "修改商品价格",
                        QString("当前价格：¥%1\n买家报价：¥%2\n\n请输入新的商品价格：")
                            .arg(offerPrice, 0, 'f', 2)
                            .arg(offerPrice, 0, 'f', 2),
                        offerPrice, 0.01, 9999999.99, 2, &ok);
                    if (!ok) return;

                    QSqlQuery qp(db);
                    qp.prepare("UPDATE products SET price = ?, status = 1 WHERE id = ?");
                    qp.addBindValue(newPrice);
                    qp.addBindValue(productId);
                    qp.exec();

                    QSqlQuery qo(db);
                    qo.prepare("UPDATE orders SET status = 2 WHERE id = ?");
                    qo.addBindValue(orderId);
                    if (qo.exec()) {
                        ChatDialog dlg(m_userId, sellerName,
                                       buyerId, buyerName, this,
                                       QString("卖家已将价格改为 ¥%1 并同意成交，准备为您发货")
                                           .arg(newPrice, 0, 'f', 2));
                        dlg.exec();
                        loadData();
                    }
                } else {
                    QSqlQuery qp(db);
                    qp.prepare("UPDATE products SET status = 1 WHERE id = ?");
                    qp.addBindValue(productId);
                    qp.exec();

                    QSqlQuery q(db);
                    q.prepare("UPDATE orders SET status = 2 WHERE id = ?");
                    q.addBindValue(orderId);
                    if (q.exec()) {
                        ChatDialog dlg(m_userId, sellerName,
                                       buyerId, buyerName, this,
                                       QString("卖家已同意按 ¥%1 成交，准备为您发货")
                                           .arg(offerPrice, 0, 'f', 2));
                        dlg.exec();
                        loadData();
                    }
                }
            });

            connect(rejectBtn, &QPushButton::clicked, this, [this, orderId, buyerId, buyerName, sellerName]() {
                QSqlDatabase db = DbManager::instance().database();
                QSqlQuery q(db);
                q.prepare("UPDATE orders SET status = 4 WHERE id = ?");
                q.addBindValue(orderId);
                if (q.exec()) {
                    ChatDialog dlg(m_userId, sellerName,
                                   buyerId, buyerName, this, "卖家不同意您的报价");
                    dlg.exec();
                    loadData();
                }
            });
        } else {
            int productId = query.value(0).toInt();
            QString title = query.value(1).toString();
            double price = query.value(2).toDouble();
            int status = query.value(3).toInt();
            QString createdAt = query.value(4).toString();

            auto *card = new QFrame();
            card->setFixedHeight(90);
            card->setStyleSheet("QFrame { background-color: #ffffff; border-radius: 10px; }");

            auto *cardLayout = new QHBoxLayout(card);
            cardLayout->setContentsMargins(15, 12, 15, 12);
            cardLayout->setSpacing(12);

            auto *imgLabel = new QLabel("📷");
            imgLabel->setFixedSize(66, 66);
            imgLabel->setAlignment(Qt::AlignCenter);
            imgLabel->setStyleSheet("background-color: #ecf0f1; border-radius: 8px; font-size: 30px;");
            cardLayout->addWidget(imgLabel);

            auto *infoLayout = new QVBoxLayout();
            infoLayout->setSpacing(4);

            auto *titleLbl = new QLabel(title);
            QFont titleF;
            titleF.setPointSize(14);
            titleF.setBold(true);
            titleLbl->setFont(titleF);
            titleLbl->setStyleSheet("color: #2c3e50; background: transparent;");
            infoLayout->addWidget(titleLbl);

            auto *priceLbl = new QLabel(QString("¥ %1").arg(price, 0, 'f', 2));
            priceLbl->setStyleSheet("color: #e74c3c; font-size: 15px; font-weight: bold; background: transparent;");
            infoLayout->addWidget(priceLbl);

            QStringList statusTexts = {"待确认", "", "待发货", "已完成", "已取消", "已发货"};
            int statusIdx = (status >= 0 && status < statusTexts.size()) ? status : 0;
            auto *statusLbl = new QLabel(statusTexts[statusIdx]);
            statusLbl->setStyleSheet("color: #3498db; font-size: 12px; background: transparent;");
            infoLayout->addWidget(statusLbl);

            cardLayout->addLayout(infoLayout, 1);

            if (m_mode == PendingShip) {
                int buyerId = query.value(5).toInt();
                int orderId = query.value(6).toInt();

                QString buyerName;
                QSqlQuery uq(db);
                uq.prepare("SELECT username FROM users WHERE id = ?");
                uq.addBindValue(buyerId);
                if (uq.exec() && uq.next())
                    buyerName = uq.value(0).toString();

                QString sellerName;
                QSqlQuery sq(db);
                sq.prepare("SELECT username FROM users WHERE id = ?");
                sq.addBindValue(m_userId);
                if (sq.exec() && sq.next())
                    sellerName = sq.value(0).toString();

                auto *shipBtn = new QPushButton("已发货");
                shipBtn->setCursor(Qt::PointingHandCursor);
                shipBtn->setFixedSize(80, 32);
                shipBtn->setStyleSheet(
                    "QPushButton { background-color: #3498db; color: white; border: none;"
                    "  border-radius: 6px; font-size: 12px; font-weight: bold; }"
                    "QPushButton:hover { background-color: #2980b9; }"
                );
                cardLayout->addWidget(shipBtn, 0, Qt::AlignRight | Qt::AlignVCenter);

                connect(shipBtn, &QPushButton::clicked, this, [this, orderId, buyerId, buyerName, sellerName]() {
                    QSqlDatabase db = DbManager::instance().database();
                    QSqlQuery q(db);
                    q.prepare("UPDATE orders SET status = 5 WHERE id = ?");
                    q.addBindValue(orderId);
                    if (q.exec()) {
                        ChatDialog dlg(m_userId, sellerName,
                                       buyerId, buyerName, this,
                                       "卖家已发货，请注意查收");
                        dlg.exec();
                        loadData();
                    }
                });
            }

            if (m_mode == PendingReceipt) {
                int sellerId = query.value(5).toInt();
                int orderId = query.value(6).toInt();

                auto *confirmBtn = new QPushButton("确认收货");
                confirmBtn->setCursor(Qt::PointingHandCursor);
                confirmBtn->setFixedSize(80, 32);
                confirmBtn->setStyleSheet(
                    "QPushButton { background-color: #27ae60; color: white; border: none;"
                    "  border-radius: 6px; font-size: 12px; font-weight: bold; }"
                    "QPushButton:hover { background-color: #219a52; }"
                );
                cardLayout->addWidget(confirmBtn, 0, Qt::AlignRight | Qt::AlignVCenter);

                QString buyerName;
                QSqlQuery uq(db);
                uq.prepare("SELECT username FROM users WHERE id = ?");
                uq.addBindValue(m_userId);
                if (uq.exec() && uq.next())
                    buyerName = uq.value(0).toString();

                connect(confirmBtn, &QPushButton::clicked, this, [this, orderId, sellerId, buyerName]() {
                    if (QMessageBox::question(nullptr, "确认收货",
                            "确定已收到商品吗？确认后交易完成。",
                            QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
                        return;

                    QSqlDatabase db = DbManager::instance().database();

                    QSqlQuery q(db);
                    q.prepare("UPDATE orders SET status = 3 WHERE id = ?");
                    q.addBindValue(orderId);
                    if (!q.exec()) return;

                    QString sellerName;
                    QSqlQuery sq(db);
                    sq.prepare("SELECT username FROM users WHERE id = ?");
                    sq.addBindValue(sellerId);
                    if (sq.exec() && sq.next())
                        sellerName = sq.value(0).toString();

                    ChatDialog dlg(m_userId, buyerName,
                                   sellerId, sellerName, this,
                                   "买方已经确认收货，请检查资金是否到账");
                    dlg.exec();
                    loadData();
                });
            }

            if (m_mode == Received) {
                int orderId = query.value(6).toInt();

                auto *deleteBtn = new QPushButton("删除");
                deleteBtn->setCursor(Qt::PointingHandCursor);
                deleteBtn->setFixedSize(60, 28);
                deleteBtn->setStyleSheet(
                    "QPushButton { background-color: #e74c3c; color: white; border: none;"
                    "  border-radius: 6px; font-size: 12px; }"
                    "QPushButton:hover { background-color: #c0392b; }"
                );
                cardLayout->addWidget(deleteBtn, 0, Qt::AlignRight | Qt::AlignVCenter);

                connect(deleteBtn, &QPushButton::clicked, this, [this, orderId]() {
                    if (QMessageBox::question(nullptr, "删除订单",
                            "确定要删除此订单记录吗？",
                            QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
                        return;

                    QSqlDatabase db = DbManager::instance().database();
                    QSqlQuery q(db);
                    q.prepare("DELETE FROM orders WHERE id = ?");
                    q.addBindValue(orderId);
                    if (q.exec())
                        loadData();
                });
            }

            m_listLayout->addWidget(card);
        }
    }

    ui->m_emptyLabel->setVisible(!hasData);
}
