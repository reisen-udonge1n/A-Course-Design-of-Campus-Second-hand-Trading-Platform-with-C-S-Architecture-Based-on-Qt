#ifndef HOMEPAGE_H
#define HOMEPAGE_H

#include <QWidget>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QLabel>
#include <QPushButton>
#include <QGridLayout>

namespace Ui { class HomePage; }

class HomePage : public QWidget
{
    Q_OBJECT

public:
    explicit HomePage(int userId, const QString &username, QWidget *parent = nullptr);
    ~HomePage();

protected:
    void showEvent(QShowEvent *event) override;

signals:
    void productClicked(int productId);

public slots:
    void refreshStats();
    void refreshHotProducts();

private slots:
    void onFollowBtnClicked();

private:
    void setupUI();
    QWidget *createProductCard(int id, const QString &title, double price,
                               const QString &category, const QStringList &images);
    void loadStats();
    void loadMoreProducts();
    bool eventFilter(QObject *obj, QEvent *event) override;

    int m_userId;
    QString m_username;
    QString m_currentCategory = QStringLiteral("全部");

    QLabel *m_publishedCount;
    QLabel *m_purchasedCount;
    QLabel *m_followingCount;

    QScrollArea *m_categoryScroll;
    QList<QPushButton*> m_categoryButtons;
    QWidget *m_gridWidget;
    QGridLayout *m_gridLayout;
    QScrollArea *m_mainScrollArea = nullptr;

    static constexpr int PAGE_SIZE = 8;
    int m_offset = 0;
    bool m_hasMore = true;
    bool m_loading = false;
    bool m_loaded = false;

    Ui::HomePage *ui;
};

#endif // HOMEPAGE_H
