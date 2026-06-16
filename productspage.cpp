#include "productspage.h"
#include "ui_productspage.h"
#include "productdetaildialog.h"
#include "dbmanager.h"
#include "util.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QFrame>
#include <QFont>
#include <QPixmap>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>

ProductsPage::ProductsPage(int userId, const QString &username, QWidget *parent)
    : QWidget(parent)
    , m_userId(userId)
    , m_username(username)
    , ui(new Ui::ProductsPage)
{
    ui->setupUi(this);
    setupUI();
}

ProductsPage::~ProductsPage()
{
    delete ui;
}

void ProductsPage::setupUI()
{
    m_emptyLabel = findChild<QLabel*>("m_emptyLabel");

    auto *gridWidget = findChild<QWidget*>("m_gridWidget");
    m_gridLayout = new QGridLayout(gridWidget);
    m_gridLayout->setContentsMargins(0, 0, 0, 0);
    m_gridLayout->setSpacing(12);

    loadProducts();
}

void ProductsPage::loadProducts()
{
    QLayoutItem *item;
    while ((item = m_gridLayout->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }

    if (!DbManager::instance().isConnected()) return;

    QSqlDatabase db = DbManager::instance().database();
    QSqlQuery q(db);
    q.prepare("SELECT id, title, price, category, images, seller_id FROM products "
              "WHERE status = 0 AND id IN ("
              "  SELECT MAX(id) FROM products WHERE status = 0 GROUP BY title"
              ") ORDER BY created_at DESC");

    if (!q.exec()) {
        qDebug() << "查询商品失败:" << q.lastError().text();
        return;
    }

    int col = 0, row = 0;
    bool hasData = false;

    while (q.next()) {
        hasData = true;
        int id = q.value(0).toInt();
        QString title = q.value(1).toString();
        double price = q.value(2).toDouble();
        QString category = q.value(3).toString();
        QString imagesStr = q.value(4).toString();
        int sellerId = q.value(5).toInt();

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

    m_emptyLabel->setVisible(!hasData);
}

void ProductsPage::refreshProducts()
{
    loadProducts();
}

QWidget *ProductsPage::createProductCard(int id, const QString &title, double price,
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

    auto *titleLbl = new QLabel(title);
    titleLbl->setStyleSheet("font-size: 14px; font-weight: bold; color: #2c3e50; background: transparent;");
    titleLbl->setContentsMargins(12, 0, 12, 0);
    cardLayout->addWidget(titleLbl);

    auto *priceLbl = new QLabel(QString("¥ %1").arg(price, 0, 'f', 2));
    priceLbl->setStyleSheet("font-size: 16px; font-weight: bold; color: #e74c3c; background: transparent;");
    priceLbl->setContentsMargins(12, 0, 12, 0);
    cardLayout->addWidget(priceLbl);

    return card;
}

bool ProductsPage::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonRelease) {
        QWidget *w = qobject_cast<QWidget*>(obj);
        if (w) {
            int id = w->property("productId").toInt();
            if (id > 0) {
                ProductDetailDialog dlg(id, m_userId, this);
                dlg.exec();
                return true;
            }
        }
    }
    return QWidget::eventFilter(obj, event);
}
