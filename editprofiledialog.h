#ifndef EDITPROFILEDIALOG_H
#define EDITPROFILEDIALOG_H

#include <QDialog>

namespace Ui { class EditProfileDialog; }

class EditProfileDialog : public QDialog
{
    Q_OBJECT

public:
    explicit EditProfileDialog(int userId, QWidget *parent = nullptr);
    ~EditProfileDialog();

private slots:
    void onSave();

private:
    bool loadProfile();
    bool saveProfile();

    int m_userId;
    Ui::EditProfileDialog *ui;
};

#endif // EDITPROFILEDIALOG_H
