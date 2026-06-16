#include "userproductsdialog.h"
#include "ui_userproductsdialog.h"
#include "productdetaildialog.h"
#include "dbmanager.h"
#include "util.h"

#include <QScrollArea>
#include <QFrame>
#include <QFont>
#include <QPixmap>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>

UserProductsDialog::UserProductsDialog(int userId, const QString &username, int currentUserId, QWidget *parent)
    : QDialog(parent)
    , m_targetUserId(userId)
    , m_targetUsername(username)
    , m_currentUserId(currentUserId)
    , ui(new Ui::UserProductsDialog)
{
    ui->setupUi(this);

    setWindowTitle(QString("%1 的商品").arg(m_targetUsername));
    setFixedSize(400, 550);

    ui->m_headerLabel->setText(QString("👤 %1 的商品").arg(m_targetUsername));

    m_gridLayout = new QGridLayout(ui->m_gridWidget);
    m_gridLayout->setContentsMargins(0, 0, 0, 0);
    m_gridLayout->setSpacing(12);

    loadProducts();
}

UserProductsDialog::~UserProductsDialog()
{
    delete ui;
}

void UserProductsDialog::loadProducts()
{
    QLayoutItem *item;
    while ((item = m_gridLayout->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }

    if (!DbManager::instance().isConnected()) return;

    QSqlDatabase db = DbManager::instance().database();
    QSqlQuery q(db);

    q.prepare("SELECT id, title, price, category, images FROM products "
              "WHERE seller_id = ? AND status = 0 ORDER BY created_at DESC");
    q.addBindValue(m_targetUserId);

    if (!q.exec()) {
        qDebug() << "查询用户商品失败:" << q.lastError().text();
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

    ui->m_emptyLabel->setVisible(!hasData);
}

QWidget *UserProductsDialog::createProductCard(int id, const QString &title, double price,
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

bool UserProductsDialog::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonRelease) {
        QWidget *w = qobject_cast<QWidget*>(obj);
        if (w) {
            int id = w->property("productId").toInt();
            if (id > 0) {
                ProductDetailDialog dlg(id, m_currentUserId, this);
                dlg.exec();
                return true;
            }
        }
    }
    return QDialog::eventFilter(obj, event);
}
