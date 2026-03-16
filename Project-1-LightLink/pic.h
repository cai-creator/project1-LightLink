#pragma once
#include<cstdio>
#include<numeric>
#include<opencv2/opencv.hpp>

/**
 * @brief 图像预处理与二维码定位命名空间
 * @details 该命名空间包含图像预处理、二维码定位点检测、透视变换等功能
 */
namespace ImgPraseV2 {
	using namespace cv;
	using namespace std;

	/**
	 * @brief 辅助函数命名空间
	 * @details 包含二维码定位相关的辅助函数和数据结构
	 */
	namespace helpFunction {
		// 前向声明
		struct ParseInfo;

		/**
		 * @brief 计算轮廓的中心点
		 * @param pointSet 轮廓点集
		 * @return 中心点坐标
		 */
		Point2f CalRectCenter(const vector<Point>& pointSet);

		/**
		 * @brief 计算轮廓的指定角点
		 * @param pointSet 轮廓点集
		 * @param cornerIndex 角点索引(0-3)
		 * @return 指定角点坐标
		 */
		Point2f CalRectCorner(const vector<Point>& pointSet, int cornerIndex);

		/**
		 * @brief 计算两点之间的欧几里得距离
		 * @param a 第一个点
		 * @param b 第二个点
		 * @return 两点之间的距离
		 */
		float distance(const Point2f& a, const Point2f& b);

		/**
		 * @brief 计算三个点之间的夹角
		 * @param point0 顶点
		 * @param point1 端点1
		 * @param point2 端点2
		 * @return 夹角角度(度)
		 */
		float Cal3PointAngle(const Point& point0, const Point& point1, const Point& point2);

		/**
		 * @brief 判断角度是否为直角
		 * @param angle 角度值(度)
		 * @return 是否在直角范围内(75°-105°)
		 */
		bool isRightAngle(float angle);

		/**
		 * @brief 判断黑白比例是否合法
		 * @param rate 黑白比例
		 * @return 是否在合法范围内(0.40-2.25)
		 */
		bool IsQrBWRateLegal(const float rate);

		/**
		 * @brief 黑白条纹预处理函数
		 * @param image 二值化图像
		 * @param vValueCount 存储各条纹像素数的向量
		 * @return 条纹数量是否足够(>=5)
		 */
		bool BWRatePreprocessing(Mat& image, vector<int>& vValueCount);

		/**
		 * @brief 检查X方向的黑白比例是否合法
		 * @param image 裁剪后的图像
		 * @return 是否符合1:1:3:1:1比例
		 */
		bool IsQrBWRateXLegal(Mat& image);

		/**
		 * @brief 检查二维码定位点的黑白比例(横纵双向)
		 * @param image 裁剪后的图像
		 * @return X和Y方向比例是否都合法
		 */
		bool IsQrBWRate(Mat& image);

		/**
		 * @brief 判断二维码定位点尺寸是否合法
		 * @param qrSize 定位点尺寸
		 * @param imgSize 原始图像尺寸
		 * @return 尺寸是否符合要求
		 */
		bool IsQrSizeLegal(const Size2f& qrSize, const Size2f& imgSize);

		/**
		 * @brief 从图像中裁剪旋转矩形区域
		 * @param srcImg 源图像
		 * @param rotatedRect 旋转矩形
		 * @return 裁剪后的图像
		 */
		Mat CropRect(const Mat& srcImg, const RotatedRect& rotatedRect);

		/**
		 * @brief 判断轮廓是否为二维码定位点
		 * @param contour 轮廓点集
		 * @param img 二值化图像
		 * @return 是否为有效的二维码定位点
		 */
		bool IsQrPoint(const vector<Point>& contour, const Mat& img);

		/**
		 * @brief 判断三个点是否存在直角
		 * @param point0 第一个点
		 * @param point1 第二个点
		 * @param point2 第三个点
		 * @return 是否存在接近90度的角
		 */
		bool isRightAngleExist(const Point& point0, const Point& point1, const Point& point2);

		/**
		 * @brief 计算三个数的方差
		 * @param a 第一个数
		 * @param b 第二个数
		 * @param c 第三个数
		 * @return 方差值
		 */
		double Cal3NumVariance(const int a, const int b, const int c);

		/**
		 * @brief 判断三点顺序是否为顺时针
		 * @param basePoint 基准点
		 * @param point1 第一个点
		 * @param point2 第二个点
		 * @return 是否为顺时针方向
		 */
		bool IsClockwise(const Point& basePoint, const Point& point1, const Point& point2);

		/**
		 * @brief 根据三个点计算第四个点(平行四边形)
		 * @param point0 第一个点
		 * @param point1 第二个点
		 * @param point2 第三个点
		 * @return 推算出的第四个点
		 */
		Point CalFourthPoint(const Point& point0, const Point& point1, const Point& point2);

		/**
		 * @brief 计算扩展向量(用于定位点校正)
		 * @param point0 顶点
		 * @param point1 端点1
		 * @param point2 端点2
		 * @param bias 扩展长度
		 * @return 扩展向量(x,y)
		 */
		pair<float, float> CalExtendVec(const Point2f& point0, const Point2f& point1, const Point2f& point2, float bias);
	}

	/**
	 * @brief 使用OTSU算法的图像预处理
	 * @param srcImg 源彩色图像
	 * @param blurRate 模糊率(默认0.0005)
	 * @return 二值化图像
	 * @details 处理流程：灰度化 -> 高斯模糊 -> OTSU二值化
	 */
	Mat preprocessImgV2_OTSU(const Mat& srcImg, float blurRate = 0.0005);

	/**
	 * @brief 使用自适应阈值的图像预处理
	 * @param srcImg 源彩色图像
	 * @param blockSize 邻域大小(默认11)
	 * @param C 常数(默认2.0)
	 * @return 二值化图像
	 * @details 处理流程：灰度化 -> 高斯模糊 -> 自适应阈值二值化
	 */
	Mat preprocessImgV2_Adaptive(const Mat& srcImg, int blockSize = 11, double C = 2.0);

	/**
	 * @brief 使用形态学操作的图像预处理
	 * @param srcImg 源二值化图像
	 * @return 处理后的图像
	 * @details 使用闭运算和开运算去除噪声
	 */
	Mat preprocessImgV2_Morphology(const Mat& srcImg);

	/**
	 * @brief 组合多种预处理方法
	 * @param srcImg 源彩色图像
	 * @return 组合后的二值化图像
	 * @details 综合OTSU和自适应阈值结果进行与运算
	 */
	Mat preprocessImgV2_Combined(const Mat& srcImg);

	/**
	 * @brief 从二值化图像中查找二维码定位点
	 * @param binaryImg 二值化图像
	 * @param qrPoint 输出的定位点轮廓向量
	 * @return 是否找到足够的定位点(>=3)
	 */
	bool findPositionPoints(const Mat& binaryImg, vector<vector<Point>>& qrPoint);

	/**
	 * @brief 调整定位点顺序并计算第四个点
	 * @param positionPoints 定位点中心坐标
	 * @param positionContours 定位点轮廓
	 * @param precise 是否精确模式
	 * @return 调整后的四个角点(左上、右上、右下、左下)
	 */
	vector<Point2f> adjustPositionPoints(const vector<Point2f>& positionPoints, const vector<vector<Point>>& positionContours, bool precise = false);

	/**
	 * @brief 透视变换：四边形区域转换为矩形
	 * @param srcImg 源图像
	 * @param srcPoints 四边形四个角点
	 * @return 变换后的矩形图像
	 */
	Mat cropParallelRect(const Mat& srcImg, const vector<Point2f>& srcPoints);

	/**
	 * @brief 二维码定位主函数
	 * @param srcImg 源彩色图像
	 * @param dstImg 输出的二维码图像(108x108)
	 * @param debugPath 调试图像保存路径(可选)
	 * @return 是否成功定位
	 */
	bool Main(const Mat& srcImg, Mat& dstImg, const string& debugPath = "");
}
