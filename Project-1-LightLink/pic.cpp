#include "pic.h"
namespace ImgPrase {

	// 常量定义
	constexpr float MinRightAngle = 75.0;
	constexpr float MaxRightAngle = 105.0;
	constexpr float MaxQRBWRate = 2.25; // 黑白比例限制（理想1.0）
	constexpr float MinQRBWRate = 0.40; // 黑白比例限制
	constexpr int MinQRSize = 10; // 最小定位点尺寸
	constexpr float MaxQRScale = 0.25; // 定位点占图像的最大比例
	constexpr float MinQRXYRate = 5.0 / 6.0; // 最小长宽比
	constexpr float MaxQRXYRate = 6.0 / 5.0; // 最大长宽比
	constexpr float MaxQrTypeRate = 2.2; // 二维码类型比例
	constexpr float minQrTypeRate = 1.8; // 二维码类型比例
	constexpr float MaxDistanceRate = 1.1; // 距离比例
	constexpr float minDistanceRate = 0.9; // 距离比例

	// 旋转90度函数
	Mat Rotation_90(const Mat& srcImg) {
		Mat tempImg;
		transpose(srcImg, tempImg);
		flip(tempImg, tempImg, 1);
		return tempImg;
	}

	namespace helpFunction {

		struct ParseInfo {
			Point2f Center;  // 定位点中心坐标
			int size;        // 定位点轮廓大小
			RotatedRect Rect; // 定位点的最小包围矩形
			ParseInfo(const vector<Point>& pointSet) :Center(CalRectCenter(pointSet)), size(pointSet.size()), Rect(minAreaRect(pointSet)) {}
			ParseInfo() = default;
		};

		Point2f CalRectCenter(const vector<Point>& pointSet) {
			return Point2f((pointSet[pointSet.size() / 4].x + pointSet[pointSet.size() * 2 / 4].x + pointSet[pointSet.size() * 3 / 4].x + pointSet[pointSet.size() - 1].x) / 4.0f,
				(pointSet[pointSet.size() / 4].y + pointSet[pointSet.size() * 2 / 4].y + pointSet[pointSet.size() * 3 / 4].y + pointSet[pointSet.size() - 1].y) / 4.0f);
		}

		float distance(const Point2f& a, const Point2f& b) {
			return sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y));
		}

		float Cal3PointAngle(const Point& point0, const Point& point1, const Point& point2) {
			float dx1 = point1.x - point0.x, dy1 = point1.y - point0.y;
			float dx2 = point2.x - point0.x, dy2 = point2.y - point0.y;
			return (dx1 * dx2 + dy1 * dy2) / sqrt((dx1 * dx1 + dy1 * dy1) * (dx2 * dx2 + dy2 * dy2) + 1e-10f) * 180.0f / CV_PI;
		}

		bool isRightAngle(float angle) {
			return MinRightAngle <= angle && MaxRightAngle >= angle;
		}

		bool IsQrBWRateLegal(const float rate) {
			return rate < MaxQRBWRate && rate > MinQRBWRate;
		}

		bool BWRatePreprocessing(Mat& image, vector<int>& vValueCount) {
			int count = 0, nc = image.cols * image.channels(), nr = image.rows / 2;
			uchar lastColor = 0, * data = image.ptr<uchar>(nr);
			for (int i = 0; i < nc; i++) {
				uchar color = data[i];
				if (color > 0) color = 255;
				if (i == 0)
				{
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

		bool IsQrBWRateXLegal(Mat& image) {
			vector<int> vValueCount;
			if (!BWRatePreprocessing(image, vValueCount)) return false;

			int index = -1, maxCount = -1;
			//找到最多的块
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
			//1:1:3:1:1，不会出现在最前面和最后面
			if (index < 2 || index > vValueCount.size() - 3) return false;

			float rate = (float)maxCount / 3.00;
			bool checkTag = true;
			for (int i = index - 2; i <= index + 2; i++) {
				if (i == index) continue;
				if (!IsQrBWRateLegal((float)vValueCount[i] / rate)) {
					checkTag = false;
					break;
				}
			}
			return checkTag;
		}

		bool IsQrBWRate(Mat& image) {
			bool xTest = IsQrBWRateXLegal(image);
			if (!xTest) return false;

			Mat imageT;
			transpose(image, imageT);
			flip(imageT, imageT, 1);
			bool yTest = IsQrBWRateXLegal(imageT);
			return yTest;

		}

		bool IsQrSizeLegal(const Size2f& qrSize, const Size2f& imgSize) {
			float xYScale = qrSize.width / qrSize.height;
			if (qrSize.height < MinQRSize || qrSize.width < MinQRSize) return false;
			if (qrSize.width / imgSize.width > MaxQRScale || qrSize.height / imgSize.height > MaxQRScale) return false;
			if (xYScale < MinQRXYRate || xYScale > MaxQRXYRate) return false;
			return true;
		}

		Mat CropRect(const Mat& image, const RotatedRect& rect) {
			Mat srcPoints, disImg;
			boxPoints(rect, srcPoints);
			vector<Point2f> disPoints = {
				Point2f(0,rect.size.height - 1),//左下
				Point2f(0,0),//左上
				Point2f(rect.size.width - 1,0),
				Point2f(rect.size.width - 1, rect.size.height - 1) };
			auto M = getPerspectiveTransform(srcPoints, disPoints);
			warpPerspective(image, disImg, M, rect.size);
			return disImg;
		}
		// 检查定位点的对称性和中心特征
		bool CheckQrSymmetry(const Mat& cropImg) {
			if (cropImg.empty() || cropImg.rows < 15 || cropImg.cols < 15) return false;

			// 计算图像中心区域
			int centerX = cropImg.cols / 2;
			int centerY = cropImg.rows / 2;
			int regionSize = std::min(cropImg.cols, cropImg.rows) / 3; // 增大中心区域检查范围

			// 检查中心区域是否为黑色（定位点中心应该是黑色）
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

			// 中心区域应该主要是黑色（更严格的阈值）
			float blackRatio = (float)blackCount / totalPixels;
			if (blackRatio < 0.85f) return false;

			// 检查水平和垂直对称性
			Mat leftHalf = cropImg(Rect(0, 0, centerX, cropImg.rows));
			Mat rightHalf = cropImg(Rect(centerX, 0, centerX, cropImg.rows));
			flip(rightHalf, rightHalf, 1);

			// 计算左右对称性差异
			float hSymDiff = 0;
			for (int y = 0; y < cropImg.rows; y++) {
				for (int x = 0; x < centerX; x++) {
					if (leftHalf.at<uchar>(y, x) != rightHalf.at<uchar>(y, x)) {
						hSymDiff++;
					}
				}
			}
			hSymDiff /= (centerX * cropImg.rows);

			// 检查上下对称性
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

			// 定位点应该具有更好的对称性（更严格的阈值）
			if (hSymDiff > 0.2f || vSymDiff > 0.2f) return false;

			// 新增：检查二维码定位点的三层结构特征
			// 计算边缘区域的黑色比例（应该较低）
			int edgeBlackCount = 0;
			int edgeTotalPixels = 0;
			int edgeWidth = std::min(cropImg.cols, cropImg.rows) / 10;

			// 检查边缘区域
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
			// 边缘区域应该主要是白色（二维码定位点的外层是白色）
			if (edgeBlackRatio > 0.4f) return false;

			// 新增：检查中心区域与边缘区域的对比度
			if (blackRatio - edgeBlackRatio < 0.4f) return false;

			return true;
		}

		//验证定位点
		bool IsQrPoint(const vector<Point>& contour, const Mat& img) {
			RotatedRect rotatedRect = minAreaRect(contour);//最小包围矩形
			Mat cropImg = CropRect(img, rotatedRect);
			if (!IsQrSizeLegal(rotatedRect.size, img.size())) return false;
			if (!IsQrBWRate(cropImg))return false;

			// 新增：验证定位点的对称性和中心特征
			if (!CheckQrSymmetry(cropImg)) return false;

			return true;
		}

		bool isRightAngleExist(const Point& point0, const Point& point1, const Point& point2) {
			return isRightAngle(Cal3PointAngle(point0, point1, point2)) ||
				isRightAngle(Cal3PointAngle(point1, point0, point2)) ||
				isRightAngle(Cal3PointAngle(point2, point0, point1));
		}

		double Cal3NumVariance(const int a, const int b, const int c) {
			double avg = (a + b + c) / 3.0;
			return ((a - avg) * (a - avg) + (b - avg) * (b - avg) + (c - avg) * (c - avg)) / 3.0;
		}

		//true表示顺时针。
		bool IsClockwise(const Point& basePoint, const Point& point1, const Point& point2) {
			float ax = point1.x - basePoint.x, ay = point1.y - basePoint.y;
			float bx = point2.x - basePoint.x, by = point2.y - basePoint.y;
			return (ax * by - ay * bx) > 0;
		}

		// 计算第四个点
		Point CalForthPoint(const Point& poi0, const Point& poi1, const Point& poi2) {
			return Point(poi2.x + poi1.x - poi0.x, poi2.y + poi1.y - poi0.y);
		}

		// 计算扩展向量
		pair<float, float> CalExtendVec(const Point2f& poi0, const Point2f& poi1, const Point2f& poi2, float bias) {
			float dis0 = distance(poi0, poi1), dis1 = distance(poi0, poi2);
			float rate = dis1 / dis0;
			float x1 = poi0.x - poi2.x, y1 = poi0.y - poi2.y;
			float x2 = (poi0.x - poi1.x) * rate, y2 = (poi0.y - poi1.y) * rate;
			float totx = x1 + x2, toty = y1 + y2, distot = sqrt(totx * totx + toty * toty);
			return { totx / distot * bias, toty / distot * bias };
		}

	}
	// 验证三个定位点是否构成有效的二维码定位点组合
	bool IsValidQrTriple(const helpFunction::ParseInfo& p1, const helpFunction::ParseInfo& p2, const helpFunction::ParseInfo& p3) {
		// 计算三个定位点之间的距离
		float d12 = helpFunction::distance(p1.Center, p2.Center);
		float d13 = helpFunction::distance(p1.Center, p3.Center);
		float d23 = helpFunction::distance(p2.Center, p3.Center);

		// 找出最长的边（应该是两个定位点之间的距离）
		float maxDist = std::max({ d12, d13, d23 });
		float minDist = std::min({ d12, d13, d23 });

		// 三个定位点不能太近也不能太远
		if (minDist < 20) return false; // 太近
		if (maxDist > 500) return false; // 太远

		// 两个较短边应该大致相等（等腰直角三角形）
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

		// 两个短边比例应该在合理范围内（0.7-1.4）
		if (ratio1 < 0.7f || ratio1 > 1.4f) return false;
		if (ratio2 < 0.7f || ratio2 > 1.4f) return false;

		// 验证角度：应该接近等腰直角三角形
		// 使用余弦定理计算角度
		float a = d12, b = d13, c = d23;
		float angle1 = acos((b * b + c * c - a * a) / (2 * b * c)) * 180.0f / CV_PI;
		float angle2 = acos((a * a + c * c - b * b) / (2 * a * c)) * 180.0f / CV_PI;
		float angle3 = acos((a * a + b * b - c * c) / (2 * a * b)) * 180.0f / CV_PI;

		// 应该有一个接近90度的角，另外两个接近45度
		bool hasRightAngle = (angle1 > 70 && angle1 < 110) ||
			(angle2 > 70 && angle2 < 110) ||
			(angle3 > 70 && angle3 < 110);

		if (!hasRightAngle) return false;

		// 检查三个定位点的大小是否相近
		float size1 = p1.Rect.size.width * p1.Rect.size.height;
		float size2 = p2.Rect.size.width * p2.Rect.size.height;
		float size3 = p3.Rect.size.width * p3.Rect.size.height;

		float avgSize = (size1 + size2 + size3) / 3.0f;
		float sizeRatio1 = size1 / avgSize;
		float sizeRatio2 = size2 / avgSize;
		float sizeRatio3 = size3 / avgSize;

		// 三个定位点大小应该相近（比例在0.5-2.0之间）
		if (sizeRatio1 < 0.5f || sizeRatio1 > 2.0f) return false;
		if (sizeRatio2 < 0.5f || sizeRatio2 > 2.0f) return false;
		if (sizeRatio3 < 0.5f || sizeRatio3 > 2.0f) return false;

		return true;
	}

	// 调整定位点顺序
	void AdjustPointsOrder(vector<vector<Point>>& src3Points) {
		vector<vector<Point>> temp;
		Point p3[3] = { helpFunction::CalRectCenter(src3Points[0]),
						helpFunction::CalRectCenter(src3Points[1]),
						helpFunction::CalRectCenter(src3Points[2]) };
		int index[3][3] = { { 0,1,2 },{1,0,2},{2,0,1} };
		for (int i = 0; i < 3; i++) {
			if (helpFunction::isRightAngle(helpFunction::Cal3PointAngle(p3[index[i][0]], p3[index[i][1]], p3[index[i][2]]))) {
				temp.push_back(std::move(src3Points[index[i][0]]));     // 左上角的点位于0号
				if (helpFunction::IsClockwise(p3[index[i][0]], p3[index[i][1]], p3[index[i][2]])) {
					temp.push_back(std::move(src3Points[index[i][1]])); // 右上角的点位于1号
					temp.push_back(std::move(src3Points[index[i][2]])); // 左下角的点位于2号
				}
				else {
					temp.push_back(std::move(src3Points[index[i][2]])); // 右上角的点位于1号
					temp.push_back(std::move(src3Points[index[i][1]])); // 左下角的点位于2号
				}
				for (int i = 3; i < src3Points.size(); ++i)
					temp.push_back(std::move(src3Points[i]));  // 移动其他的点
				std::swap(temp, src3Points);
				return;
			}
		}
		return;
	}

	// 过滤多余的定位点
	bool DumpExcessQrPoint(vector<vector<Point>>& qrPoints) {
		// 排序后计算面积存在直角的方差最接近的三个点
		sort(
			qrPoints.begin(), qrPoints.end(),
			[](const vector<Point>& a, const vector<Point>& b) { return a.size() < b.size(); }
		);
		// 按面积排序
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
		// 如果pos==-1，则按大小排序后不存在夹角90度左右的识别点。
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

	//预处理
	Mat preprocessImg(const Mat& srcImg, float blurRate) {
		Mat tempImg;
		// 1. 灰度化
		cvtColor(srcImg, tempImg, COLOR_BGR2GRAY);
		// 2. 模糊处理，减少高频信息干扰
		float BlurSize = 1.0 + srcImg.rows * blurRate;
		blur(tempImg, tempImg, Size2f(BlurSize, BlurSize));
		// 3. 二值化（OTSU算法自动阈值）
		threshold(tempImg, tempImg, 0, 255, THRESH_BINARY | THRESH_OTSU);
		return tempImg;
	}

	//定位点筛选
	bool findPositionPoints(const Mat& binaryImg, vector<vector<Point>>& qrPoints) {
		//检测轮廓
		vector<vector<Point>> contours;
		vector<Vec4i> hierarchy;
		findContours(binaryImg, contours, hierarchy, RETR_TREE, CHAIN_APPROX_NONE, Point(0, 0));

		//通过黑色定位角作为父轮廓，有两个子轮廓的特点，筛选出定位角
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
				//保存找到的定位角
				if (isQrPoint) {
					qrPoints.push_back(contours[parentIdx]);
				}
				ic = 0;
				parentIdx = -1;
			}
		}

		return qrPoints.size() < 3;
	}

	//计算第四个点
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
		vector<Point2f> result; //左上开始顺时针
		result.push_back(topLeft);
		result.push_back(topRight);
		result.push_back(bottomRight);
		result.push_back(bottomLeft);
		return result;
	}

	//透视变换
	Mat cropParallelRect(const Mat& srcImg, const vector<Point2f>& srcPoints) {
		vector<Point2f> dstPoints = {
			Point2f(0,0),
			Point2f(107,0),
			Point2f(107,107),
			Point2f(0,107)
		};

		Mat M = getPerspectiveTransform(srcPoints, dstPoints);
		Mat dstImg;
		warpPerspective(srcImg, dstImg, M, Size(108, 108));
		return dstImg;
	}

	bool Main(const Mat& srcImg, Mat& dstImg) {
		// 五种模糊率设置
		vector<vector<Point>> qrPointsTemp;
		std::array<float, 5> ar = { 0.0005, 0.0000, 0.00025, 0.001, 0.0001 };

		for (auto& rate : ar) { // 尝试不同的模糊率
			// 图像预处理,然后扫描定位点
			Mat binaryImg = preprocessImg(srcImg, rate);
			if (!findPositionPoints(binaryImg, qrPointsTemp)) {
				if (qrPointsTemp.size() >= 4 && !DumpExcessQrPoint(qrPointsTemp)) {
					// 调整定位点顺序
					AdjustPointsOrder(qrPointsTemp);

					// 提取中心点
					vector<Point2f> points;
					for (const auto& contour : qrPointsTemp) {
						points.push_back(helpFunction::CalRectCenter(contour));
					}

					// 计算第四个点并调整
					vector<Point2f> adjusted = adjustPositionPoints(points);

					// 透视变换
					dstImg = cropParallelRect(srcImg, adjusted);
					return !dstImg.empty();
				}
			}
		}

		return false;
	}
}