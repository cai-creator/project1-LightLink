#include <opencv2/opencv.hpp>
#include <iostream>
#include <pic.h>
#include <Windows.h>

int main()
{
	// 设置控制台输出编码为UTF-8
	SetConsoleOutputCP(CP_UTF8);

	std::string image_path = R"(E:\01850.jpg)";
	cv::Mat image = cv::imread(image_path, cv::IMREAD_COLOR);

	if (image.empty())
	{
		std::cout << "无法读取图片: " << image_path << std::endl;
		return -1;
	}

	cv::Mat processed_image = ImgPrase::preprocessImg(image);
	cv::imshow("二值化", processed_image);

	cv::Mat qr_result;
	if (ImgPrase::Main(image, qr_result)) {
		cv::imshow("结果:", qr_result);
		std::cout << "成功!" << std::endl;
		std::cout << "按 's' 保存结果" << std::endl;

		int key = cv::waitKey(0);

		if (key == 's') {
			cv::imwrite("qr_result.png", qr_result);
			std::cout << "结果已保存到 qr_result.png" << std::endl;
		}
	} else {
		std::cout << "默认！" << std::endl;
		std::cout << "按任意键退出..." << std::endl;
		cv::waitKey(0);
	}

	cv::destroyAllWindows();

	return 0;
}