#include "perspectivewidget.h"
#include <QApplication>
#include <QPainter>
#include <QImage>
#include <QPixmap>

PerspectiveWidget::PerspectiveWidget(QWidget *parent)
    : QWidget(parent)
    , m_transformDone(false)
{
    setWindowTitle("透视变换 - 选择4个角点");
    resize(900, 700);

    m_img = imread("C:\\Users\\34751\\Desktop\\origin.png");
    if (m_img.empty()) {
        QMessageBox::critical(this, "错误", "无法读取图像 origin.png");
        return;
    }

    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    m_imageLabel = new QLabel(this);
    m_imageLabel->setAlignment(Qt::AlignCenter);
    m_imageLabel->setMinimumSize(800, 500);
    m_imageLabel->setStyleSheet("border: 1px solid #ccc; background: white;");
    mainLayout->addWidget(m_imageLabel);

    QLabel *hintLabel = new QLabel("请按顺序点击四个角点: 左上 → 右上 → 右下 → 左下", this);
    hintLabel->setAlignment(Qt::AlignCenter);
    hintLabel->setStyleSheet("color: #666; font-size: 14px;");
    mainLayout->addWidget(hintLabel);

    QHBoxLayout *btnLayout = new QHBoxLayout();
    m_resetBtn = new QPushButton("重置", this);
    m_confirmBtn = new QPushButton("确认变换", this);
    btnLayout->addStretch();
    btnLayout->addWidget(m_resetBtn);
    btnLayout->addWidget(m_confirmBtn);
    mainLayout->addLayout(btnLayout);

    connect(m_resetBtn, &QPushButton::clicked, this, &PerspectiveWidget::onResetClicked);
    connect(m_confirmBtn, &QPushButton::clicked, this, &PerspectiveWidget::onConfirmClicked);

    update();
}

PerspectiveWidget::~PerspectiveWidget()
{
}

void PerspectiveWidget::closeEvent(QCloseEvent *event)
{
    int ret = QMessageBox::question(this, "提示", "确定要关闭透视变换窗口吗？",
                                    QMessageBox::Yes | QMessageBox::No);

    if (ret == QMessageBox::Yes) {
        event->accept();
    } else {
        event->ignore();
    }
}

void PerspectiveWidget::mousePressEvent(QMouseEvent *event)
{
    if (m_transformDone) return;
    if (m_srcPoints.size() >= 4) return;

    QPoint pos = m_imageLabel->mapFrom(this, event->pos());
    QRect labelRect = m_imageLabel->rect();

    if (!labelRect.contains(pos)) return;

    double scaleX = (double)m_img.cols / m_imageLabel->width();
    double scaleY = (double)m_img.rows / m_imageLabel->height();

    int x = static_cast<int>(pos.x() * scaleX);
    int y = static_cast<int>(pos.y() * scaleY);

    if (x >= 0 && x < m_img.cols && y >= 0 && y < m_img.rows) {
        m_srcPoints.push_back(Point2f(x, y));
        update();
    }
}

void PerspectiveWidget::paintEvent(QPaintEvent *event)
{
    QWidget::paintEvent(event);

    if (m_img.empty()) return;

    Mat displayImg = m_img.clone();

    for (size_t i = 0; i < m_srcPoints.size(); i++) {
        circle(displayImg, m_srcPoints[i], 8, Scalar(0, 0, 255), -1);
        putText(displayImg, to_string(i + 1),
                Point(m_srcPoints[i].x + 10, m_srcPoints[i].y - 5),
                FONT_HERSHEY_SIMPLEX, 1, Scalar(0, 255, 0), 2);
    }

    if (m_srcPoints.size() >= 4) {
        for (int i = 0; i < 4; i++) {
            line(displayImg, m_srcPoints[i], m_srcPoints[(i + 1) % 4],
                 Scalar(255, 0, 0), 2);
        }
    }

    QImage qimg = QImage(displayImg.data, displayImg.cols, displayImg.rows,
                         displayImg.step, QImage::Format_RGB888).rgbSwapped();
    m_imageLabel->setPixmap(QPixmap::fromImage(qimg));
}

void PerspectiveWidget::onResetClicked()
{
    m_srcPoints.clear();
    m_transformDone = false;
    update();
}

void PerspectiveWidget::onConfirmClicked()
{
    if (m_srcPoints.size() != 4) {
        QMessageBox::warning(this, "提示", "请先选择4个角点！");
        return;
    }

    performPerspectiveTransform();
    m_transformDone = true;
    update();

    QMessageBox::information(this, "完成", "透视变换完成！");
}

void PerspectiveWidget::performPerspectiveTransform()
{
    int dstWidth = 600, dstHeight = 400;
    vector<Point2f> dstPoints = {
        Point2f(0, 0),
        Point2f(dstWidth - 1, 0),
        Point2f(dstWidth - 1, dstHeight - 1),
        Point2f(0, dstHeight - 1)
    };

    Mat M = getPerspectiveTransform(m_srcPoints, dstPoints);
    warpPerspective(m_img, m_corrected, M, Size(dstWidth, dstHeight));

    imshow("矫正结果", m_corrected);
    waitKey(0);
    destroyWindow("矫正结果");
}
