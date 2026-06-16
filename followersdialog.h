#ifndef FOLLOWERS_DIALOG_H
#define FOLLOWERS_DIALOG_H

#include <QDialog>

namespace Ui { class FollowersDialog; }

class FollowersDialog : public QDialog
{
    Q_OBJECT

public:
    enum TabType {
        Following,
        Discover
    };

    explicit FollowersDialog(int userId, const QString &username, QWidget *parent = nullptr);
    ~FollowersDialog();

signals:
    void followChanged();
    void viewUserProducts(int userId, const QString &username);

private slots:
    void onFollowUser();
    void onUnfollowUser();
    void refreshList();
    void onViewUserProducts();

private:
    void loadUsers();
    void loadFollowers();

    int m_userId;
    QString m_username;
    TabType m_currentTab = Following;

    Ui::FollowersDialog *ui;
};

#endif // FOLLOWERS_DIALOG_H
