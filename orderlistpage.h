#ifndef ORDERLISTPAGE_H
#define ORDERLISTPAGE_H

#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>

namespace Ui { class OrderListPage; }

class OrderListPage : public QWidget
{
    Q_OBJECT

public:
    enum Mode { Published, Purchased, PendingShip, PendingReceipt, PendingConfirm, Received };

    explicit OrderListPage(int userId, Mode mode, QWidget *parent = nullptr);
    ~OrderListPage();

public slots:
    void refreshData();

signals:
    void goBack();

private:
    void setupUI();
    void loadData();
    QString modeTitle() const;
    QString modeQuery() const;

    int m_userId;
    Mode m_mode;
    QVBoxLayout *m_listLayout;

    Ui::OrderListPage *ui;
};

#endif // ORDERLISTPAGE_H
