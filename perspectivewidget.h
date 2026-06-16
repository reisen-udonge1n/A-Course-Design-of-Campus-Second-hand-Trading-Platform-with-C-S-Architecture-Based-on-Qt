#ifndef PERSPECTIVEWIDGET_H
#define PERSPECTIVEWIDGET_H

#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QMessageBox>
#include <QCloseEvent>
#include <vector>
#include <opencv2/opencv.hpp>

using namespace cv;

class PerspectiveWidget : public QWidget
{
    Q_OBJECT

public:
    explicit PerspectiveWidget(QWidget *parent = nullptr);
    ~PerspectiveWidget();

protected:
    void closeEvent(QCloseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private slots:
    void onResetClicked();
    void onConfirmClicked();

private:
    void performPerspectiveTransform();

    std::vector<Point2f> m_srcPoints;
    Mat m_img;
    Mat m_corrected;
    QLabel *m_imageLabel;
    QPushButton *m_resetBtn;
    QPushButton *m_confirmBtn;
    bool m_transformDone;
};

#endif // PERSPECTIVEWIDGET_H
