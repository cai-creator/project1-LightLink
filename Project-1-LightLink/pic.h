#pragma once
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

	namespace helpFunction {
		struct ParseInfo;	// 解析信息结构体
		Point2f CalRectCenter(const vector<Point>& pointSet); // 计算轮廓中心点
		float distance(const Point2f& a, const Point2f& b);
		float Cal3PointAngle(const Point& point0, const Point& point1, const Point& point2);//计算三个点的角度
		bool isRightAngle(float angle);//判断是否为直角
		bool IsQrBWRateLegal(const float rate);//判断黑白比例是否合法
		bool BWRatePreprocessing(Mat& image, vector<int>& vValueCount);//黑白条纹预处理
		bool IsQrBWRateXLegal(Mat& image);//检查X方向的黑白比例
		bool IsQrBWRate(Mat& image); //检查二维码的黑白比例
		bool IsQrSizeLegal(const Size2f& qrSize, const Size2f& imgSize);//判断二维码尺寸是否合法
		Mat CropRect(const Mat& srcImg, const RotatedRect& rotatedRect);//裁剪旋转矩形区域
		bool IsQrPoint(const vector<Point>& contour, const Mat& img);//判断是否为二维码定位点
		bool isRightAngleExist(const Point& point0, const Point& point1, const Point& point2);//判断三个点是否存在直角
		double Cal3NumVariance(const int a, const int b, const int c);//计算三个数的方差
		bool IsClockwise(const Point& basePoint, const Point& point1, const Point& point2);//判断顺逆时针
		Point CalFourthPoint(const Point& point0, const Point& point1, const Point& point2);//计算第四个点
		pair<float, float> CalExtendVec(const Point2f& point0, const Point2f& point1, const Point2f& point2, float bias);//计算扩展向量
	}
	// 图像预处理：灰度化 → 降噪 → 二值化
	Mat preprocessImg(const Mat& srcImg, float blurRate = 0.001);

	// 轮廓检测和定位点筛选
	bool findPositionPoints(const Mat& binaryImg, vector<vector<Point>>& qrPoint);

	//调整定位点顺序，计算第四个点
	vector<Point2f> adjustPositionPoints(const vector<Point2f>& positionPoints);

	//裁剪平行四边形区域并透视变换为正方形
	Mat cropParallelRect(const Mat& srcImg, const vector<Point2f>& srcPoints);

	//主定位函数
	bool Main(const Mat& srcImg, Mat& dstImg);
}