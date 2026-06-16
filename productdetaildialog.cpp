#include "productdetaildialog.h"
#include "ui_productdetaildialog.h"
#include "chatdialog.h"
#include "dbmanager.h"
#include "util.h"

#include <QFont>
#include <QPixmap>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QMessageBox>
#include <QInputDialog>
#include <QDebug>

ProductDetailDialog::ProductDetailDialog(int productId, int currentUserId, QWidget *parent)
    : QDialog(parent)
    , m_productId(productId)
    , m_currentUserId(currentUserId)
    , m_sellerId(-1)
    , ui(new Ui::ProductDetailDialog)
{
    ui->setupUi(this);

    setFixedSize(400, 650);

    connect(ui->closeBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(ui->m_chatBtn, &QPushButton::clicked, this, &ProductDetailDialog::onChatClicked);
    connect(ui->m_buyBtn, &QPushButton::clicked, this, &ProductDetailDialog::onBuyClicked);

    loadData();
}

ProductDetailDialog::~ProductDetailDialog()
{
    delete ui;
}

void ProductDetailDialog::loadData()
{
    if (!DbManager::instance().isConnected()) return;

    QSqlDatabase db = DbManager::instance().database();

    QSqlQuery q(db);
    q.prepare("SELECT title, price, description, category, images, seller_id FROM products WHERE id = ?");
    q.addBindValue(m_productId);

    if (!q.exec() || !q.next()) {
        qDebug() << "商品详情：查询失败";
        return;
    }

    QString title = q.value(0).toString();
    double price = q.value(1).toDouble();
    QString description = q.value(2).toString();
    QString category = q.value(3).toString();
    QString imagesStr = q.value(4).toString();
    m_sellerId = q.value(5).toInt();

    m_productTitle = title;
    m_productPrice = price;
    ui->m_titleLabel->setText(title);
    ui->m_priceLabel->setText(QString("¥ %1").arg(price, 0, 'f', 2));

    if (!category.isEmpty()) {
        ui->m_categoryLabel->setText(category);
        ui->m_categoryLabel->setVisible(true);
    }

    if (!description.isEmpty()) {
        ui->m_descLabel->setText(description);
    } else {
        ui->m_descLabel->setText("暂无描述");
        ui->m_descLabel->setStyleSheet("font-size: 14px; color: #bdc3c7; background: transparent;");
    }

    if (!imagesStr.isEmpty()) {
        QStringList images = imagesStr.split(",", Qt::SkipEmptyParts);
        if (!images.isEmpty()) {
            QPixmap pix = loadPixmap(images[0]);
            if (!pix.isNull()) {
                ui->m_imageLabel->setPixmap(pix.scaled(400, 300, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                ui->m_imageLabel->setStyleSheet("background-color: #ecf0f1;");
            }
        }
    }

    QSqlQuery sq(db);
    sq.prepare("SELECT username FROM users WHERE id = ?");
    sq.addBindValue(m_sellerId);
    if (sq.exec() && sq.next()) {
        m_sellerName = sq.value(0).toString();
        ui->m_sellerLabel->setText(QString("👤 卖家：%1").arg(m_sellerName));
    }

    QSqlQuery uq(db);
    uq.prepare("SELECT username FROM users WHERE id = ?");
    uq.addBindValue(m_currentUserId);
    if (uq.exec() && uq.next())
        m_myUsername = uq.value(0).toString();

    QSqlQuery aq(db);
    aq.prepare("SELECT address, shipping_address FROM user_profiles WHERE user_id = ?");
    aq.addBindValue(m_sellerId);
    if (aq.exec() && aq.next()) {
        QString address = aq.value(0).toString();
        QString shippingAddr = aq.value(1).toString();
        QString displayAddr = !shippingAddr.isEmpty() ? shippingAddr : address;
        if (!displayAddr.isEmpty()) {
            ui->m_addressLabel->setText(QString("📍 地址：%1").arg(displayAddr));
        } else {
            ui->m_addressLabel->setText("📍 地址：未设置");
        }
    } else {
        ui->m_addressLabel->setText("📍 地址：未设置");
    }

    if (m_sellerId != m_currentUserId) {
        ui->m_bottomBar->setVisible(true);
    }
}

void ProductDetailDialog::onChatClicked()
{
    if (m_sellerId <= 0 || m_sellerName.isEmpty())
        return;

    ChatDialog dlg(m_currentUserId, m_myUsername,
                   m_sellerId, m_sellerName, this);
    dlg.exec();
}

void ProductDetailDialog::onBuyClicked()
{
    if (m_sellerId <= 0 || m_sellerName.isEmpty()) return;

    if (m_sellerId == m_currentUserId) {
        QMessageBox::information(this, "提示", "不能购买自己的商品");
        return;
    }

    bool ok;
    double offerPrice = QInputDialog::getDouble(this, "出价",
        QString("商品原价：¥%1\n\n请输入您的报价：").arg(m_productPrice, 0, 'f', 2),
        m_productPrice, 0.01, 9999999.99, 2, &ok);
    if (!ok) return;

    if (DbManager::instance().isConnected()) {
        QSqlDatabase db = DbManager::instance().database();
        QSqlQuery q(db);
        q.prepare("INSERT INTO orders (product_id, buyer_id, seller_id, status, created_at, offer_price) "
                  "VALUES (?, ?, ?, 0, NOW(), ?)");
        q.addBindValue(m_productId);
        q.addBindValue(m_currentUserId);
        q.addBindValue(m_sellerId);
        q.addBindValue(offerPrice);
        if (!q.exec()) {
            QMessageBox::warning(this, "错误", "创建订单失败：" + q.lastError().text());
            return;
        }
    }

    QString offerMsg = QString("买家对【%1】报价 ¥%2（原价 ¥%3），请确认是否同意此价格")
                           .arg(m_productTitle)
                           .arg(offerPrice, 0, 'f', 2)
                           .arg(m_productPrice, 0, 'f', 2);

    ChatDialog dlg(m_currentUserId, m_myUsername,
                   m_sellerId, m_sellerName, this, offerMsg);
    dlg.exec();
}
