#ifndef PRODUCTDETAILDIALOG_H
#define PRODUCTDETAILDIALOG_H

#include <QDialog>

namespace Ui { class ProductDetailDialog; }

class ProductDetailDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ProductDetailDialog(int productId, int currentUserId, QWidget *parent = nullptr);
    ~ProductDetailDialog();

private:
    void setupUI();
    void loadData();

    int m_productId;
    int m_currentUserId;

    void onChatClicked();
    void onBuyClicked();

    int m_sellerId;
    QString m_sellerName;
    QString m_myUsername;
    QString m_productTitle;
    double m_productPrice;

    Ui::ProductDetailDialog *ui;
};

#endif // PRODUCTDETAILDIALOG_H
