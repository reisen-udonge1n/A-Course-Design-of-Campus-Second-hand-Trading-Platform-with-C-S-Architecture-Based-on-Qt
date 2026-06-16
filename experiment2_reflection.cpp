#include <opencv2/opencv.hpp>
#include <vector>

using namespace cv;
using namespace std;

vector<Point2f> srcPoints;
Mat img;
string winName = "Select 4 corners (TL -> TR -> BR -> BL)";
bool selectionDone = false;

void onMouse(int event, int x, int y, int, void*) {
    if (selectionDone) return;
    if (event == EVENT_LBUTTONDOWN && srcPoints.size() < 4) {
        srcPoints.push_back(Point2f(x, y));
        circle(img, Point(x, y), 8, Scalar(0, 0, 255), -1);
        putText(img, to_string(srcPoints.size()), Point(x + 10, y - 5),
                FONT_HERSHEY_SIMPLEX, 1, Scalar(0, 255, 0), 2);
        imshow(winName, img);
    }
}

void closeCallback(int key, void*) {
    if (key == 27) {
        int ret = MessageBox(NULL, "确定要关闭透视变换窗口吗？", "提示",
                           MB_YESNO | MB_ICONQUESTION);
        if (ret == IDYES) {
            selectionDone = true;
        }
    }
}

int main() {
    img = imread("C:\\Users\\34751\\Desktop\\origin.png");
    if (img.empty()) {
        cerr << "无法读取图像 origin.png" << endl;
        return -1;
    }

    namedWindow(winName, WINDOW_NORMAL);
    resizeWindow(winName, 800, 600);
    setMouseCallback(winName, onMouse, &selectionDone);
    createButton("关闭", closeCallback, &selectionDone, QT_PUSH_BUTTON | QT_WHEN_CHANGED);
    
    imshow(winName, img);
    waitKey(0);
    
    if (srcPoints.size() != 4) {
        destroyAllWindows();
        return -1;
    }
    destroyWindow(winName);

    int dstWidth = 600, dstHeight = 400;
    vector<Point2f> dstPoints = {
        Point2f(0, 0),
        Point2f(dstWidth - 1, 0),
        Point2f(dstWidth - 1, dstHeight - 1),
        Point2f(0, dstHeight - 1)
    };

    Mat M = getPerspectiveTransform(srcPoints, dstPoints);
    Mat corrected;
    warpPerspective(img, corrected, M, Size(dstWidth, dstHeight));

    namedWindow("Original Image", WINDOW_NORMAL);
    resizeWindow("Original Image", 600, 450);
    imshow("Original Image", img);

    namedWindow("Corrected Result", WINDOW_NORMAL);
    resizeWindow("Corrected Result", 600, 400);
    imshow("Corrected Result", corrected);

    waitKey(0);
    return 0;
}
