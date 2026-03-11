#include<cstdio>
#include<opencv2/opencv.hpp>

// ==================== 图像预处理和二维码定位模块 ====================
// 功能：
// 1. 图像预处理：灰度化 → 高斯模糊 → 二值化
// 2. 轮廓检测：找到所有可能的定位点轮廓
// 3. 定位点筛选：根据黑白比例、大小等特征筛选出3个大定位点
// 4. 第四点计算：根据3个定位点计算出第4个点（透视变换用）
// 5. 透视变换：将斜的二维码矫正为正的108×108图像

namespace ImgPrase {
	using namespace cv;
	using namespace std;

	Mat preprocessImg(const Mat& srcImg,float blurRate = 0.0005);
}