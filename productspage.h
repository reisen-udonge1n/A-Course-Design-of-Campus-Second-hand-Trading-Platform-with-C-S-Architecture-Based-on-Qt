#ifndef PRODUCTSPAGE_H
#define PRODUCTSPAGE_H

#include <QWidget>
#include <QGridLayout>
#include <QLabel>
#include <QEvent>

namespace Ui { class ProductsPage; }

class ProductsPage : public QWidget
{
    Q_OBJECT

public:
    explicit ProductsPage(int userId, const QString &username, QWidget *parent = nullptr);
    ~ProductsPage();

public slots:
    void refreshProducts();

private:
    void setupUI();
    void loadProducts();
    QWidget *createProductCard(int id, const QString &title, double price,
                               const QString &category, const QStringList &images);
    bool eventFilter(QObject *obj, QEvent *event) override;

    int m_userId;
    QString m_username;
    QGridLayout *m_gridLayout;
    QLabel *m_emptyLabel;

    Ui::ProductsPage *ui;
};

#endif // PRODUCTSPAGE_H
