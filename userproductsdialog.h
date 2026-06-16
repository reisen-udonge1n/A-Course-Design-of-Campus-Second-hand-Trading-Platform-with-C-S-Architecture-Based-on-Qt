#ifndef USERPRODUCTSDIALOG_H
#define USERPRODUCTSDIALOG_H

#include <QDialog>
#include <QGridLayout>
#include <QLabel>

namespace Ui { class UserProductsDialog; }

class UserProductsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit UserProductsDialog(int userId, const QString &username, int currentUserId, QWidget *parent = nullptr);
    ~UserProductsDialog();

private:
    void loadProducts();
    QWidget *createProductCard(int id, const QString &title, double price,
                               const QString &category, const QStringList &images);
    bool eventFilter(QObject *obj, QEvent *event) override;

    int m_targetUserId;
    QString m_targetUsername;
    int m_currentUserId;
    QGridLayout *m_gridLayout;

    Ui::UserProductsDialog *ui;
};

#endif // USERPRODUCTSDIALOG_H
