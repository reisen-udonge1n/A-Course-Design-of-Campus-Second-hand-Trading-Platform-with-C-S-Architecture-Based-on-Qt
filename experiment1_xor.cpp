/** 
  * 实验一：XOR 图像加解密 
  * 编译：g++ experiment1.cpp -o exp1 `pkg-config --cflags --libs opencv4` 
  * 运行：./exp1 
  */ 

#include <opencv2/opencv.hpp> 
#include <iostream> 

int main() { 
    // 1. 读取彩色图像（请将路径替换为您的图片） 
    cv::Mat img = cv::imread("input.jpg"); 
    if (img.empty()) { 
        std::cerr << "无法读取图像 input.jpg" << std::endl; 
        return -1; 
    } 

    // 2. 生成与图像同尺寸的随机密钥图像（每个通道 0~255） 
    cv::Mat key(img.size(), img.type()); 
    cv::randu(key, 0, 256);   // 随机生成像素值 

    // 3. XOR 加密 
    cv::Mat encrypted; 
    cv::bitwise_xor(img, key, encrypted); 

    // 4. XOR 解密（使用相同密钥） 
    cv::Mat decrypted; 
    cv::bitwise_xor(encrypted, key, decrypted); 

    // 5. 验证可逆性：计算解密图与原图的差异 
    cv::Mat diff; 
    cv::absdiff(img, decrypted, diff); 
    double diffSum = cv::sum(diff)[0] + cv::sum(diff)[1] + cv::sum(diff)[2]; 
    std::cout << "原图与解密图差异像素值和: " << diffSum << " (应为0)" << std::endl; 

    // 6. 显示结果（若没有图形界面，可改为保存图片） 
    cv::imshow("原始图像", img); 
    cv::imshow("加密图像", encrypted); 
    cv::imshow("解密还原图像", decrypted); 
    cv::waitKey(0); 
    cv::destroyAllWindows(); 

    // 可选：保存图片 
    cv::imwrite("encrypted.jpg", encrypted); 
    cv::imwrite("decrypted.jpg", decrypted); 
    return 0; 
}