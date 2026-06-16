#include "notificationpopup.h"
#include "ui_notificationpopup.h"
#include <QFont>

NotificationPopup::NotificationPopup(int fromUserId, const QString &fromUsername,
                                     const QString &content, QWidget *parent)
    : QWidget(parent)
    , m_fromUserId(fromUserId)
    , ui(new Ui::NotificationPopup)
{
    ui->setupUi(this);

    setFixedSize(320, 80);

    ui->m_nameLabel->setText(fromUsername);
    ui->m_contentLabel->setText(content);

    if (parentWidget()) {
        int x = (parentWidget()->width() - width()) / 2;
        move(x, 60);
    }

    raise();

    QTimer::singleShot(5000, this, &QWidget::hide);
    show();
}

NotificationPopup::~NotificationPopup()
{
    delete ui;
}

void NotificationPopup::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        emit clicked(m_fromUserId);
        hide();
    }
    QWidget::mousePressEvent(event);
}
