#include "pic_v2.h"

namespace ImgPraseV2 {

	// ==================== 阈值参数 ====================
	// 二维码定位点检测的判断阈值（放宽以适应手机拍摄图片）
	constexpr float MinRightAngle = 70.0;    // 最小直角角度
	constexpr float MaxRightAngle = 110.0;   // 最大直角角度
	constexpr float MaxQRBWRate = 3.5;       // 黑白比例最大值（放宽）
	constexpr float MinQRBWRate = 0.30;      // 黑白比例最小值（放宽）
	constexpr int MinQRSize = 8;            // 定位点最小尺寸 (像素)（减小）
	constexpr float MaxQRScale = 0.5;       // 定位点占图像最大比例（放宽）
	constexpr float MinQRXYRate = 0.5;       // 定位点宽高比下限（大幅放宽）
	constexpr float MaxQRXYRate = 2.0;       // 定位点宽高比上限（大幅放宽）

	// ==================== 辅助函数 ====================

	// 将图像旋转90度
	Mat Rotation_90(const Mat& srcImg) {
		Mat tempImg;
		transpose(srcImg, tempImg);
		flip(tempImg, tempImg, 1);
		return tempImg;
	}

	namespace helpFunction {

		// 解析信息结构体：存储二维码定位点的中心、尺寸和旋转矩形
		struct ParseInfo {
			Point2f Center;      // 定位点中心
			int size;            // 轮廓点数
			RotatedRect Rect;    // 最小外接矩形
			ParseInfo(const vector<Point>& pointSet) :Center(CalRectCenter(pointSet)), size(pointSet.size()), Rect(minAreaRect(pointSet)) {}
			ParseInfo() = default;
		};

		// 计算轮廓的中心点：取轮廓边界上4个等分点的平均值
		Point2f CalRectCenter(const vector<Point>& pointSet) {
			int n = pointSet.size();
			return Point2f(
				(pointSet[n / 4].x + pointSet[n * 2 / 4].x + pointSet[n * 3 / 4].x + pointSet[n - 1].x) / 4.0f,
				(pointSet[n / 4].y + pointSet[n * 2 / 4].y + pointSet[n * 3 / 4].y + pointSet[n - 1].y) / 4.0f
			);
		}

		// 计算两点之间的欧氏距离
		float distance(const Point2f& a, const Point2f& b) {
			return sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y));
		}

		// 计算三点构成的夹角角度（度）
		float Cal3PointAngle(const Point& point0, const Point& point1, const Point& point2) {
			float dx1 = point1.x - point0.x, dy1 = point1.y - point0.y;
			float dx2 = point2.x - point0.x, dy2 = point2.y - point0.y;
			return (dx1 * dx2 + dy1 * dy2) / sqrt((dx1 * dx1 + dy1 * dy1) * (dx2 * dx2 + dy2 * dy2) + 1e-10f) * 180.0f / CV_PI;
		}

		// 判断角度是否为直角（允许一定误差范围）
		bool isRightAngle(float angle) {
			return MinRightAngle <= angle && MaxRightAngle >= angle;
		}

		// 判断黑白比例是否合法
		bool IsQrBWRateLegal(const float rate) {
			return rate < MaxQRBWRate && rate > MinQRBWRate;
		}

		// 黑白条纹预处理：统计图像中心行的黑白条纹数量和宽度
		// 二维码定位点特征：黑白黑白黑 共5条条纹（1:1:3:1:1）
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
			return vValueCount.size() >= 5;  // 至少需要5条条纹
		}

		// 黑白比例检测：简化版，放宽约束
		bool IsQrBWRateXLegal(Mat& image) {
			vector<int> vValueCount;
			if (!BWRatePreprocessing(image, vValueCount)) return false;

			// 找到中间最宽的黑条
			int index = -1, maxCount = -1;
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

			// 放宽条件：只需要确保有足够的条纹
			if (index < 1 || (vValueCount.size() - index) < 2) return false;

			float rate = (float)maxCount / 3.00;
			bool checkTag = true;
			for (int i = max(0, index - 1); i <= min((int)vValueCount.size() - 1, index + 1); i++) {
				if (i == index) continue;
				float ratio = (float)vValueCount[i] / rate;
				if (ratio < 0.3f || ratio > 3.5f) {
					checkTag = false;
					break;
				}
			}
			return checkTag;
		}

		// 双向黑白比例检测：分别检测X和Y方向
		bool IsQrBWRate(Mat& image) {
			bool xTest = IsQrBWRateXLegal(image);
			if (!xTest) return false;

			// 旋转90度检测Y方向
			Mat imageT;
			transpose(image, imageT);
			flip(imageT, imageT, 1);
			bool yTest = IsQrBWRateXLegal(imageT);
			return yTest;
		}

		// 检测定位点尺寸是否合法
		bool IsQrSizeLegal(const Size2f& qrSize, const Size2f& imgSize) {
			float xYScale = qrSize.width / qrSize.height;
			if (qrSize.height < MinQRSize || qrSize.width < MinQRSize) return false;  // 太小
			if (qrSize.width / imgSize.width > MaxQRScale || qrSize.height / imgSize.height > MaxQRScale) return false;  // 太大
			if (xYScale < MinQRXYRate || xYScale > MaxQRXYRate) return false;  // 宽高比失衡
			return true;
		}

		// 从图像中裁剪出旋转矩形区域
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

		// 检测定位点对称性（备用，已被注释不使用）
		bool CheckQrSymmetry(const Mat& cropImg) {
			if (cropImg.empty() || cropImg.rows < 10 || cropImg.cols < 10) return false;

			int centerX = cropImg.cols / 2;
			int centerY = cropImg.rows / 2;
			int regionSize = std::min(cropImg.cols, cropImg.rows) / 4;

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

			if (hSymDiff > 0.35f || vSymDiff > 0.35f) return false;

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

			if (blackRatio - edgeBlackRatio < 0.3f) return false;

			return true;
		}

		// 判断轮廓是否为二维码定位点
		// 验证条件：1. 尺寸合法 2. 黑白比例符合1:1:3:1:1模式
		bool IsQrPoint(const vector<Point>& contour, const Mat& img) {
			RotatedRect rotatedRect = minAreaRect(contour);
			Mat cropImg = CropRect(img, rotatedRect);
			if (!IsQrSizeLegal(rotatedRect.size, img.size())) return false;
			return IsQrBWRate(cropImg);
		}

		// 判断三个点是否存在直角关系
		bool isRightAngleExist(const Point& point0, const Point& point1, const Point& point2) {
			return isRightAngle(Cal3PointAngle(point0, point1, point2)) ||
				isRightAngle(Cal3PointAngle(point1, point0, point2)) ||
				isRightAngle(Cal3PointAngle(point2, point0, point1));
		}

		// 计算三个数的方差
		double Cal3NumVariance(const int a, const int b, const int c) {
			double avg = (a + b + c) / 3.0;
			return ((a - avg) * (a - avg) + (b - avg) * (b - avg) + (c - avg) * (c - avg)) / 3.0;
		}

		// 判断点序的顺逆时针关系
		bool IsClockwise(const Point& basePoint, const Point& point1, const Point& point2) {
			float ax = point1.x - basePoint.x, ay = point1.y - basePoint.y;
			float bx = point2.x - basePoint.x, by = point2.y - basePoint.y;
			return (ax * by - ay * bx) > 0;
		}

		// 根据三个角点计算第四个角点（平行四边形）
		Point CalForthPoint(const Point& poi0, const Point& poi1, const Point& poi2) {
			return Point(poi2.x + poi1.x - poi0.x, poi2.y + poi1.y - poi0.y);
		}

		// 计算外角平分向量
		pair<float, float> CalExtendVec(const Point2f& poi0, const Point2f& poi1, const Point2f& poi2, float bias) {
			float dis0 = distance(poi0, poi1), dis1 = distance(poi0, poi2);
			float rate = dis1 / dis0;
			float x1 = poi0.x - poi2.x, y1 = poi0.y - poi2.y;
			float x2 = (poi0.x - poi1.x) * rate, y2 = (poi0.y - poi1.y) * rate;
			float totx = x1 + x2, toty = y1 + y2, distot = sqrt(totx * totx + toty * toty);
			return { totx / distot * bias, toty / distot * bias };
		}

		// 保持纵横比的resize函数
		void resizeKeepAspect(const Mat& src, Mat& dst, const Size& targetSize) {
			if (src.empty()) return;

			int srcW = src.cols;
			int srcH = src.rows;
			int dstW = targetSize.width;
			int dstH = targetSize.height;

			float scale = min((float)dstW / srcW, (float)dstH / srcH);
			int newW = (int)(srcW * scale);
			int newH = (int)(srcH * scale);

			Mat scaled;
			resize(src, scaled, Size(newW, newH), 0, 0, INTER_LANCZOS4);

			dst = Mat(dstH, dstW, src.type(), Scalar::all(255));

			int xOffset = (dstW - newW) / 2;
			int yOffset = (dstH - newH) / 2;

			Rect roi(xOffset, yOffset, newW, newH);
			scaled.copyTo(dst(roi));
		}

	}

	// ==================== 图像预处理函数 ====================

	// OTSU自适应阈值预处理（改进版）
	Mat preprocessImgV2_OTSU(const Mat& srcImg, float blurRate) {
		Mat tempImg;
		cvtColor(srcImg, tempImg, COLOR_BGR2GRAY);

		// 轻度模糊去噪
		blur(tempImg, tempImg, Size(3, 3));

		// 第一次OTSU
		Mat otsu1;
		threshold(tempImg, otsu1, 0, 255, THRESH_BINARY | THRESH_OTSU);

		// 计算黑像素比例
		int blackCount = countNonZero(otsu1 == 0);
		float blackRatio = (float)blackCount / (otsu1.rows * otsu1.cols);

		// 如果黑色像素过多，增加模糊后重新处理
		if (blackRatio > 0.85f) {
			GaussianBlur(tempImg, tempImg, Size(5, 5), 0);
		}

		threshold(tempImg, tempImg, 0, 255, THRESH_BINARY | THRESH_OTSU);
		return tempImg;
	}

	// ==================== 核心定位函数 ====================

	// 验证三个定位点是否构成有效的二维码
	bool IsValidQrTriple(const helpFunction::ParseInfo& p1, const helpFunction::ParseInfo& p2, const helpFunction::ParseInfo& p3) {
		float d12 = helpFunction::distance(p1.Center, p2.Center);
		float d13 = helpFunction::distance(p1.Center, p3.Center);
		float d23 = helpFunction::distance(p2.Center, p3.Center);

		float maxDist = std::max({ d12, d13, d23 });
		float minDist = std::min({ d12, d13, d23 });

		if (minDist < 15) return false;
		if (maxDist > 600) return false;

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

		float a = d12, b = d13, c = d23;
		float angle1 = acos((b * b + c * c - a * a) / (2 * b * c + 1e-10f)) * 180.0f / CV_PI;
		float angle2 = acos((a * a + c * c - b * b) / (2 * a * c + 1e-10f)) * 180.0f / CV_PI;
		float angle3 = acos((a * a + b * b - c * c) / (2 * a * b + 1e-10f)) * 180.0f / CV_PI;

		bool hasRightAngle = (angle1 > 65 && angle1 < 115) ||
			(angle2 > 65 && angle2 < 115) ||
			(angle3 > 65 && angle3 < 115);

		if (!hasRightAngle) return false;

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

	// 调整三个定位点的顺序，使其符合 左上、右上、左下 的顺序
	void AdjustPointsOrder(vector<vector<Point>>& src3Points) {
		vector<vector<Point>> temp;
		Point p3[3] = { helpFunction::CalRectCenter(src3Points[0]),
						helpFunction::CalRectCenter(src3Points[1]),
						helpFunction::CalRectCenter(src3Points[2]) };
		int index[3][3] = { { 0,1,2 },{1,0,2},{2,0,1} };
		for (int i = 0; i < 3; i++) {
			if (helpFunction::isRightAngle(helpFunction::Cal3PointAngle(p3[index[i][0]], p3[index[i][1]], p3[index[i][2]]))) {
				temp.push_back(std::move(src3Points[index[i][0]]));
				if (helpFunction::IsClockwise(p3[index[i][0]], p3[index[i][1]], p3[index[i][2]])) {
					temp.push_back(std::move(src3Points[index[i][1]]));
					temp.push_back(std::move(src3Points[index[i][2]]));
				}
				else {
					temp.push_back(std::move(src3Points[index[i][2]]));
					temp.push_back(std::move(src3Points[index[i][1]]));
				}
				for (int i = 3; i < src3Points.size(); ++i)
					temp.push_back(std::move(src3Points[i]));
				std::swap(temp, src3Points);
				return;
			}
		}
		return;
	}

	// 筛选出正确的三个定位点：去除多余轮廓，保留构成直角的三个
	bool DumpExcessQrPoint(vector<vector<Point>>& qrPoints) {
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
			if (!helpFunction::isRightAngleExist(Point2 = helpFunction::CalRectCenter(qrPoints[i]), Point1, Point0))
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

	// 查找二维码的大致轮廓区域
	// 通过形态学操作找出包含二维码的矩形区域
	bool findQrRegion(const Mat& binaryImg, RotatedRect& qrRect) {
		// 闭运算：填充二维码内部的小空白
		Mat morphImg;
		Mat kernel = getStructuringElement(MORPH_RECT, Size(15, 15));
		morphologyEx(binaryImg, morphImg, MORPH_CLOSE, kernel);

		// 查找轮廓
		vector<vector<Point>> contours;
		vector<Vec4i> hierarchy;
		findContours(morphImg.clone(), contours, hierarchy, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);

		// 找最大的矩形轮廓
		float maxArea = 0;
		for (const auto& contour : contours) {
			RotatedRect rect = minAreaRect(contour);
			float area = rect.size.width * rect.size.height;
			if (area > maxArea && rect.size.width > 50 && rect.size.height > 50) {
				// 检查宽高比接近正方形（二维码）
				float ratio = rect.size.width / rect.size.height;
				if (ratio > 0.5f && ratio < 2.0f) {
					maxArea = area;
					qrRect = rect;
				}
			}
		}

		return maxArea > 0;
	}

	// 查找二维码定位点
	// 直接找三级嵌套结构，跳过黑白比例检查
	bool findPositionPoints(const Mat& binaryImg, vector<vector<Point>>& qrPoints) {
		vector<vector<Point>> contours;
		vector<Vec4i> hierarchy;
		findContours(binaryImg, contours, hierarchy, RETR_TREE, CHAIN_APPROX_NONE, Point(0, 0));

		// 直接找三级嵌套：父轮廓 -> 子轮廓 -> 孙轮廓
		for (size_t i = 0; i < contours.size(); ++i) {
			int child = hierarchy[i][2];
			if (child == -1) continue;

			int grandchild = hierarchy[child][2];
			if (grandchild == -1) continue;

			// 确保不超过三级
			if (hierarchy[grandchild][2] != -1) continue;

			// 直接作为候选点（跳过IsQrPoint检查）
			qrPoints.push_back(contours[i]);
		}

		return qrPoints.size() >= 3;
	}

	// 根据三个定位点坐标，调整为标准的四边形角点顺序
	// 输出：左上、右上、右下、左下
	vector<Point2f> adjustPositionPoints(const vector<Point2f>& points) {
		double d[3] = { helpFunction::distance(points[0],points[1]),
			helpFunction::distance(points[0],points[2]),
			helpFunction::distance(points[1],points[2])
		};

		Point2f topLeft, topRight, bottomLeft;
		if (d[0] > d[1] && d[0] > d[2])
		{
			topLeft = points[2];
			if (helpFunction::IsClockwise(points[2], points[0], points[1])) {
				topRight = points[0];
				bottomLeft = points[1];
			}
			else {
				topRight = points[1];
				bottomLeft = points[0];
			}
		}
		else if (d[1] > d[0] && d[1] > d[2]) {
			topLeft = points[1];
			if (helpFunction::IsClockwise(points[1], points[0], points[2])) {
				topRight = points[0];
				bottomLeft = points[2];
			}
			else {
				topRight = points[2];
				bottomLeft = points[0];
			}
		}
		else {
			topLeft = points[0];
			if (helpFunction::IsClockwise(points[0], points[1], points[2])) {
				topRight = points[1];
				bottomLeft = points[2];
			}
			else {
				topRight = points[2];
				bottomLeft = points[1];
			}
		}

		Point2f bottomRight = bottomLeft + topRight - topLeft;
		vector<Point2f> result;
		result.push_back(topLeft);
		result.push_back(topRight);
		result.push_back(bottomRight);
		result.push_back(bottomLeft);
		return result;
	}

	// 根据定位点轮廓计算裁剪区域（改进版）
	// 使用三个定位点之间的实际距离来计算二维码大小
	vector<Point2f> calculateQrRegion(const vector<vector<Point>>& qrPoints) {
		// 获取三个定位点的中心
		Point2f centers[3];
		RotatedRect rects[3];
		for (int i = 0; i < 3; i++) {
			rects[i] = minAreaRect(qrPoints[i]);
			centers[i] = helpFunction::CalRectCenter(qrPoints[i]);
		}

		// 计算定位点大小（取宽高的平均值）
		float sizes[3];
		for (int i = 0; i < 3; i++) {
			sizes[i] = (rects[i].size.width + rects[i].size.height) / 2.0f;
		}
		float avgSize = (sizes[0] + sizes[1] + sizes[2]) / 3.0f;

		// 计算三个定位点两两之间的距离
		float d01 = helpFunction::distance(centers[0], centers[1]);
		float d02 = helpFunction::distance(centers[0], centers[2]);
		float d12 = helpFunction::distance(centers[1], centers[2]);

		// 找到最长边（这是二维码的一条边）
		float maxDist = max({ d01, d02, d12 });
		float minDist = min({ d01, d02, d12 });

		// 根据定位点间距离估算二维码边长
		// 定位点中心到二维码边缘的距离约为定位点大小的 3 倍
		// 二维码边长 ≈ 定位点间距 + 2 × 定位点大小 × 3
		float qrSide = maxDist + avgSize * 1.5f;

		// 找到左上、右上、左下定位点
		Point2f topLeft, topRight, bottomLeft;
		if (d01 >= d02 && d01 >= d12) {
			topLeft = centers[2];
			if (helpFunction::IsClockwise(centers[2], centers[0], centers[1])) {
				topRight = centers[0];
				bottomLeft = centers[1];
			}
			else {
				topRight = centers[1];
				bottomLeft = centers[0];
			}
		}
		else if (d02 >= d01 && d02 >= d12) {
			topLeft = centers[1];
			if (helpFunction::IsClockwise(centers[1], centers[0], centers[2])) {
				topRight = centers[0];
				bottomLeft = centers[2];
			}
			else {
				topRight = centers[2];
				bottomLeft = centers[0];
			}
		}
		else {
			topLeft = centers[0];
			if (helpFunction::IsClockwise(centers[0], centers[1], centers[2])) {
				topRight = centers[1];
				bottomLeft = centers[2];
			}
			else {
				topRight = centers[2];
				bottomLeft = centers[1];
			}
		}

		// 使用定位点大小作为扩展基准
		// 从定位点中心向外扩展约 3.0 倍定位点大小（适当减小）
		float margin = avgSize * 3.0f;

		Point2f resultTopLeft(topLeft.x - margin, topLeft.y - margin);
		Point2f resultTopRight(topRight.x + margin, topRight.y - margin);
		Point2f resultBottomRight(bottomLeft.x + margin, bottomLeft.y + margin);
		Point2f resultBottomLeft(bottomLeft.x - margin, bottomLeft.y + margin);

		vector<Point2f> result = { resultTopLeft, resultTopRight, resultBottomRight, resultBottomLeft };
		return result;
	}

	// ==================== 裁剪相关函数 ====================

	// Step 1: 初步剪切 - 根据定位点外接矩形确定大致区域
	Mat step1_preliminaryCrop(const Mat& srcImg, const vector<vector<Point>>& qrPoints) {
		// 获取三个定位点的外接矩形
		RotatedRect rects[3];
		for (int i = 0; i < 3; i++) {
			rects[i] = minAreaRect(qrPoints[i]);
		}

		// 找到所有定位点的外接矩形（合并三个矩形）
		Point2f centers[3];
		for (int i = 0; i < 3; i++) {
			centers[i] = rects[i].center;
		}

		// 计算定位点大小
		float sizes[3];
		for (int i = 0; i < 3; i++) {
			sizes[i] = (rects[i].size.width + rects[i].size.height) / 2.0f;
		}
		float avgSize = (sizes[0] + sizes[1] + sizes[2]) / 3.0f;

		// 使用定位点大小作为边距基准（较小边距，只为排除明显环境）
		float margin = avgSize * 1.2f;

		// 找到边界
		float minX = centers[0].x, maxX = centers[0].x;
		float minY = centers[0].y, maxY = centers[0].y;
		for (int i = 1; i < 3; i++) {
			minX = min(minX, centers[i].x);
			maxX = max(maxX, centers[i].x);
			minY = min(minY, centers[i].y);
			maxY = max(maxY, centers[i].y);
		}

		// 扩展边界
		int x1 = (int)(minX - margin);
		int y1 = (int)(minY - margin);
		int x2 = (int)(maxX + margin);
		int y2 = (int)(maxY + margin);

		// 边界检查
		x1 = max(0, x1);
		y1 = max(0, y1);
		x2 = min(srcImg.cols - 1, x2);
		y2 = min(srcImg.rows - 1, y2);

		if (x2 <= x1 || y2 <= y1) return Mat();

		return srcImg(Rect(x1, y1, x2 - x1, y2 - y1));
	}

	// Step 2: 透视变换校正 - 精确计算二维码的四个完整角点
	Mat step2_perspectiveCorrect(const Mat& srcImg, const vector<vector<Point>>& qrPoints) {
		if (qrPoints.size() < 3) return Mat();

		// 获取三个定位点的信息
		RotatedRect rects[3];
		Point2f centers[3];
		float sizes[3];
		for (int i = 0; i < 3; i++) {
			rects[i] = minAreaRect(qrPoints[i]);
			centers[i] = helpFunction::CalRectCenter(qrPoints[i]);
			// 使用外接矩形的宽度作为定位点大小
			sizes[i] = max(rects[i].size.width, rects[i].size.height);
		}
		float avgSize = (sizes[0] + sizes[1] + sizes[2]) / 3.0f;

		// 计算三个定位点之间的距离
		float d01 = helpFunction::distance(centers[0], centers[1]);
		float d02 = helpFunction::distance(centers[0], centers[2]);
		float d12 = helpFunction::distance(centers[1], centers[2]);

		// 找到最长边，确定三个定位点的角色（左上、右上、左下）
		Point2f p0, p1, p2;  // 临时变量
		if (d01 >= d02 && d01 >= d12) {
			p0 = centers[2];  // topLeft
			p1 = centers[0];  // topRight
			p2 = centers[1];  // bottomLeft
		}
		else if (d02 >= d01 && d02 >= d12) {
			p0 = centers[1];  // topLeft
			p1 = centers[0];  // topRight
			p2 = centers[2];  // bottomLeft
		}
		else {
			p0 = centers[0];  // topLeft
			p1 = centers[1];  // topRight
			p2 = centers[2];  // bottomLeft
		}

		// 根据顺时针/逆时针确定方向
		Point2f topLeft, topRight, bottomLeft;
		if (helpFunction::IsClockwise(p0, p1, p2)) {
			topLeft = p0;
			topRight = p1;
			bottomLeft = p2;
		}
		else {
			topLeft = p0;
			topRight = p2;
			bottomLeft = p1;
		}

		// 计算右下角
		Point2f bottomRight = topRight + bottomLeft - topLeft;

		// 计算二维码边长：定位点中心间距 + 定位点大小
		// 定位点中心到边缘的距离约为 3.5 * 定位点大小
		float distTop = helpFunction::distance(topLeft, topRight);
		float distLeft = helpFunction::distance(topLeft, bottomLeft);

		// 精确扩展：从定位点中心向外扩展
		// 二维码定位点约占1/7，所以边距约为 3 * 定位点大小
		float expandX = avgSize * 3.0f;  // 左右扩展
		float expandY = avgSize * 3.0f;  // 上下扩展

		// 四个角点
		Point2f cornerTL(topLeft.x - expandX, topLeft.y - expandY);
		Point2f cornerTR(topRight.x + expandX, topRight.y - expandY);
		Point2f cornerBL(bottomLeft.x - expandX, bottomLeft.y + expandY);
		Point2f cornerBR(bottomRight.x + expandX, bottomRight.y + expandY);

		// 计算目标正方形边长
		float width = max(helpFunction::distance(cornerTL, cornerTR), helpFunction::distance(cornerBL, cornerBR));
		float height = max(helpFunction::distance(cornerTL, cornerBL), helpFunction::distance(cornerTR, cornerBR));
		float side = max(width, height);
		int dstSize = (int)(side + 0.5f);

		// 源四边形（四个角点）- 按顺时针顺序
		vector<Point2f> srcPoints = { cornerTL, cornerTR, cornerBR, cornerBL };

		// 目标四边形（正方形）
		vector<Point2f> dstPoints = {
			Point2f(0, 0),
			Point2f(dstSize - 1, 0),
			Point2f(dstSize - 1, dstSize - 1),
			Point2f(0, dstSize - 1)
		};

		// 透视变换
		Mat M = getPerspectiveTransform(srcPoints, dstPoints);
		Mat result;
		warpPerspective(srcImg, result, M, Size(dstSize, dstSize), INTER_LANCZOS4);

		return result;
	}

	// Step 3: 精确定位 - 找到二维码整体轮廓进行最终裁剪
	Mat step3_finalCrop(const Mat& srcImg) {
		// 转为灰度并二值化
		Mat gray;
		cvtColor(srcImg, gray, COLOR_BGR2GRAY);
		threshold(gray, gray, 0, 255, THRESH_BINARY | THRESH_OTSU);

		// 形态学操作：闭运算连接二维码区域
		Mat morph;
		Mat kernel = getStructuringElement(MORPH_RECT, Size(5, 5));
		morphologyEx(gray, morph, MORPH_CLOSE, kernel);

		// 查找所有轮廓
		vector<vector<Point>> contours;
		vector<Vec4i> hierarchy;
		findContours(morph, contours, hierarchy, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);

		// 找到最大的矩形轮廓（二维码区域）
		float maxArea = 0;
		RotatedRect qrRect;
		for (const auto& contour : contours) {
			RotatedRect rect = minAreaRect(contour);
			float area = rect.size.width * rect.size.height;
			if (area > maxArea && rect.size.width > 10 && rect.size.height > 10) {
				// 检查宽高比接近正方形
				float ratio = rect.size.width / rect.size.height;
				if (ratio > 0.5f && ratio < 2.0f) {
					maxArea = area;
					qrRect = rect;
				}
			}
		}

		if (maxArea == 0) {
			// 如果找不到轮廓，返回原图
			return srcImg.clone();
		}

		// 从外接矩形扩展一点边距
		float margin = min(qrRect.size.width, qrRect.size.height) * 0.05f;

		// 使用最小外接矩形进行裁剪
		Point2f pts[4];
		qrRect.points(pts);

		// 转换为Point2f向量
		vector<Point2f> srcPoints(pts, pts + 4);

		// 计算目标尺寸
		int dstW = (int)(qrRect.size.width + margin * 2);
		int dstH = (int)(qrRect.size.height + margin * 2);
		dstW = max(dstW, dstH);
		dstH = dstW;

		// 目标四边形
		vector<Point2f> dstPoints = {
			Point2f(0, 0),
			Point2f(dstW - 1, 0),
			Point2f(dstW - 1, dstH - 1),
			Point2f(0, dstH - 1)
		};

		// 透视变换
		Mat M = getPerspectiveTransform(srcPoints, dstPoints);
		Mat result;
		warpPerspective(srcImg, result, M, Size(dstW, dstH), INTER_LANCZOS4);

		return result;
	}

	// 透视变换裁剪（保留原样）
	Mat cropParallelRect(const Mat& srcImg, const vector<Point2f>& srcPoints) {
		// 计算源四边形的宽高
		float width = max(helpFunction::distance(srcPoints[0], srcPoints[1]),
						  helpFunction::distance(srcPoints[3], srcPoints[2]));
		float height = max(helpFunction::distance(srcPoints[0], srcPoints[3]),
						  helpFunction::distance(srcPoints[1], srcPoints[2]));

		// 使用与源图像相同比例的目标四边形，避免拉伸
		int dstWidth = static_cast<int>(width + 0.5f);
		int dstHeight = static_cast<int>(height + 0.5f);

		// 确保尺寸合理
		dstWidth = max(50, min(dstWidth, 2000));
		dstHeight = max(50, min(dstHeight, 2000));

		vector<Point2f> dstPoints = {
			Point2f(0, 0),
			Point2f(dstWidth - 1, 0),
			Point2f(dstWidth - 1, dstHeight - 1),
			Point2f(0, dstHeight - 1)
		};

		Mat M = getPerspectiveTransform(srcPoints, dstPoints);
		Mat dstImg;
		warpPerspective(srcImg, dstImg, M, Size(dstWidth, dstHeight), INTER_LANCZOS4);

		return dstImg;
	}

	// 主入口函数：从原图中定位并裁剪出二维码
	// 三步流程：初步剪切 -> 透视变换校正 -> 精确定位
	// debugMode: 是否保存中间过程图片
	bool Main(const Mat& srcImg, Mat& dstImg, bool debugMode) {
		static int debugCount = 0;
		string debugDir = "debug_output";
		if (debugMode) {
			fs::create_directory(debugDir);
		}

		// 保存原图
		if (debugMode) {
			imwrite(debugDir + "/0_src.png", srcImg);
		}

		// OTSU 预处理
		Mat binaryImg = preprocessImgV2_OTSU(srcImg, 0.0f);

		// 保存二值图
		if (debugMode) {
			imwrite(debugDir + "/1_binary.png", binaryImg);
		}

		// 查找定位点
		vector<vector<Point>> qrPoints;
		if (!findPositionPoints(binaryImg, qrPoints)) {
			return false;
		}

		// 处理定位点：需要恰好3个
		if (qrPoints.size() < 3) {
			return false;
		}

		// 保存定位点标记图
		if (debugMode) {
			Mat debugImg = srcImg.clone();
			drawContours(debugImg, qrPoints, -1, Scalar(0, 0, 255), 2);
			imwrite(debugDir + "/2_qrpoints.png", debugImg);
		}

		// 选择3个最佳定位点（方法1：尝试DumpExcessQrPoint，方法2：取最大3个）
		vector<vector<Point>> selectedPoints = qrPoints;

		bool useMethod1 = false;
		if (qrPoints.size() >= 3) {
			vector<vector<Point>> testPoints = qrPoints;
			if (!DumpExcessQrPoint(testPoints)) {
				useMethod1 = true;
				selectedPoints = testPoints;
				AdjustPointsOrder(selectedPoints);
			}
		}

		if (!useMethod1) {
			// 方法2：按面积排序取前3个
			sort(selectedPoints.begin(), selectedPoints.end(), [](const vector<Point>& a, const vector<Point>& b) {
				return contourArea(a) > contourArea(b);
			});
			selectedPoints.resize(3);
			AdjustPointsOrder(selectedPoints);
		}

		// 保存选中的定位点
		if (debugMode) {
			Mat debugImg = srcImg.clone();
			drawContours(debugImg, selectedPoints, -1, Scalar(0, 255, 0), 3);
			imwrite(debugDir + "/3_selected_points.png", debugImg);
		}

		// ===== 三步裁剪流程 =====

		// Step 1: 初步剪切 - 排除大部分环境
		Mat cropped1 = step1_preliminaryCrop(srcImg, selectedPoints);
		if (cropped1.empty()) {
			return false;
		}

		// 保存Step1结果
		if (debugMode) {
			imwrite(debugDir + "/4_step1_cropped.png", cropped1);
		}

		// Step 1.5: 透视变换校正 - 使用原始图像上的定位点，将二维码矫正为正方形
		// 关键：使用selectedPoints（原始坐标），不是在剪切后的图像上重新找定位点
		Mat croppedCorrected = step2_perspectiveCorrect(srcImg, selectedPoints);
		if (croppedCorrected.empty()) {
			// 如果透视变换失败，使用初步剪切结果
			helpFunction::resizeKeepAspect(cropped1, dstImg, Size(108, 108));
			return true;
		}

		// 保存透视变换校正结果
		if (debugMode) {
			imwrite(debugDir + "/5_step1_perspective.png", croppedCorrected);
		}

		// Step 2: 在校正后的正方形图像上精确定位（使用二维码整体轮廓）
		Mat cropped3 = step3_finalCrop(croppedCorrected);
		if (!cropped3.empty()) {
			// 保存Step2结果
			if (debugMode) {
				imwrite(debugDir + "/6_step2_final.png", cropped3);
			}

			// 保持纵横比resize到108x108
			helpFunction::resizeKeepAspect(cropped3, dstImg, Size(108, 108));
			return true;
		}

		// 如果精确定位失败，使用透视变换校正结果
		if (!croppedCorrected.empty()) {
			helpFunction::resizeKeepAspect(croppedCorrected, dstImg, Size(108, 108));
			return true;
		}

		// 如果透视变换失败，使用初步剪切结果
		if (!cropped1.empty()) {
			helpFunction::resizeKeepAspect(cropped1, dstImg, Size(108, 108));
			return true;
		}

		return false;
	}
}
