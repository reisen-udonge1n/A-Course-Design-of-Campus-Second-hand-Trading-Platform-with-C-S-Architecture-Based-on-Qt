#ifndef SEARCHPAGE_H
#define SEARCHPAGE_H

#include <QWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QGridLayout>
#include <QLabel>
#include <QScrollArea>
#include <QEvent>

namespace Ui { class SearchPage; }

class SearchPage : public QWidget
{
    Q_OBJECT

public:
    explicit SearchPage(int userId, const QString &username, QWidget *parent = nullptr);
    ~SearchPage();

private slots:
    void onSearch();

private:
    void setupUI();
    void searchProducts(const QString &keyword);
    QWidget *createProductCard(int id, const QString &title, double price,
                               const QString &category, const QStringList &images);
    bool eventFilter(QObject *obj, QEvent *event) override;

    int m_userId;
    QString m_username;
    QGridLayout *m_gridLayout;
    QLabel *m_emptyLabel;
    QLabel *m_resultInfo;

    Ui::SearchPage *ui;
};

#endif // SEARCHPAGE_H
