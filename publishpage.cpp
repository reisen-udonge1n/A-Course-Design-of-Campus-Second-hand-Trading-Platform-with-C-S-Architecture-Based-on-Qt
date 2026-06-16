#include "publishpage.h"
#include "ui_publishpage.h"
#include "dbmanager.h"
#include "util.h"
#include "filter.h"
#include "config.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QFrame>
#include <QFont>
#include <QFileDialog>
#include <QFile>
#include <QFileInfo>
#include <QPixmap>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QEventLoop>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>

PublishPage::PublishPage(int userId, const QString &username, QWidget *parent)
    : QWidget(parent)
    , m_userId(userId)
    , m_username(username)
    , m_editingProductId(-1)
    , ui(new Ui::PublishPage)
{
    ui->setupUi(this);

    m_listLayout = new QVBoxLayout();
    setupListPage();
    setupFormPage();

    refreshList();
}

PublishPage::~PublishPage()
{
    delete ui;
}

void PublishPage::setupListPage()
{
    ui->m_listPage->setStyleSheet("background-color: #f5f6fa;");

    // Find the scroll area in list page and set up the list container
    auto *listScroll = ui->m_listPage->findChild<QScrollArea*>("m_listScrollArea");
    if (listScroll) {
        auto *listContainer = new QWidget();
        listContainer->setStyleSheet("background: transparent;");
        m_listLayout->setContentsMargins(0, 0, 0, 0);
        m_listLayout->setSpacing(10);
        listContainer->setLayout(m_listLayout);
        listScroll->setWidget(listContainer);
    }

    auto *newBtn = ui->m_listPage->findChild<QPushButton*>("m_newBtn");
    if (newBtn) {
        connect(newBtn, &QPushButton::clicked, this, &PublishPage::onShowForm);
    }
}

void PublishPage::setupFormPage()
{
    m_imageGrid = new QGridLayout(ui->m_imageGridContainer);
    m_imageGrid->setContentsMargins(0, 0, 0, 0);
    m_imageGrid->setSpacing(8);
    refreshImageGrid();

    auto *backBtn = ui->m_formPage->findChild<QPushButton*>("m_backBtn");
    if (backBtn) {
        connect(backBtn, &QPushButton::clicked, this, [this]() {
            clearForm();
            refreshList();
            ui->m_stack->setCurrentIndex(0);
        });
    }

    connect(ui->m_submitBtn, &QPushButton::clicked, this, &PublishPage::onSubmit);
}

void PublishPage::refreshList()
{
    QLayoutItem *item;
    while ((item = m_listLayout->takeAt(0)) != nullptr) {
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }

    if (!DbManager::instance().isConnected()) return;

    QSqlDatabase db = DbManager::instance().database();
    QSqlQuery q(db);
    q.prepare("SELECT id, title, price, category, images, status "
              "FROM products WHERE seller_id = ? ORDER BY created_at DESC");
    q.addBindValue(m_userId);

    if (!q.exec()) return;

    bool hasData = false;

    while (q.next()) {
        hasData = true;
        int id = q.value(0).toInt();
        QString title = q.value(1).toString();
        double price = q.value(2).toDouble();
        QString category = q.value(3).toString();
        QString imagesStr = q.value(4).toString();
        int status = q.value(5).toInt();

        QStringList images;
        if (!imagesStr.isEmpty())
            images = imagesStr.split(",", Qt::SkipEmptyParts);

        auto *card = new QFrame();
        card->setStyleSheet("QFrame { background-color: #ffffff; border-radius: 10px; }");
        auto *cardLayout = new QHBoxLayout(card);
        cardLayout->setContentsMargins(12, 12, 12, 12);
        cardLayout->setSpacing(12);

        auto *thumb = new QLabel();
        thumb->setFixedSize(66, 66);
        thumb->setAlignment(Qt::AlignCenter);
        thumb->setStyleSheet("background-color: #ecf0f1; border-radius: 8px;");

        if (!images.isEmpty()) {
            QPixmap pix = loadPixmap(images[0]);
            if (!pix.isNull())
                thumb->setPixmap(pix.scaled(62, 62, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            else
                thumb->setText("📷");
        } else {
            thumb->setText("📷");
        }
        cardLayout->addWidget(thumb);

        auto *infoLayout = new QVBoxLayout();
        infoLayout->setSpacing(4);

        auto *titleLbl = new QLabel(title);
        QFont tf;
        tf.setPointSize(14);
        tf.setBold(true);
        titleLbl->setFont(tf);
        titleLbl->setStyleSheet("color: #2c3e50; background: transparent;");
        infoLayout->addWidget(titleLbl);

        auto *priceLbl = new QLabel(QString("¥ %1").arg(price, 0, 'f', 2));
        priceLbl->setStyleSheet("color: #e74c3c; font-size: 13px; font-weight: bold; background: transparent;");
        infoLayout->addWidget(priceLbl);

        auto *meta = new QLabel(category.isEmpty() ? QStringLiteral("未分类") : category);
        meta->setStyleSheet("color: #7f8c8d; font-size: 11px; background: transparent;");
        infoLayout->addWidget(meta);

        cardLayout->addLayout(infoLayout, 1);

        if (status > 0) {
            auto *statusLbl = new QLabel("已下架");
            statusLbl->setStyleSheet(
                "color: white; background-color: #bdc3c7; border-radius: 4px;"
                "padding: 2px 8px; font-size: 11px;"
            );
            cardLayout->addWidget(statusLbl, 0, Qt::AlignTop);
        }

        auto *btnLayout = new QVBoxLayout();
        btnLayout->setSpacing(6);

        auto *editBtn = new QPushButton("编辑");
        editBtn->setCursor(Qt::PointingHandCursor);
        editBtn->setFixedSize(52, 28);
        editBtn->setStyleSheet(
            "QPushButton { background-color: #3498db; color: white; border: none;"
            "  border-radius: 5px; font-size: 12px; font-weight: bold; }"
            "QPushButton:hover { background-color: #2980b9; }"
        );
        btnLayout->addWidget(editBtn);

        auto *delBtn = new QPushButton("删除");
        delBtn->setCursor(Qt::PointingHandCursor);
        delBtn->setFixedSize(52, 28);
        delBtn->setStyleSheet(
            "QPushButton { background-color: #e74c3c; color: white; border: none;"
            "  border-radius: 5px; font-size: 12px; font-weight: bold; }"
            "QPushButton:hover { background-color: #c0392b; }"
        );
        btnLayout->addWidget(delBtn);

        cardLayout->addLayout(btnLayout);

        m_listLayout->addWidget(card);
        connect(editBtn, &QPushButton::clicked, this, [this, id]() { onEditProduct(id); });
        connect(delBtn, &QPushButton::clicked, this, [this, id]() { onDeleteProduct(id); });
    }

    if (!hasData) {
        auto *empty = new QLabel("还没有发布过商品\n\n点击上方按钮发布第一件商品吧");
        empty->setAlignment(Qt::AlignCenter);
        empty->setStyleSheet("color: #bdc3c7; font-size: 15px; padding: 80px 0; background: transparent;");
        m_listLayout->addWidget(empty);
    }

    m_listLayout->addStretch();
}

void PublishPage::onShowForm()
{
    clearForm();
    ui->m_stack->setCurrentIndex(1);
}

void PublishPage::onEditProduct(int productId)
{
    if (!DbManager::instance().isConnected()) return;

    QSqlDatabase db = DbManager::instance().database();
    QSqlQuery q(db);
    q.prepare("SELECT title, price, category, description, images FROM products WHERE id = ? AND seller_id = ?");
    q.addBindValue(productId);
    q.addBindValue(m_userId);

    if (!q.exec() || !q.next()) return;

    m_editingProductId = productId;
    ui->m_titleEdit->setText(q.value(0).toString());
    ui->m_priceEdit->setText(QString::number(q.value(1).toDouble(), 'f', 2));
    ui->m_categoryCombo->setCurrentText(q.value(2).toString());
    ui->m_descEdit->setPlainText(q.value(3).toString());

    QString imagesStr = q.value(4).toString();
    m_imagePaths.clear();
    if (!imagesStr.isEmpty())
        m_imagePaths = imagesStr.split(",", Qt::SkipEmptyParts);
    refreshImageGrid();

    ui->m_submitBtn->setText("保存修改");
    ui->m_stack->setCurrentIndex(1);
}

void PublishPage::onDeleteProduct(int productId)
{
    if (QMessageBox::question(this, "确认删除", "确定要删除该商品吗？删除后不可恢复。",
                              QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
        return;

    if (!DbManager::instance().isConnected()) return;

    QSqlDatabase db = DbManager::instance().database();
    QSqlQuery q(db);
    q.prepare("DELETE FROM products WHERE id = ? AND seller_id = ?");
    q.addBindValue(productId);
    q.addBindValue(m_userId);

    if (q.exec()) {
        refreshList();
        emit publishSuccess();
    } else {
        QMessageBox::warning(this, "错误", "删除失败：" + q.lastError().text());
    }
}

void PublishPage::refreshImageGrid()
{
    QLayoutItem *item;
    while ((item = m_imageGrid->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }

    int cols = 3, idx = 0;
    for (int i = 0; i < m_imagePaths.size(); ++i) {
        int r = i / cols, c = i % cols;
        auto *cell = new QFrame();
        cell->setFixedSize(90, 90);
        cell->setStyleSheet("QFrame { background-color: #ecf0f1; border-radius: 8px; }");
        auto *cl = new QVBoxLayout(cell);
        cl->setContentsMargins(0, 0, 0, 0);
        cl->setAlignment(Qt::AlignCenter);
        QPixmap pix = loadPixmap(m_imagePaths[i]);
        auto *il = new QLabel();
        il->setAlignment(Qt::AlignCenter);
        il->setPixmap(pix.scaled(86, 86, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        cl->addWidget(il);

        auto *del = new QPushButton("×");
        del->setFixedSize(20, 20);
        del->setCursor(Qt::PointingHandCursor);
        del->setStyleSheet(
            "QPushButton { background-color: rgba(231,76,60,0.8); color: white;"
            "  border: none; border-radius: 10px; font-size: 14px; font-weight: bold; }"
            "QPushButton:hover { background-color: #e74c3c; }"
        );

        auto *wrap = new QWidget();
        wrap->setStyleSheet("background: transparent;");
        auto *wl = new QVBoxLayout(wrap);
        wl->setContentsMargins(0, 0, 0, 0);
        wl->setSpacing(0);
        wl->addWidget(cell, 0, Qt::AlignCenter);
        auto *dc = new QWidget();
        dc->setStyleSheet("background: transparent;");
        auto *dl = new QHBoxLayout(dc);
        dl->setContentsMargins(0, 0, 0, 0);
        dl->addStretch();
        dl->addWidget(del);
        wl->addWidget(dc, 0, Qt::AlignRight | Qt::AlignTop);

        m_imageGrid->addWidget(wrap, r, c, Qt::AlignCenter);
        connect(del, &QPushButton::clicked, this, [this, i]() { onRemoveImage(i); });
        idx = i + 1;
    }

    if (m_imagePaths.size() < 9) {
        int r = idx / cols, c = idx % cols;
        auto *add = new QPushButton("+");
        add->setFixedSize(90, 90);
        add->setCursor(Qt::PointingHandCursor);
        add->setStyleSheet(
            "QPushButton { background-color: #ecf0f1; border: 2px dashed #bdc3c7;"
            "  border-radius: 8px; font-size: 36px; color: #95a5a6; }"
            "QPushButton:hover { background-color: #dfe6e9; border-color: #95a5a6; }"
        );
        m_imageGrid->addWidget(add, r, c, Qt::AlignCenter);
        connect(add, &QPushButton::clicked, this, &PublishPage::onAddImages);
    }
}

void PublishPage::onAddImages()
{
    QStringList files = QFileDialog::getOpenFileNames(
        this, "选择图片", QString(), "图片文件 (*.png *.jpg *.jpeg *.bmp *.gif)");
    if (files.isEmpty()) return;

    int remaining = 9 - m_imagePaths.size();
    for (int i = 0; i < files.size() && i < remaining; ++i)
        m_imagePaths.append(files[i]);

    refreshImageGrid();
}

void PublishPage::onRemoveImage(int index)
{
    if (index >= 0 && index < m_imagePaths.size()) {
        m_imagePaths.removeAt(index);
        refreshImageGrid();
    }
}

void PublishPage::onSubmit()
{
    ui->m_errorLabel->setVisible(false);

    QString title = ui->m_titleEdit->text().trimmed();
    QString priceStr = ui->m_priceEdit->text().trimmed();
    QString category = ui->m_categoryCombo->currentText();
    QString description = ui->m_descEdit->toPlainText().trimmed();

    if (title.isEmpty()) {
        ui->m_errorLabel->setText("请输入商品标题");
        ui->m_errorLabel->setVisible(true);
        ui->m_titleEdit->setFocus();
        return;
    }

    if (Filter::instance().containsSensitiveWords(title)) {
        ui->m_errorLabel->setText("商品标题包含敏感词汇，请修改后重试");
        ui->m_errorLabel->setVisible(true);
        ui->m_titleEdit->setFocus();
        return;
    }

    if (Filter::instance().containsSensitiveWords(description)) {
        ui->m_errorLabel->setText("商品描述包含敏感词汇，请修改后重试");
        ui->m_errorLabel->setVisible(true);
        ui->m_descEdit->setFocus();
        return;
    }

    if (priceStr.isEmpty()) {
        ui->m_errorLabel->setText("请输入商品价格");
        ui->m_errorLabel->setVisible(true);
        ui->m_priceEdit->setFocus();
        return;
    }

    bool priceOk;
    double price = priceStr.toDouble(&priceOk);
    if (!priceOk || price <= 0) {
        ui->m_errorLabel->setText("请输入有效价格（大于0的数字）");
        ui->m_errorLabel->setVisible(true);
        ui->m_priceEdit->setFocus();
        return;
    }

    if (!DbManager::instance().isConnected()) {
        ui->m_errorLabel->setText("数据库连接失败");
        ui->m_errorLabel->setVisible(true);
        return;
    }

    QSqlDatabase db = DbManager::instance().database();

    QStringList imageUrls;
    QNetworkAccessManager netMgr;

    for (const QString &path : m_imagePaths) {
        if (path.startsWith("http://") || path.startsWith("https://")) {
            imageUrls.append(path);
            continue;
        }

        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            qDebug() << "无法打开图片:" << path;
            continue;
        }
        QByteArray data = file.readAll();
        file.close();

        QString filename = QFileInfo(path).fileName();
        QUrl uploadUrl(QString("http://%1:%2/upload?filename=%3")
                           .arg(IMAGE_SERVER_HOST)
                           .arg(IMAGE_SERVER_PORT)
                           .arg(filename));

        QNetworkRequest req(uploadUrl);
        req.setHeader(QNetworkRequest::ContentTypeHeader, "application/octet-stream");
        req.setTransferTimeout(30000);

        QNetworkReply *reply = netMgr.post(req, data);
        QEventLoop loop;
        QTimer timeout;
        timeout.setSingleShot(true);
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
        timeout.start(30000);
        loop.exec();

        if (!timeout.isActive()) {
            qDebug() << "上传图片超时:" << path;
            ui->m_errorLabel->setText("图片上传超时，请检查服务器连接");
            ui->m_errorLabel->setVisible(true);
            reply->deleteLater();
            return;
        }
        timeout.stop();

        if (reply->error() != QNetworkReply::NoError) {
            qDebug() << "上传图片失败:" << reply->errorString();
            ui->m_errorLabel->setText("图片上传失败：" + reply->errorString());
            ui->m_errorLabel->setVisible(true);
            reply->deleteLater();
            return;
        }

        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        QString url = doc.object().value("url").toString();
        if (!url.isEmpty())
            imageUrls.append(url);
        reply->deleteLater();
    }

    QString imagesStr = imageUrls.join(",");
    QSqlQuery q(db);

    if (m_editingProductId > 0) {
        q.prepare("UPDATE products SET title=?, price=?, description=?, images=?, category=? "
                  "WHERE id=? AND seller_id=?");
        q.addBindValue(title);
        q.addBindValue(price);
        q.addBindValue(description);
        q.addBindValue(imagesStr);
        q.addBindValue(category);
        q.addBindValue(m_editingProductId);
        q.addBindValue(m_userId);

        if (q.exec()) {
            QMessageBox::information(this, "成功", "商品修改成功！");
            clearForm();
            refreshList();
            emit publishSuccess();
            ui->m_stack->setCurrentIndex(0);
        } else {
            ui->m_errorLabel->setText("修改失败：" + q.lastError().text());
            ui->m_errorLabel->setVisible(true);
        }
    } else {
        q.prepare("INSERT INTO products (title, price, description, images, category, seller_id) "
                  "VALUES (?, ?, ?, ?, ?, ?)");
        q.addBindValue(title);
        q.addBindValue(price);
        q.addBindValue(description);
        q.addBindValue(imagesStr);
        q.addBindValue(category);
        q.addBindValue(m_userId);

        if (q.exec()) {
            QMessageBox::information(this, "成功", "商品发布成功！");
            clearForm();
            refreshList();
            emit publishSuccess();
            ui->m_stack->setCurrentIndex(0);
        } else {
            ui->m_errorLabel->setText("发布失败：" + q.lastError().text());
            ui->m_errorLabel->setVisible(true);
        }
    }
}

void PublishPage::clearForm()
{
    m_editingProductId = -1;
    ui->m_titleEdit->clear();
    ui->m_priceEdit->clear();
    ui->m_descEdit->clear();
    m_imagePaths.clear();
    ui->m_categoryCombo->setCurrentIndex(0);
    ui->m_submitBtn->setText("发 布");
    ui->m_errorLabel->setVisible(false);
    refreshImageGrid();
}
