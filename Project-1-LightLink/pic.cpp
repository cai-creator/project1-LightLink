#include "pic.h"

// 保存预处理图像的宏
#define SAVE_PREPROCESS_IMAGES 1

/**
 * @brief 图像预处理与二维码定位命名空间
 * @details 该命名空间包含图像预处理、二维码定位点检测、透视变换等功能
 */
namespace ImgPraseV2 {

	// ==================== 常量定义 ====================
	// 直角角度范围(度)
	constexpr float MinRightAngle = 75.0;
	constexpr float MaxRightAngle = 105.0;
	// 黑白比例范围(理想值为1.0)
	constexpr float MaxQRBWRate = 2.25;
	constexpr float MinQRBWRate = 0.40;
	// 最小定位点尺寸(像素)
	constexpr int MinQRSize = 10;
	// 定位点占图像的最大比例
	constexpr float MaxQRScale = 0.25;
	// 长宽比范围
	constexpr float MinQRXYRate = 5.0 / 6.0;
	constexpr float MaxQRXYRate = 6.0 / 5.0;
	// 二维码类型比例
	constexpr float MaxQrTypeRate = 2.2;
	constexpr float minQrTypeRate = 1.8;
	// 距离比例范围
	constexpr float MaxDistanceRate = 1.1;
	constexpr float minDistanceRate = 0.9;

	/**
	 * @brief 将图像旋转90度
	 * @param srcImg 源图像
	 * @return 旋转后的图像
	 * @details 通过转置和水平翻转实现90度顺时针旋转
	 */
	Mat Rotation_90(const Mat& srcImg) {
		Mat tempImg;
		transpose(srcImg, tempImg);
		flip(tempImg, tempImg, 1);
		return tempImg;
	}

	/**
	 * @brief 辅助函数命名空间
	 * @details 包含二维码定位相关的辅助函数和数据结构
	 */
	namespace helpFunction {

		/**
		 * @brief 定位点信息结构体
		 * @details 存储二维码定位点的中心、尺寸和最小包围矩形信息
		 */
		struct ParseInfo {
			Point2f Center;      // 定位点中心坐标
			int size;            // 定位点轮廓大小
			RotatedRect Rect;    // 定位点的最小包围矩形

			/**
			 * @brief 构造函数
			 * @param pointSet 轮廓点集
			 */
			ParseInfo(const vector<Point>& pointSet) :Center(CalRectCenter(pointSet)), size(pointSet.size()), Rect(minAreaRect(pointSet)) {}
			ParseInfo() = default;
		};

		/**
		 * @brief 计算轮廓的中心点
		 * @param pointSet 轮廓点集
		 * @return 中心点坐标
		 * @details 通过取轮廓上四个等分点的平均值估算中心
		 */
		Point2f CalRectCenter(const vector<Point>& pointSet) {
			return Point2f((pointSet[pointSet.size() / 4].x + pointSet[pointSet.size() * 2 / 4].x + pointSet[pointSet.size() * 3 / 4].x + pointSet[pointSet.size() - 1].x) / 4.0f,
				(pointSet[pointSet.size() / 4].y + pointSet[pointSet.size() * 2 / 4].y + pointSet[pointSet.size() * 3 / 4].y + pointSet[pointSet.size() - 1].y) / 4.0f);
		}

		/**
		 * @brief 计算轮廓的指定角点
		 * @param pointSet 轮廓点集
		 * @param cornerIndex 角点索引(0-3)
		 * @return 指定角点坐标
		 */
		Point2f CalRectCorner(const vector<Point>& pointSet, int cornerIndex) {
			RotatedRect rect = minAreaRect(pointSet);
			Point2f corners[4];
			rect.points(corners);
			return corners[cornerIndex];
		}

		/**
		 * @brief 在定位点附近查找指定方向的黑色像素角点
		 * @param binaryImg 二值化图像
		 * @param contour 轮廓（最外层黑色环的轮廓）
		 * @param direction 方向：0=左上, 1=右上, 2=右下, 3=左下
		 * @return 找到的黑色像素角点
		 */
		Point2f FindPreciseCornerPixel(const Mat& binaryImg, const vector<Point>& contour, int direction) {
			// 图像边界检查
			int imgWidth = binaryImg.cols;
			int imgHeight = binaryImg.rows;

			Point bestPixel;
			float bestScore = -1e10f;

			// 首先从轮廓本身的点中找最佳角点
			for (const auto& pt : contour) {
				int x = pt.x;
				int y = pt.y;

				// 检查边界
				if (x < 0 || x >= imgWidth || y < 0 || y >= imgHeight) continue;

				// 必须是黑色像素
				if (binaryImg.at<uchar>(y, x) != 0) continue;

				float score = 0;
				switch (direction) {
					case 0: // 左上：x和y越小越好
						score = -x - y;
						break;
					case 1: // 右上：x越大，y越小越好
						score = x - y;
						break;
					case 2: // 右下：x和y越大越好
						score = x + y;
						break;
					case 3: // 左下：x越小，y越大越好
						score = -x + y;
						break;
				}

				if (score > bestScore) {
					bestScore = score;
					bestPixel = pt;
				}
			}

			// 如果在轮廓中找到了，直接返回
			if (bestScore > -1e9f) {
				return Point2f((float)bestPixel.x, (float)bestPixel.y);
			}

			// 备用方案：如果轮廓中没找到，用minAreaRect的角点
			RotatedRect rect = minAreaRect(contour);
			Point2f rectCorners[4];
			rect.points(rectCorners);

			Point2f fallbackCorner = rectCorners[0];
			for (int i = 1; i < 4; i++) {
				float scoreCurrent = 0, scoreFallback = 0;
				switch (direction) {
					case 0: 
						scoreCurrent = -rectCorners[i].x - rectCorners[i].y;
						scoreFallback = -fallbackCorner.x - fallbackCorner.y;
						break;
					case 1: 
						scoreCurrent = rectCorners[i].x - rectCorners[i].y;
						scoreFallback = fallbackCorner.x - fallbackCorner.y;
						break;
					case 2: 
						scoreCurrent = rectCorners[i].x + rectCorners[i].y;
						scoreFallback = fallbackCorner.x + fallbackCorner.y;
						break;
					case 3: 
						scoreCurrent = -rectCorners[i].x + rectCorners[i].y;
						scoreFallback = -fallbackCorner.x + fallbackCorner.y;
						break;
				}
				if (scoreCurrent > scoreFallback) {
					fallbackCorner = rectCorners[i];
				}
			}

			return fallbackCorner;
		}

		/**
		 * @brief 计算外轮廓角点
		 * @param contour 轮廓
		 * @param allCenters 所有定位点中心
		 * @return 距离中心最远的角点
		 * @details 用于确定定位点的外角(二维码外部的角)
		 */
		Point2f CalOuterCorner(const vector<Point>& contour, const vector<Point2f>& allCenters) {
			RotatedRect rect = minAreaRect(contour);
			Point2f corners[4];
			rect.points(corners);

			Point2f center = rect.center;

			// 计算所有定位点中心的平均值作为参考中心
			int n = min(3, (int)allCenters.size());
			float cx = 0, cy = 0;
			for (int i = 0; i < n; i++) {
				cx += allCenters[i].x;
				cy += allCenters[i].y;
			}
			cx /= n;
			cy /= n;
			Point2f centroid(cx, cy);

			// 找到距离参考中心最远的角点
			float maxDist = 0;
			Point2f outerCorner = corners[0];

			for (int i = 0; i < 4; i++) {
				float d = helpFunction::distance(corners[i], centroid);
				if (d > maxDist) {
					maxDist = d;
					outerCorner = corners[i];
				}
			}

			return outerCorner;
		}

		/**
		 * @brief 计算两点之间的欧几里得距离
		 * @param a 第一个点
		 * @param b 第二个点
		 * @return 两点之间的距离
		 */
		float distance(const Point2f& a, const Point2f& b) {
			return sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y));
		}

		/**
		 * @brief 计算三个点之间的夹角
		 * @param point0 顶点
		 * @param point1 端点1
		 * @param point2 端点2
		 * @return 夹角角度(度)
		 * @details 使用余弦定理计算角度
		 */
		float Cal3PointAngle(const Point& point0, const Point& point1, const Point& point2) {
			float dx1 = point1.x - point0.x, dy1 = point1.y - point0.y;
			float dx2 = point2.x - point0.x, dy2 = point2.y - point0.y;
			return (dx1 * dx2 + dy1 * dy2) / sqrt((dx1 * dx1 + dy1 * dy1) * (dx2 * dx2 + dy2 * dy2) + 1e-10f) * 180.0f / CV_PI;
		}

		/**
		 * @brief 判断角度是否为直角
		 * @param angle 角度值(度)
		 * @return 是否在直角范围内(75°-105°)
		 */
		bool isRightAngle(float angle) {
			return MinRightAngle <= angle && MaxRightAngle >= angle;
		}

		/**
		 * @brief 判断黑白比例是否合法
		 * @param rate 黑白比例
		 * @return 是否在合法范围内(0.40-2.25)
		 */
		bool IsQrBWRateLegal(const float rate) {
			return rate < MaxQRBWRate && rate > MinQRBWRate;
		}

		/**
		 * @brief 黑白条纹预处理函数
		 * @param image 二值化图像
		 * @param vValueCount 存储各条纹像素数的向量
		 * @return 条纹数量是否足够(>=5)
		 * @details 统计图像中心行黑白交替的条纹数量
		 */
		bool BWRatePreprocessing(Mat& image, vector<int>& vValueCount) {
			int count = 0, nc = image.cols * image.channels(), nr = image.rows / 2;
			uchar lastColor = 0, * data = image.ptr<uchar>(nr);
			for (int i = 0; i < nc; i++) {
				uchar color = data[i];
				if (color > 0) color = 255;
				if (i == 0) {
					lastColor = color;
					count++;
				}
				else {
					if (lastColor != color) {
						vValueCount.push_back(count);
						count = 0;
					}
					count++;
					lastColor = color;
				}
			}
			if (count) vValueCount.push_back(count);
			return vValueCount.size() >= 5;
		}

		/**
		 * @brief 检查X方向的黑白比例是否合法
		 * @param image 裁剪后的图像
		 * @return 是否符合1:1:3:1:1比例
		 * @details 二维码定位点具有特征性的黑白比例
		 */
		bool IsQrBWRateXLegal(Mat& image) {
			vector<int> vValueCount;
			if (!BWRatePreprocessing(image, vValueCount)) return false;

			int index = -1, maxCount = -1;
			// 找到最多的块
			for (int i = 0; i < vValueCount.size(); i++) {
				if (i == 0) {
					index = i;
					maxCount = vValueCount[i];
				}
				else if (vValueCount[i] > maxCount) {
					maxCount = vValueCount[i];
					index = i;
				}
			}

			// 1:1:3:1:1，不会出现在最前面和最后面
			if (index < 2 || (vValueCount.size() - index) < 3) return false;

			float rate = (float)maxCount / 3.00;
			bool checkTag = true;
			for (int i = index - 1; i <= index + 1; i++) {
				if (i == index) continue;
				if (!IsQrBWRateLegal((float)vValueCount[i] / rate)) {
					checkTag = false;
					break;
				}
			}
			return checkTag;
		}

		/**
		 * @brief 检查二维码定位点的黑白比例(横纵双向)
		 * @param image 裁剪后的图像
		 * @return X和Y方向比例是否都合法
		 * @details 先检查X方向，然后旋转图像检查Y方向
		 */
		bool IsQrBWRate(Mat& image) {
			bool xTest = IsQrBWRateXLegal(image);
			if (!xTest) return false;

			// 旋转90度检查Y方向
			Mat imageT;
			transpose(image, imageT);
			flip(imageT, imageT, 1);
			bool yTest = IsQrBWRateXLegal(imageT);
			return yTest;
		}

		/**
		 * @brief 判断二维码定位点尺寸是否合法
		 * @param qrSize 定位点尺寸
		 * @param imgSize 原始图像尺寸
		 * @return 尺寸是否符合要求
		 * @details 检查绝对尺寸和相对尺寸
		 */
		bool IsQrSizeLegal(const Size2f& qrSize, const Size2f& imgSize) {
			float xYScale = qrSize.width / qrSize.height;
			if (qrSize.height < MinQRSize || qrSize.width < MinQRSize) return false;
			if (qrSize.width / imgSize.width > MaxQRScale || qrSize.height / imgSize.height > MaxQRScale) return false;
			if (xYScale < MinQRXYRate || xYScale > MaxQRXYRate) return false;
			return true;
		}

		/**
		 * @brief 从图像中裁剪旋转矩形区域
		 * @param image 源图像
		 * @param rect 旋转矩形
		 * @return 裁剪后的图像
		 * @details 使用透视变换进行裁剪
		 */
		Mat CropRect(const Mat& image, const RotatedRect& rect) {
			Mat srcPoints, disImg;
			boxPoints(rect, srcPoints);
			vector<Point2f> disPoints = {
				Point2f(0,rect.size.height - 1),
				Point2f(0,0),
				Point2f(rect.size.width - 1,0),
				Point2f(rect.size.width - 1, rect.size.height - 1) };
			auto M = getPerspectiveTransform(srcPoints, disPoints);
			warpPerspective(image, disImg, M, rect.size);
			return disImg;
		}

		/**
		 * @brief 检查定位点的对称性和中心特征
		 * @param cropImg 裁剪后的定位点图像
		 * @return 是否具有二维码定位点的特征
		 * @details 检查中心黑色、边缘白色、左右上下对称性
		 */
		bool CheckQrSymmetry(const Mat& cropImg) {
			if (cropImg.empty() || cropImg.rows < 10 || cropImg.cols < 10) return false;

			int centerX = cropImg.cols / 2;
			int centerY = cropImg.rows / 2;
			int regionSize = std::min(cropImg.cols, cropImg.rows) / 4;

			// 检查中心区域是否为黑色
			int blackCount = 0;
			int totalPixels = 0;
			for (int y = centerY - regionSize; y < centerY + regionSize; y++) {
				for (int x = centerX - regionSize; x < centerX + regionSize; x++) {
					if (y >= 0 && y < cropImg.rows && x >= 0 && x < cropImg.cols) {
						if (cropImg.at<uchar>(y, x) == 0) blackCount++;
						totalPixels++;
					}
				}
			}

			float blackRatio = (float)blackCount / totalPixels;
			if (blackRatio < 0.6f) return false;

			// 检查水平对称性
			Mat leftHalf = cropImg(Rect(0, 0, centerX, cropImg.rows));
			Mat rightHalf = cropImg(Rect(centerX, 0, centerX, cropImg.rows));
			flip(rightHalf, rightHalf, 1);

			float hSymDiff = 0;
			for (int y = 0; y < cropImg.rows; y++) {
				for (int x = 0; x < centerX; x++) {
					if (leftHalf.at<uchar>(y, x) != rightHalf.at<uchar>(y, x)) {
						hSymDiff++;
					}
				}
			}
			hSymDiff /= (centerX * cropImg.rows);

			// 检查垂直对称性
			Mat topHalf = cropImg(Rect(0, 0, cropImg.cols, centerY));
			Mat bottomHalf = cropImg(Rect(0, centerY, cropImg.cols, centerY));
			flip(bottomHalf, bottomHalf, 0);

			float vSymDiff = 0;
			for (int y = 0; y < centerY; y++) {
				for (int x = 0; x < cropImg.cols; x++) {
					if (topHalf.at<uchar>(y, x) != bottomHalf.at<uchar>(y, x)) {
						vSymDiff++;
					}
				}
			}
			vSymDiff /= (centerY * cropImg.cols);

			// 对称性阈值检查
			if (hSymDiff > 0.35f || vSymDiff > 0.35f) return false;

			// 检查边缘区域是否为白色(定位点外层是白色)
			int edgeBlackCount = 0;
			int edgeTotalPixels = 0;
			int edgeWidth = std::min(cropImg.cols, cropImg.rows) / 12;

			for (int y = 0; y < cropImg.rows; y++) {
				for (int x = 0; x < cropImg.cols; x++) {
					if (x < edgeWidth || x >= cropImg.cols - edgeWidth ||
						y < edgeWidth || y >= cropImg.rows - edgeWidth) {
						if (cropImg.at<uchar>(y, x) == 0) edgeBlackCount++;
						edgeTotalPixels++;
					}
				}
			}

			float edgeBlackRatio = (float)edgeBlackCount / edgeTotalPixels;
			if (edgeBlackRatio > 0.5f) return false;

			// 中心与边缘对比度检查
			if (blackRatio - edgeBlackRatio < 0.3f) return false;

			return true;
		}

		/**
		 * @brief 判断轮廓是否为二维码定位点
		 * @param contour 轮廓点集
		 * @param img 二值化图像
		 * @return 是否为有效的二维码定位点
		 * @details 综合检查尺寸和黑白比例
		 */
		bool IsQrPoint(const vector<Point>& contour, const Mat& img) {
			RotatedRect rotatedRect = minAreaRect(contour);
			Mat cropImg = CropRect(img, rotatedRect);

			// 检查尺寸是否合法
			bool sizeLegal = IsQrSizeLegal(rotatedRect.size, img.size());
			if (!sizeLegal) return false;

			// 检查黑白比例是否合法
			bool bwRateLegal = IsQrBWRate(cropImg);
			return bwRateLegal;
		}

		/**
		 * @brief 判断三个点是否存在直角
		 * @param point0 第一个点
		 * @param point1 第二个点
		 * @param point2 第三个点
		 * @return 是否存在接近90度的角
		 */
		bool isRightAngleExist(const Point& point0, const Point& point1, const Point& point2) {
			float angle0 = Cal3PointAngle(point0, point1, point2);
			float angle1 = Cal3PointAngle(point1, point0, point2);
			float angle2 = Cal3PointAngle(point2, point0, point1);
			return isRightAngle(angle0) || isRightAngle(angle1) || isRightAngle(angle2);
		}

		/**
		 * @brief 计算三个数的方差
		 * @param a 第一个数
		 * @param b 第二个数
		 * @param c 第三个数
		 * @return 方差值
		 */
		double Cal3NumVariance(const int a, const int b, const int c) {
			double avg = (a + b + c) / 3.0;
			return ((a - avg) * (a - avg) + (b - avg) * (b - avg) + (c - avg) * (c - avg)) / 3.0;
		}

		/**
		 * @brief 判断三点顺序是否为顺时针
		 * @param basePoint 基准点
		 * @param point1 第一个点
		 * @param point2 第二个点
		 * @return 是否为顺时针方向
		 * @details 使用叉积判断方向
		 */
		bool IsClockwise(const Point& basePoint, const Point& point1, const Point& point2) {
			float ax = point1.x - basePoint.x, ay = point1.y - basePoint.y;
			float bx = point2.x - basePoint.x, by = point2.y - basePoint.y;
			return (ax * by - ay * bx) > 0;
		}

		/**
		 * @brief 计算点到中心的角度
		 * @param center 中心点
		 * @param p 待计算点
		 * @return 角度值(度)
		 */
		float CalAngle(const Point2f& center, const Point2f& p) {
			return atan2(p.y - center.y, p.x - center.x) * 180.0f / CV_PI;
		}

		/**
		 * @brief 按角度对点进行排序
		 * @param points 点集
		 * @param indices 输出的索引序列
		 */
		void SortPointsByAngle(const vector<Point2f>& points, vector<int>& indices) {
			if (points.size() < 3) {
				indices.resize(points.size());
				iota(indices.begin(), indices.end(), 0);
				return;
			}

			// 计算重心
			float cx = (points[0].x + points[1].x + points[2].x) / 3.0f;
			float cy = (points[0].y + points[1].y + points[2].y) / 3.0f;
			Point2f center(cx, cy);

			// 按角度排序
			vector<pair<float, int>> angleIdx;
			for (int i = 0; i < points.size(); i++) {
				float angle = CalAngle(center, points[i]);
				angleIdx.push_back({ angle, i });
			}
			sort(angleIdx.begin(), angleIdx.end(), [](const auto& a, const auto& b) {
				return a.first < b.first;
				});

			indices.resize(points.size());
			for (int i = 0; i < angleIdx.size(); i++) {
				indices[i] = angleIdx[i].second;
			}
		}

		/**
		 * @brief 根据三个点计算第四个点(平行四边形)
		 * @param poi0 第一个点
		 * @param poi1 第二个点
		 * @param poi2 第三个点
		 * @return 推算出的第四个点
		 * @details 使用向量加法原理: D = B + C - A
		 */
		Point CalForthPoint(const Point& poi0, const Point& poi1, const Point& poi2) {
			return Point(poi2.x + poi1.x - poi0.x, poi2.y + poi1.y - poi0.y);
		}

		/**
		 * @brief 计算扩展向量(用于定位点校正)
		 * @param poi0 顶点
		 * @param poi1 端点1
		 * @param poi2 端点2
		 * @param bias 扩展长度
		 * @return 扩展向量(x,y)
		 */
		pair<float, float> CalExtendVec(const Point2f& poi0, const Point2f& poi1, const Point2f& poi2, float bias) {
			float dis0 = distance(poi0, poi1), dis1 = distance(poi0, poi2);
			float rate = dis1 / dis0;
			float x1 = poi0.x - poi2.x, y1 = poi0.y - poi2.y;
			float x2 = (poi0.x - poi1.x) * rate, y2 = (poi0.y - poi1.y) * rate;
			float totx = x1 + x2, toty = y1 + y2, distot = sqrt(totx * totx + toty * toty);
			return { totx / distot * bias, toty / distot * bias };
		}

	}

	// ==================== 预处理函数 ====================

	/**
     * @brief 使用OTSU算法的图像预处理
     * @param srcImg 源彩色图像
     * @param blurRate 模糊率(默认0.0005)
     * @return 二值化图像
     * @details 处理流程：灰度化 -> 非局部均值去噪 -> 中值滤波 -> OTSU二值化
     */
    Mat preprocessImgV2_OTSU(const Mat& srcImg, float blurRate) {
        Mat tempImg;
        // 1. 灰度化
        cvtColor(srcImg, tempImg, COLOR_BGR2GRAY);

        // 2. 非局部均值去噪，保留边缘
        Mat denoisedImg;
        fastNlMeansDenoising(tempImg, denoisedImg, 30, 7, 21);

        // 3. 中值滤波，去除摩尔纹和椒盐噪声
        medianBlur(denoisedImg, tempImg, 3);

        // 4. 高斯模糊，进一步减少高频干扰
        float BlurSize = 1.0f + srcImg.rows * blurRate;
        if (BlurSize < 1.0f) BlurSize = 1.0f;
        blur(tempImg, tempImg, Size2f(BlurSize, BlurSize));

        // 5. OTSU自动阈值二值化
        threshold(tempImg, tempImg, 0, 255, THRESH_BINARY | THRESH_OTSU);
        return tempImg;
    }

	/**
	 * @brief 使用自适应阈值的图像预处理
	 * @param srcImg 源彩色图像
	 * @param blockSize 邻域大小(默认11，必须为奇数)
	 * @param C 常数(默认2.0)
	 * @return 二值化图像
	 * @details 适用于光照不均匀的图像
	 */
	Mat preprocessImgV2_Adaptive(const Mat& srcImg, int blockSize, double C) {
		Mat grayImg;
		// 1. 灰度化
		cvtColor(srcImg, grayImg, COLOR_BGR2GRAY);

		// 2. 高斯模糊降噪
		GaussianBlur(grayImg, grayImg, Size(5, 5), 0);

		// 3. 自适应阈值二值化
		Mat adaptiveImg;
		adaptiveThreshold(grayImg, adaptiveImg, 255,
			ADAPTIVE_THRESH_GAUSSIAN_C, THRESH_BINARY,
			blockSize, C);

		return adaptiveImg;
	}

	/**
	 * @brief 使用形态学操作的图像预处理
	 * @param srcImg 源二值化图像
	 * @return 处理后的图像
	 * @details 使用闭运算和开运算去除噪声、填充空洞
	 */
	Mat preprocessImgV2_Morphology(const Mat& srcImg) {
		Mat morphImg;
		Mat kernel = getStructuringElement(MORPH_RECT, Size(3, 3));

		// 闭运算：填充小空洞
		morphologyEx(srcImg, morphImg, MORPH_CLOSE, kernel);
		// 开运算：去除小噪声
		morphologyEx(morphImg, morphImg, MORPH_OPEN, kernel);

		return morphImg;
	}

	/**
	 * @brief 组合多种预处理方法
	 * @param srcImg 源彩色图像
	 * @return 组合后的二值化图像
	 * @details 综合OTSU和自适应阈值结果进行与运算，提高鲁棒性
	 */
	Mat preprocessImgV2_Combined(const Mat& srcImg) {
		Mat grayImg;
		cvtColor(srcImg, grayImg, COLOR_BGR2GRAY);

		// 高斯模糊降噪
		GaussianBlur(grayImg, grayImg, Size(3, 3), 0);

		// OTSU阈值
		Mat result1, result2;
		threshold(grayImg, result1, 0, 255, THRESH_BINARY | THRESH_OTSU);

		// 自适应阈值 - 回到原来成功的版本
		adaptiveThreshold(grayImg, result2, 255,
			ADAPTIVE_THRESH_GAUSSIAN_C, THRESH_BINARY,
			15, 5);

		// 形态学闭运算 - 回到原来成功的版本
		Mat kernel = getStructuringElement(MORPH_RECT, Size(3, 3));
		morphologyEx(result1, result1, MORPH_CLOSE, kernel);
		morphologyEx(result2, result2, MORPH_CLOSE, kernel);

		// 与运算组合两种结果
		Mat combined;
		bitwise_and(result1, result2, combined);

		return combined;
	}

	// ==================== 定位点处理函数 ====================

	/**
	 * @brief 验证三个定位点是否构成有效的组合
	 * @param p1 第一个定位点信息
	 * @param p2 第二个定位点信息
	 * @param p3 第三个定位点信息
	 * @return 是否为有效的二维码定位点组合
	 * @details 检查距离比例、角度关系、大小相似性
	 */
	bool IsValidQrTriple(const helpFunction::ParseInfo& p1, const helpFunction::ParseInfo& p2, const helpFunction::ParseInfo& p3) {
		// 计算三个定位点之间的距离
		float d12 = helpFunction::distance(p1.Center, p2.Center);
		float d13 = helpFunction::distance(p1.Center, p3.Center);
		float d23 = helpFunction::distance(p2.Center, p3.Center);

		float maxDist = std::max({ d12, d13, d23 });
		float minDist = std::min({ d12, d13, d23 });

		// 距离范围检查
		if (minDist < 15) return false;
		if (maxDist > 600) return false;

		// 检查两边比例(等腰直角三角形)
		float ratio1, ratio2;
		if (maxDist == d12) {
			ratio1 = d13 / d23;
			ratio2 = d23 / d13;
		}
		else if (maxDist == d13) {
			ratio1 = d12 / d23;
			ratio2 = d23 / d12;
		}
		else {
			ratio1 = d12 / d13;
			ratio2 = d13 / d12;
		}

		if (ratio1 < 0.6f || ratio1 > 1.6f) return false;
		if (ratio2 < 0.6f || ratio2 > 1.6f) return false;

		// 使用余弦定理计算角度
		float a = d12, b = d13, c = d23;
		float angle1 = acos((b * b + c * c - a * a) / (2 * b * c + 1e-10f)) * 180.0f / CV_PI;
		float angle2 = acos((a * a + c * c - b * b) / (2 * a * c + 1e-10f)) * 180.0f / CV_PI;
		float angle3 = acos((a * a + b * b - c * c) / (2 * a * b + 1e-10f)) * 180.0f / CV_PI;

		// 检查是否存在直角
		bool hasRightAngle = (angle1 > 65 && angle1 < 115) ||
			(angle2 > 65 && angle2 < 115) ||
			(angle3 > 65 && angle3 < 115);

		if (!hasRightAngle) return false;

		// 检查大小相似性
		float size1 = p1.Rect.size.width * p1.Rect.size.height;
		float size2 = p2.Rect.size.width * p2.Rect.size.height;
		float size3 = p3.Rect.size.width * p3.Rect.size.height;

		float avgSize = (size1 + size2 + size3) / 3.0f;
		float sizeRatio1 = size1 / avgSize;
		float sizeRatio2 = size2 / avgSize;
		float sizeRatio3 = size3 / avgSize;

		if (sizeRatio1 < 0.4f || sizeRatio1 > 2.5f) return false;
		if (sizeRatio2 < 0.4f || sizeRatio2 > 2.5f) return false;
		if (sizeRatio3 < 0.4f || sizeRatio3 > 2.5f) return false;

		return true;
	}

	/**
	 * @brief 调整定位点顺序
	 * @param src3Points 定位点轮廓集合
	 * @details 将定位点按 左上、右上、左下 顺序排列
	 */
	void AdjustPointsOrder(vector<vector<Point>>& src3Points) {
		vector<vector<Point>> temp;
		Point p3[3] = { helpFunction::CalRectCenter(src3Points[0]),
						helpFunction::CalRectCenter(src3Points[1]),
						helpFunction::CalRectCenter(src3Points[2]) };
		int index[3][3] = { { 0,1,2 },{1,0,2},{2,0,1} };
		for (int i = 0; i < 3; i++) {
			if (helpFunction::isRightAngle(helpFunction::Cal3PointAngle(p3[index[i][0]], p3[index[i][1]], p3[index[i][2]]))) {
				temp.push_back(std::move(src3Points[index[i][0]]));  // 左上角
				if (helpFunction::IsClockwise(p3[index[i][0]], p3[index[i][1]], p3[index[i][2]])) {
					temp.push_back(std::move(src3Points[index[i][1]]));  // 右上角
					temp.push_back(std::move(src3Points[index[i][2]]));  // 左下角
				}
				else {
					temp.push_back(std::move(src3Points[index[i][2]]));  // 右上角
					temp.push_back(std::move(src3Points[index[i][1]]));  // 左下角
				}
				for (int i = 3; i < src3Points.size(); ++i)
					temp.push_back(std::move(src3Points[i]));
				std::swap(temp, src3Points);
				return;
			}
		}
		return;
	}

	/**
	 * @brief 过滤多余的定位点
	 * @param qrPoints 候选定位点集合
	 * @return 是否成功筛选
	 * @details 筛选出面积方差最小且构成直角的三个点
	 */
	bool DumpExcessQrPoint(vector<vector<Point>>& qrPoints) {
		// 如果正好有4个点，尝试找出构成直角组合
		if (qrPoints.size() == 4) {
			vector<vector<Point>> temp = qrPoints;
			// 按面积降序排序
			sort(temp.begin(), temp.end(), [](const vector<Point>& a, const vector<Point>& b) {
				return a.size() > b.size();
				});

			// 前三个点检查
			Point p0 = helpFunction::CalRectCenter(temp[0]);
			Point p1 = helpFunction::CalRectCenter(temp[1]);
			Point p2 = helpFunction::CalRectCenter(temp[2]);

			bool hasRightAngle = helpFunction::isRightAngleExist(p2, p1, p0);

			if (hasRightAngle) {
				qrPoints = { temp[0], temp[1], temp[2] };
				return 0;
			}

			// 尝试其他组合
			int combos[4][3] = {
				{0, 1, 3},
				{0, 2, 3},
				{1, 2, 3}
			};

			for (int c = 0; c < 4; c++) {
				Point cp0 = helpFunction::CalRectCenter(temp[combos[c][0]]);
				Point cp1 = helpFunction::CalRectCenter(temp[combos[c][1]]);
				Point cp2 = helpFunction::CalRectCenter(temp[combos[c][2]]);

				if (helpFunction::isRightAngleExist(cp2, cp1, cp0)) {
					qrPoints = { temp[combos[c][0]], temp[combos[c][1]], temp[combos[c][2]] };
					return 0;
				}
			}

			return 1;
		}

		// 多于4个点时，按面积排序后筛选
		sort(
			qrPoints.begin(), qrPoints.end(),
			[](const vector<Point>& a, const vector<Point>& b) { return a.size() < b.size(); }
		);

		double mindis = INFINITY;
		int pos = -1;
		Point Point0 = helpFunction::CalRectCenter(qrPoints[0]),
			Point1 = helpFunction::CalRectCenter(qrPoints[1]), Point2;

		for (int i = 2; i < qrPoints.size(); ++i) {
			bool tag = 0;
			Point2 = helpFunction::CalRectCenter(qrPoints[i]);
			bool hasRightAngle = helpFunction::isRightAngleExist(Point2, Point1, Point0);
			if (!hasRightAngle)
				tag = 1;
			if (!tag) {
				auto temp = helpFunction::Cal3NumVariance(qrPoints[i].size(), qrPoints[i - 1].size(), qrPoints[i - 2].size());
				if (mindis > temp) {
					mindis = temp;
					pos = i;
				}
			}
			Point0 = Point1;
			Point1 = Point2;
		}

		if (pos == -1) return 1;
		else {
			vector<vector<Point>> temp =
			{
				std::move(qrPoints[pos - 2]),
				std::move(qrPoints[pos - 1]),
				std::move(qrPoints[pos])
			};
			for (int i = 0; i < pos - 2; ++i)
				temp.push_back(std::move(qrPoints[i]));
			for (int i = pos + 1; i < qrPoints.size(); ++i)
				temp.push_back(std::move(qrPoints[i]));
			std::swap(temp, qrPoints);
			return 0;
		}
	}

	/**
	 * @brief 从二值化图像中查找二维码定位点
	 * @param binaryImg 二值化图像
	 * @param qrPoint 输出的定位点轮廓向量
	 * @return 是否找到足够的定位点(>=3)
	 * @details 通过轮廓层级关系筛选定位点
	 */
	bool findPositionPoints(const Mat& binaryImg, vector<vector<Point>>& qrPoints) {
		vector<vector<Point>> contours;
		vector<Vec4i> hierarchy;
		findContours(binaryImg, contours, hierarchy, RETR_TREE, CHAIN_APPROX_NONE, Point(0, 0));

		// 通过黑色定位角作为父轮廓，有两个子轮廓的特点筛选
		int parentIdx = -1;
		int ic = 0;

		for (int i = 0; i < contours.size(); i++) {
			if (hierarchy[i][2] != -1 && ic == 0) {
				parentIdx = i;
				ic++;
			}
			else if (hierarchy[i][2] != -1) {
				ic++;
			}
			else if (hierarchy[i][2] == -1) {
				ic = 0;
				parentIdx = -1;
			}
			if (ic == 2) {
				bool isQrPoint = helpFunction::IsQrPoint(contours[parentIdx], binaryImg);
				if (isQrPoint) {
					qrPoints.push_back(contours[parentIdx]);
				}
				ic = 0;
				parentIdx = -1;
			}
		}

		// 如果找到足够的点，直接返回
		if (qrPoints.size() >= 3) {
			return true;
		}

		// 备用方案：检查所有大轮廓
		for (int i = 0; i < contours.size(); i++) {
			if (contours[i].size() > 30) {
				RotatedRect rect = minAreaRect(contours[i]);
				Mat cropImg = helpFunction::CropRect(binaryImg, rect);
				if (!cropImg.empty() && cropImg.rows > 10 && cropImg.cols > 10) {
					if (helpFunction::IsQrBWRate(cropImg)) {
						qrPoints.push_back(contours[i]);
					}
				}
			}
		}

		return qrPoints.size() >= 3;
	}

	/**
	 * @brief 调整定位点顺序并计算第四个点
	 * @param positionPoints 定位点中心坐标
	 * @param positionContours 定位点轮廓
	 * @param precise 是否精确模式
	 * @return 调整后的四个角点(左上、右上、右下、左下)
	 */
	vector<Point2f> adjustPositionPoints(const vector<Point2f>& points, const vector<vector<Point>>& positionContours, bool precise) {
		if (points.size() < 3 || points.size() > 4) {
			return points;
		}

		// 如果有4个点，直接用这4个点进行排序
		if (points.size() == 4) {
			vector<Point2f> result(4);
			
			// 找出四个角：左上、右上、右下、左下
			Point2f tl = points[0];
			Point2f tr = points[0];
			Point2f br = points[0];
			Point2f bl = points[0];
			
			for (const auto& p : points) {
				// 左上：x+y最小
				if (p.x + p.y < tl.x + tl.y) tl = p;
				// 右上：x-y最大
				if (p.x - p.y > tr.x - tr.y) tr = p;
				// 右下：x+y最大
				if (p.x + p.y > br.x + br.y) br = p;
				// 左下：-x+y最大
				if (-p.x + p.y > -bl.x + bl.y) bl = p;
			}
			
			result[0] = tl;  // 左上
			result[1] = tr;  // 右上
			result[2] = br;  // 右下
			result[3] = bl;  // 左下
			
			return result;
		}

		// 只有3个点的情况
		vector<Point2f> workPoints = points;

		// 计算边界
		float minX = 1e10f, maxX = -1e10f;
		float minY = 1e10f, maxY = -1e10f;
		for (const auto& p : workPoints) {
			if (p.x < minX) minX = p.x;
			if (p.x > maxX) maxX = p.x;
			if (p.y < minY) minY = p.y;
			if (p.y > maxY) maxY = p.y;
		}

		Point2f topLeft, topRight, bottomLeft;

		// 按Y坐标分组
		float midY = (minY + maxY) / 2;
		vector<Point2f> topCands, bottomCands;
		for (const auto& p : workPoints) {
			if (p.y < midY) {
				topCands.push_back(p);
			}
			else {
				bottomCands.push_back(p);
			}
		}

		// 确定左下角
		if (bottomCands.size() >= 1) {
			sort(bottomCands.begin(), bottomCands.end(), [](const Point2f& a, const Point2f& b) {
				return a.x < b.x;
			});
			bottomLeft = bottomCands[0];
		}

		// 确定左上角和右上角
		if (topCands.size() >= 2) {
			sort(topCands.begin(), topCands.end(), [](const Point2f& a, const Point2f& b) {
				return a.x < b.x;
			});
			topLeft = topCands[0];
			topRight = topCands[1];
		}
		else if (topCands.size() == 1 && bottomCands.size() >= 2) {
			topLeft = topCands[0];
			sort(bottomCands.begin(), bottomCands.end(), [](const Point2f& a, const Point2f& b) {
				return a.x < b.x;
			});
			topRight = bottomCands[0];
		}

		// 构建结果
		vector<Point2f> result;
		result.push_back(topLeft);       // 左上
		result.push_back(topRight);      // 右上
		result.push_back(bottomLeft + topRight - topLeft);  // 计算右下角
		result.push_back(bottomLeft);    // 左下

		return result;
	}

	/**
	 * @brief 透视变换：四边形区域转换为矩形
	 * @param srcImg 源图像
	 * @param srcPoints 四边形四个角点
	 * @return 变换后的矩形图像
	 */
	Mat cropParallelRect(const Mat& srcImg, const vector<Point2f>& srcPoints) {
		if (srcPoints.size() != 4) {
			return Mat();
		}

		// 固定输出尺寸为1080x1080，包含20像素缓冲区
		const int targetSize = 1080;
		const int bufferSize = 20;
		const int qrSize = targetSize - 2 * bufferSize; // 二维码实际大小

		Size size(targetSize, targetSize);

		// 目标点(标准矩形)，包含缓冲区，确保左角点左上角为(20,20)
		vector<Point2f> dstPoints = {
			Point2f(bufferSize, bufferSize),           // 左上
			Point2f(bufferSize + qrSize - 1, bufferSize),  // 右上
			Point2f(bufferSize + qrSize - 1, bufferSize + qrSize - 1), // 右下
			Point2f(bufferSize, bufferSize + qrSize - 1)    // 左下
		};

		// 计算透视变换矩阵并变换
		Mat M = getPerspectiveTransform(srcPoints, dstPoints);
		Mat dstImg;
		warpPerspective(srcImg, dstImg, M, size, INTER_LINEAR, BORDER_CONSTANT, Scalar(255, 255, 255));

		return dstImg;
	}

	/**
	 * @brief 10x10像素块统一处理：将图像按10x10像素块统一为黑色或白色
	 * @param img 输入图像(二值化图像)
	 * @return 处理后的图像
	 */
	Mat OptimizeBy10x10Blocks(const Mat& img) {
		Mat result = img.clone();
		int blockSize = 10;

		for (int y = 0; y < img.rows; y += blockSize) {
			for (int x = 0; x < img.cols; x += blockSize) {
				// 计算当前块的范围，只处理完整的10x10块
				if (x + blockSize > img.cols || y + blockSize > img.rows) {
					continue;
				}

				// 只统计最中心4x4区域的像素
				int blackCount = 0;
				int totalPixels = 0;

				// 只看中心4x4（像素位置 3-6）
				for (int by = 3; by <= 6; by++) {
					for (int bx = 3; bx <= 6; bx++) {
						if (img.at<uchar>(y + by, x + bx) == 0) {
							blackCount++;
						}
						totalPixels++;
					}
				}

				float blackRatio = (float)blackCount / totalPixels;
				uchar blockColor;

				// 只使用中心4x4，简单阈值判定
				if (blackRatio >= 0.50f) {
					blockColor = 0;
				}
				else {
					blockColor = 255;
				}

				// 应用到结果图像
				for (int by = 0; by < blockSize; by++) {
					for (int bx = 0; bx < blockSize; bx++) {
						result.at<uchar>(y + by, x + bx) = blockColor;
					}
				}
			}
		}

		// 确保边缘20像素缓冲区为纯白色
		int bufferSize = 20;
		for (int y = 0; y < img.rows; y++) {
			for (int x = 0; x < img.cols; x++) {
				if (x < bufferSize || x >= img.cols - bufferSize ||
					y < bufferSize || y >= img.rows - bufferSize) {
					result.at<uchar>(y, x) = 255;
				}
			}
		}

		return result;
	}

	/**
	 * @brief 二维码定位主函数
	 * @param srcImg 源彩色图像
	 * @param dstImg 输出的二维码图像(108x108)
	 * @param debugPath 调试图像保存路径(可选)
	 * @return 是否成功定位
	 * @details 完整的二维码定位流程
	 */
	bool Main(const Mat& srcImg, Mat& dstImg, const string& debugPath) {
		vector<vector<Point>> qrPoints;
		// 尝试不同的模糊率
		float blurRates[] = { 0.0005f, 0.0000f, 0.00025f, 0.001f, 0.0001f };

		for (int i = 0; i < 5; i++) {
				// 预处理：使用组合方法处理图像
				Mat binaryImg;
				if (i == 0) {
					// 组合预处理方法
					binaryImg = preprocessImgV2_Combined(srcImg);
				} else {
					// 尝试不同的模糊率
					binaryImg = preprocessImgV2_OTSU(srcImg, blurRates[i]);
				}

				// 保存调试图像
				if (!debugPath.empty() && i == 0) {
					imwrite(debugPath + "_0_binary.png", binaryImg);
				}

				// 应用形态学操作去除噪声
				binaryImg = preprocessImgV2_Morphology(binaryImg);

				qrPoints.clear();
				// 查找定位点
				if (findPositionPoints(binaryImg, qrPoints)) {
				if (qrPoints.size() >= 3) {
					// 保存定位点调试图像
			if (!debugPath.empty()) {
				Mat debugPoints = srcImg.clone();
				for (size_t k = 0; k < qrPoints.size(); k++) {
					vector<Point> cont = qrPoints[k];
					for (int c = 0; c < cont.size(); c++) {
						circle(debugPoints, cont[c], 3, Scalar(0, 255, 0), -1);
					}
				}
				imwrite(debugPath + "_1_qrpoints.png", debugPoints);
			}

			// 保存预处理后的二值图像
			if (!debugPath.empty() && SAVE_PREPROCESS_IMAGES && !binaryImg.empty()) {
				imwrite(debugPath + "_binary.png", binaryImg);
			}

					// 过滤多余的点
					int dumpResult = DumpExcessQrPoint(qrPoints);
					// 调整点顺序
					AdjustPointsOrder(qrPoints);

					// 计算所有定位点的中心
					vector<Point2f> centers;
					for (const auto& contour : qrPoints) {
						centers.push_back(helpFunction::CalRectCenter(contour));
					}

					// 根据定位点的位置确定每个定位点的角点方向
					// 先找到四个角的大致位置（基于中心）
					vector<Point2f> precisePoints;
					
					if (qrPoints.size() >= 3) {
						// 找到各个定位点在坐标系中的位置
						Point2f tlCenter, trCenter, blCenter, brCenter;
						bool hasFourth = qrPoints.size() >= 4;
						
						// 找出x和y的极值
						float minX = 1e10f, maxX = -1e10f;
						float minY = 1e10f, maxY = -1e10f;
						for (const auto& c : centers) {
							if (c.x < minX) minX = c.x;
							if (c.x > maxX) maxX = c.x;
							if (c.y < minY) minY = c.y;
							if (c.y > maxY) maxY = c.y;
						}
						
						// 为每个定位点确定对应的角点方向并查找精确像素
						for (int k = 0; k < qrPoints.size(); k++) {
							Point2f center = centers[k];
							int direction = -1;
							
							// 判断这个定位点是哪个角
							float distToTL = (center.x - minX) + (center.y - minY);
							float distToTR = (maxX - center.x) + (center.y - minY);
							float distToBL = (center.x - minX) + (maxY - center.y);
							float distToBR = (maxX - center.x) + (maxY - center.y);
							
							// 找到最小距离的角
							if (distToTL <= distToTR && distToTL <= distToBL && distToTL <= distToBR) {
								direction = 0; // 左上定位点的左上角
							} else if (distToTR <= distToTL && distToTR <= distToBL && distToTR <= distToBR) {
								direction = 1; // 右上定位点的右上角
							} else if (distToBL <= distToTL && distToBL <= distToTR && distToBL <= distToBR) {
								direction = 3; // 左下定位点的左下角
							} else {
								direction = 2; // 右下定位点的右下角
							}
							
							// 查找精确的黑色像素角点
							Point2f preciseCorner = helpFunction::FindPreciseCornerPixel(binaryImg, qrPoints[k], direction);
							precisePoints.push_back(preciseCorner);
						}
					}

					// 调整位置点顺序
					vector<Point2f> adjusted1 = adjustPositionPoints(precisePoints, qrPoints, false);

					// 保存调整后的点调试图像
					if (!debugPath.empty()) {
						Mat debugPoints2 = srcImg.clone();
						for (int k = 0; k < precisePoints.size(); k++) {
							circle(debugPoints2, precisePoints[k], 8, Scalar(0, 255, 0), 2);
							putText(debugPoints2, to_string(k), precisePoints[k], FONT_HERSHEY_SIMPLEX, 0.5, Scalar(255, 255, 0), 2);
						}
						for (int k = 0; k < adjusted1.size(); k++) {
							circle(debugPoints2, adjusted1[k], 5, Scalar(0, 0, 255), -1);
							putText(debugPoints2, to_string(k) + "a", adjusted1[k], FONT_HERSHEY_SIMPLEX, 0.4, Scalar(255, 0, 255), 2);
						}
						imwrite(debugPath + "_1a_points.png", debugPoints2);
					}

					// 透视变换裁剪
					Mat cropped1 = cropParallelRect(srcImg, adjusted1);

					if (!cropped1.empty()) {
						// 保存裁剪调试图像（不画点，因为坐标已转换）
					if (!debugPath.empty()) {
						Mat debugCrop = cropped1.clone();
						imwrite(debugPath + "_2_cropped1.png", debugCrop);
					}

						Mat finalCrop = cropped1;

						Mat grayCrop;
						// 灰度化
						if (finalCrop.channels() == 3) {
							cvtColor(finalCrop, grayCrop, COLOR_BGR2GRAY);
						}
						else {
							grayCrop = finalCrop.clone();
						}

						// 二值化 - 只用OTSU（不做模糊，保留浅灰色白色块的原始信息）
						Mat binaryCrop;
						threshold(grayCrop, binaryCrop, 0, 255, THRESH_BINARY | THRESH_OTSU);

						// 确保最终输出为1080x1080（使用INTER_NEAREST保持像素边缘锐利）
						Mat resizedCrop;
						if (binaryCrop.size() != Size(1080, 1080)) {
							resize(binaryCrop, resizedCrop, Size(1080, 1080), 0, 0, INTER_NEAREST);
						} else {
							resizedCrop = binaryCrop.clone();
						}

						// 在resize后再次查找定位点验证位置，避免阴影影响
						Mat verifiedCrop = resizedCrop;
						vector<vector<Point>> qrPoints2;
						if (findPositionPoints(verifiedCrop, qrPoints2)) {
							if (qrPoints2.size() >= 3) {
								// 找到四个角点，进行精确对齐
								vector<Point2f> precisePoints2;
								for (size_t k = 0; k < qrPoints2.size(); k++) {
									Point2f center = helpFunction::CalRectCenter(qrPoints2[k]);
									precisePoints2.push_back(center);
								}

								// 调整点顺序（找到四个角的大致位置）
								if (precisePoints2.size() >= 3) {
									Point2f tlCenter, trCenter, blCenter, brCenter;
									bool hasFourth = qrPoints2.size() >= 4;

									// 找出x和y的极值
									float minX = 1e10f, maxX = -1e10f;
									float minY = 1e10f, maxY = -1e10f;
									for (const auto& c : precisePoints2) {
										if (c.x < minX) minX = c.x;
										if (c.x > maxX) maxX = c.x;
										if (c.y < minY) minY = c.y;
										if (c.y > maxY) maxY = c.y;
									}

									// 为每个定位点确定对应的角点方向并查找精确像素
									vector<Point2f> preciseCorners2;
									for (int k = 0; k < qrPoints2.size(); k++) {
										Point2f center = precisePoints2[k];
										int direction = -1;

										// 判断这个定位点是哪个角
										float distToTL = (center.x - minX) + (center.y - minY);
										float distToTR = (maxX - center.x) + (center.y - minY);
										float distToBL = (center.x - minX) + (maxY - center.y);
										float distToBR = (maxX - center.x) + (maxY - center.y);

										// 找到最小距离的角
										if (distToTL <= distToTR && distToTL <= distToBL && distToTL <= distToBR) {
											direction = 0; // 左上定位点的左上角
										} else if (distToTR <= distToTL && distToTR <= distToBL && distToTR <= distToBR) {
											direction = 1; // 右上定位点的右上角
										} else if (distToBL <= distToTL && distToBL <= distToTR && distToBL <= distToBR) {
											direction = 3; // 左下定位点的左下角
										} else {
											direction = 2; // 右下定位点的右下角
										}

										// 查找精确的黑色像素角点
										Point2f preciseCorner = helpFunction::FindPreciseCornerPixel(verifiedCrop, qrPoints2[k], direction);
										preciseCorners2.push_back(preciseCorner);
									}

									// 调整位置点顺序
									vector<Point2f> adjusted2 = adjustPositionPoints(preciseCorners2, qrPoints2, false);

									// 只有当我们找到4个点时才进行微调
									if (adjusted2.size() == 4) {
										// 再次透视变换，使用和cropParallelRect完全一样的目标坐标
										const int targetSize = 1080;
										const int bufferSize = 20;
										const int qrSize = targetSize - 2 * bufferSize;
										Size size(targetSize, targetSize);

										vector<Point2f> dstPoints2 = {
											Point2f(bufferSize, bufferSize),
											Point2f(bufferSize + qrSize - 1, bufferSize),
											Point2f(bufferSize + qrSize - 1, bufferSize + qrSize - 1),
											Point2f(bufferSize, bufferSize + qrSize - 1)
										};

										Mat M2 = getPerspectiveTransform(adjusted2, dstPoints2);
										Mat dstImg2;
										warpPerspective(verifiedCrop, dstImg2, M2, size, INTER_NEAREST, BORDER_CONSTANT, Scalar(255));
										verifiedCrop = dstImg2;
									}
								}
							}
						}

						// 应用10x10像素块统一优化
						dstImg = OptimizeBy10x10Blocks(verifiedCrop);

						// 保存最终调整大小后的图像
						if (!debugPath.empty() && SAVE_PREPROCESS_IMAGES && !dstImg.empty()) {
							imwrite(debugPath + "_final_1080x1080.png", dstImg);
						}

						return true;
					}
				}
			}
		}

		return false;
	}
}
