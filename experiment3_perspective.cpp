#include <opencv2/opencv.hpp> 
#include <iostream> 
#include <vector> 

using namespace cv; 
using namespace std; 

vector<Point2f> srcPoints;   // 存储用户点击的四个点 
Mat image; 
string winName = "Select 4 corners (top-left, top-right, bottom-right, bottom-left)"; 

void onMouse(int event, int x, int y, int, void*) { 
    if (event == EVENT_LBUTTONDOWN && srcPoints.size() < 4) { 
        srcPoints.push_back(Point2f(x, y)); 
        circle(image, Point(x, y), 5, Scalar(0, 0, 255), -1); 
        putText(image, to_string(srcPoints.size()), Point(x + 5, y - 5), 
            FONT_HERSHEY_SIMPLEX, 0.5, Scalar(0, 255, 0), 2); 
        imshow(winName, image); 
        if (srcPoints.size() == 4) { 
            cout << "四点已选完，正在矫正..." << endl; 
            destroyWindow(winName); 
        } 
    } 
} 

int main() { 
    // 读取原始图像
    Mat original = imread("C:\\Users\\34751\\Desktop\\origin.png"); 
    if (original.empty()) { 
        cerr << "无法读取图像 origin.png" << endl; 
        return -1; 
    } 

    // 缩小图片到0.4倍
    double scale = 0.4;
    resize(original, image, Size(), scale, scale); 
    cout << "原始尺寸: " << original.cols << "x" << original.rows << endl;
    cout << "缩小后尺寸: " << image.cols << "x" << image.rows << endl;

    namedWindow(winName); 
    setMouseCallback(winName, onMouse); 
    imshow(winName, image); 
    waitKey(0); 

    if (srcPoints.size() != 4) { 
        cerr << "未正确选取4个点" << endl; 
        return -1; 
    } 

    // 目标矩形的四个顶点（按缩放比例缩小）
    int width = 150, height = 100; 
    vector<Point2f> dstPoints = { 
        Point2f(0, 0), 
        Point2f(width - 1, 0), 
        Point2f(width - 1, height - 1), 
        Point2f(0, height - 1) 
    }; 

    Mat M = getPerspectiveTransform(srcPoints, dstPoints); 
    Mat corrected; 
    warpPerspective(image, corrected, M, Size(width, height)); 

    // 显示结果（缩小后的原始图和矫正结果）
    Mat displayOriginal, displayCorrected;
    resize(image, displayOriginal, Size(), 0.4, 0.4);
    resize(corrected, displayCorrected, Size(), 0.4, 0.4);
    
    imshow("Original (scaled)", displayOriginal); 
    imshow("Corrected (scaled)", displayCorrected); 
    imwrite("corrected_plate.jpg", corrected); 
    waitKey(0); 
    return 0; 
}