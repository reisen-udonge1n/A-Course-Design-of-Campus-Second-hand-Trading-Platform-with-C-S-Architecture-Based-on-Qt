#ifndef NOTIFICATION_POPUP_H
#define NOTIFICATION_POPUP_H

#include <QWidget>
#include <QLabel>
#include <QTimer>
#include <QMouseEvent>

namespace Ui { class NotificationPopup; }

class NotificationPopup : public QWidget
{
    Q_OBJECT

public:
    explicit NotificationPopup(int fromUserId, const QString &fromUsername,
                               const QString &content, QWidget *parent = nullptr);
    ~NotificationPopup();

signals:
    void clicked(int userId);

protected:
    void mousePressEvent(QMouseEvent *event) override;

private:
    int m_fromUserId;
    Ui::NotificationPopup *ui;
};

#endif // NOTIFICATION_POPUP_H
