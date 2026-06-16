#ifndef PUBLISHPAGE_H
#define PUBLISHPAGE_H

#include <QWidget>
#include <QLineEdit>
#include <QTextEdit>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include <QStringList>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QStackedWidget>

namespace Ui { class PublishPage; }

class PublishPage : public QWidget
{
    Q_OBJECT

public:
    explicit PublishPage(int userId, const QString &username, QWidget *parent = nullptr);
    ~PublishPage();

signals:
    void publishSuccess();

public slots:
    void refreshList();

private slots:
    void onAddImages();
    void onRemoveImage(int index);
    void onSubmit();
    void onShowForm();
    void onEditProduct(int productId);
    void onDeleteProduct(int productId);

private:
    void setupListPage();
    void setupFormPage();
    void refreshImageGrid();
    void clearForm();

    int m_userId;
    QString m_username;

    QStringList m_imagePaths;
    QGridLayout *m_imageGrid;
    QVBoxLayout *m_listLayout;
    int m_editingProductId;

    Ui::PublishPage *ui;
};

#endif // PUBLISHPAGE_H
