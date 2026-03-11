#include <opencv2/opencv.hpp>
#include <iostream>
#include <pic.h>
//暂时用于测试

int main()
{
    // 1. 读取图片
    std::string image_path = R"(F:\Gemini_Generated_Image_x6jjkcx6jjkcx6jj.png)";
    cv::Mat image = cv::imread(image_path, cv::IMREAD_COLOR);

    // 检查图片是否读取成功
    if (image.empty())
    {
        std::cout << "无法读取图片，请检查路径: " << image_path << std::endl;
        return -1;
    }

	//图像预处理：灰度化 → 降噪 → 二值化
    
    cv::Mat processed_image = ImgPrase::preprocessImg(image);
    // 2. 显示图片
    cv::imshow("显示窗口", processed_image);
    std::cout << "press 's' to save picture,press others to exit..." << std::endl;

    // 3. 等待按键
    int key = cv::waitKey(0); // 等待无限长时间

    // 4. 如果按下 's' 键，保存图片
    if (key == 's')
    {
        cv::imwrite("saved_image.png", image);
        std::cout << "picture saved in saved_image.png" << std::endl;
    }
    else
    {
        std::cout << "exit." << std::endl;
    }

    // 5. 销毁所有窗口
    cv::destroyAllWindows();

    return 0;
}