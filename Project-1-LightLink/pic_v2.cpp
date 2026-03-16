#include "pic_v2.h"

namespace ImgPraseV2 {

	constexpr float MinRightAngle = 75.0;
	constexpr float MaxRightAngle = 105.0;
	constexpr float MaxQRBWRate = 2.25;
	constexpr float MinQRBWRate = 0.40;
	constexpr int MinQRSize = 10;
	constexpr float MaxQRScale = 0.3;
	constexpr float MinQRXYRate = 5.0 / 6.0;
	constexpr float MaxQRXYRate = 6.0 / 5.0;
	constexpr float MaxQrTypeRate = 2.5;
	constexpr float minQrTypeRate = 1.7;
	constexpr float MaxDistanceRate = 1.2;
	constexpr float minDistanceRate = 0.8;

	Mat Rotation_90(const Mat& srcImg) {
		Mat tempImg;
		transpose(srcImg, tempImg);
		flip(tempImg, tempImg, 1);
		return tempImg;
	}

	namespace helpFunction {

		struct ParseInfo {
			Point2f Center;
			int size;
			RotatedRect Rect;
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

		bool IsQrBWRateXLegal(Mat& image) {
			vector<int> vValueCount;
			if (!BWRatePreprocessing(image, vValueCount)) return false;

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
				Point2f(0,rect.size.height - 1),
				Point2f(0,0),
				Point2f(rect.size.width - 1,0),
				Point2f(rect.size.width - 1, rect.size.height - 1) };
			auto M = getPerspectiveTransform(srcPoints, disPoints);
			warpPerspective(image, disImg, M, rect.size);
			return disImg;
		}

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

		bool IsQrPoint(const vector<Point>& contour, const Mat& img) {
			RotatedRect rotatedRect = minAreaRect(contour);
			Mat cropImg = CropRect(img, rotatedRect);
			if (!IsQrSizeLegal(rotatedRect.size, img.size())) return false;
			return IsQrBWRate(cropImg);
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

		bool IsClockwise(const Point& basePoint, const Point& point1, const Point& point2) {
			float ax = point1.x - basePoint.x, ay = point1.y - basePoint.y;
			float bx = point2.x - basePoint.x, by = point2.y - basePoint.y;
			return (ax * by - ay * bx) > 0;
		}

		Point CalForthPoint(const Point& poi0, const Point& poi1, const Point& poi2) {
			return Point(poi2.x + poi1.x - poi0.x, poi2.y + poi1.y - poi0.y);
		}

		pair<float, float> CalExtendVec(const Point2f& poi0, const Point2f& poi1, const Point2f& poi2, float bias) {
			float dis0 = distance(poi0, poi1), dis1 = distance(poi0, poi2);
			float rate = dis1 / dis0;
			float x1 = poi0.x - poi2.x, y1 = poi0.y - poi2.y;
			float x2 = (poi0.x - poi1.x) * rate, y2 = (poi0.y - poi1.y) * rate;
			float totx = x1 + x2, toty = y1 + y2, distot = sqrt(totx * totx + toty * toty);
			return { totx / distot * bias, toty / distot * bias };
		}

	}

	Mat preprocessImgV2_OTSU(const Mat& srcImg, float blurRate) {
		Mat tempImg;
		cvtColor(srcImg, tempImg, COLOR_BGR2GRAY);

		float BlurSize = 1.0f + srcImg.rows * blurRate;
		if (BlurSize < 1.0f) BlurSize = 1.0f;
		blur(tempImg, tempImg, Size2f(BlurSize, BlurSize));

		threshold(tempImg, tempImg, 0, 255, THRESH_BINARY | THRESH_OTSU);
		return tempImg;
	}

	Mat preprocessImgV2_Adaptive(const Mat& srcImg, int blockSize, double C) {
		Mat grayImg;
		cvtColor(srcImg, grayImg, COLOR_BGR2GRAY);

		GaussianBlur(grayImg, grayImg, Size(5, 5), 0);

		Mat adaptiveImg;
		adaptiveThreshold(grayImg, adaptiveImg, 255,
			ADAPTIVE_THRESH_GAUSSIAN_C, THRESH_BINARY,
			blockSize, C);

		return adaptiveImg;
	}

	Mat preprocessImgV2_Morphology(const Mat& srcImg) {
		Mat morphImg;
		Mat kernel = getStructuringElement(MORPH_RECT, Size(3, 3));

		morphologyEx(srcImg, morphImg, MORPH_CLOSE, kernel);
		morphologyEx(morphImg, morphImg, MORPH_OPEN, kernel);

		return morphImg;
	}

	Mat preprocessImgV2_Combined(const Mat& srcImg) {
		Mat grayImg;
		cvtColor(srcImg, grayImg, COLOR_BGR2GRAY);

		GaussianBlur(grayImg, grayImg, Size(3, 3), 0);

		Mat result1, result2;
		threshold(grayImg, result1, 0, 255, THRESH_BINARY | THRESH_OTSU);

		adaptiveThreshold(grayImg, result2, 255,
			ADAPTIVE_THRESH_GAUSSIAN_C, THRESH_BINARY,
			15, 5);

		Mat kernel = getStructuringElement(MORPH_RECT, Size(3, 3));
		morphologyEx(result1, result1, MORPH_CLOSE, kernel);
		morphologyEx(result2, result2, MORPH_CLOSE, kernel);

		Mat combined;
		bitwise_and(result1, result2, combined);

		return combined;
	}

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

	bool findPositionPoints(const Mat& binaryImg, vector<vector<Point>>& qrPoints) {
		vector<vector<Point>> contours;
		vector<Vec4i> hierarchy;
		findContours(binaryImg, contours, hierarchy, RETR_TREE, CHAIN_APPROX_NONE, Point(0, 0));

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

		if (qrPoints.size() >= 3) {
			return true;
		}

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

		Mat gray;
		cvtColor(dstImg, gray, COLOR_BGR2GRAY);
		threshold(gray, gray, 127, 255, THRESH_BINARY);

		int top = 0, bottom = 107, left = 0, right = 107;
		for (int y = 0; y < 108; y++) {
			bool hasBlack = false;
			for (int x = 0; x < 108; x++) {
				if (gray.at<uchar>(y, x) < 128) {
					hasBlack = true;
					break;
				}
			}
			if (hasBlack) { top = y; break; }
		}
		for (int y = 107; y >= 0; y--) {
			bool hasBlack = false;
			for (int x = 0; x < 108; x++) {
				if (gray.at<uchar>(y, x) < 128) {
					hasBlack = true;
					break;
				}
			}
			if (hasBlack) { bottom = y; break; }
		}
		for (int x = 0; x < 108; x++) {
			bool hasBlack = false;
			for (int y = top; y <= bottom; y++) {
				if (gray.at<uchar>(y, x) < 128) {
					hasBlack = true;
					break;
				}
			}
			if (hasBlack) { left = x; break; }
		}
		for (int x = 107; x >= 0; x--) {
			bool hasBlack = false;
			for (int y = top; y <= bottom; y++) {
				if (gray.at<uchar>(y, x) < 128) {
					hasBlack = true;
					break;
				}
			}
			if (hasBlack) { right = x; break; }
		}

		int margin = 2;
		top = max(0, top - margin);
		bottom = min(107, bottom + margin);
		left = max(0, left - margin);
		right = min(107, right + margin);

		int w = right - left;
		int h = bottom - top;
		int size = max(w, h);
		size = min(size, 108 - max(top, left));

		if (size > 10) {
			Mat cropped = dstImg(Rect(left, top, size, size));
			resize(cropped, dstImg, Size(108, 108));
		}

		return dstImg;
	}

	bool Main(const Mat& srcImg, Mat& dstImg) {
		vector<vector<Point>> qrPoints;
		float blurRates[] = { 0.0005f, 0.0000f, 0.00025f, 0.001f, 0.0001f };

		for (int i = 0; i < 5; i++) {
			Mat binaryImg = preprocessImgV2_OTSU(srcImg, blurRates[i]);
			Mat morphImg;
			morphologyEx(binaryImg, morphImg, MORPH_CLOSE, getStructuringElement(MORPH_RECT, Size(3, 3)));
			morphologyEx(morphImg, morphImg, MORPH_OPEN, getStructuringElement(MORPH_RECT, Size(2, 2)));

			qrPoints.clear();
			if (findPositionPoints(morphImg, qrPoints)) {
				if (qrPoints.size() >= 3 && !DumpExcessQrPoint(qrPoints)) {
					AdjustPointsOrder(qrPoints);

					vector<Point2f> points;
					for (const auto& contour : qrPoints) {
						points.push_back(helpFunction::CalRectCenter(contour));
					}

					vector<Point2f> adjusted = adjustPositionPoints(points);
					Mat cropped1 = cropParallelRect(srcImg, adjusted);

					if (!cropped1.empty()) {
						qrPoints.clear();
						float cropBlurRates[] = { 0.001f, 0.0005f };
						Mat cropped2;
						for (int j = 0; j < 2; j++) {
							Mat bin2 = preprocessImgV2_OTSU(cropped1, cropBlurRates[j]);
							qrPoints.clear();
							if (findPositionPoints(bin2, qrPoints)) {
								if (qrPoints.size() >= 3 && !DumpExcessQrPoint(qrPoints)) {
									AdjustPointsOrder(qrPoints);
									vector<Point2f> points2;
									for (const auto& contour : qrPoints) {
										points2.push_back(helpFunction::CalRectCenter(contour));
									}
									vector<Point2f> adjusted2 = adjustPositionPoints(points2);
									cropped2 = cropParallelRect(cropped1, adjusted2);
									break;
								}
							}
						}

						if (!cropped2.empty()) {
							resize(cropped2, cropped2, Size(1080, 1080));
							Mat gray;
							cvtColor(cropped2, gray, COLOR_BGR2GRAY);
							Mat bin;
							threshold(gray, bin, 127, 255, THRESH_BINARY);

							int top = 0, bottom = 1079, left = 0, right = 1079;
							for (int y = 0; y < 1080; y++) {
								bool hasBlack = false;
								for (int x = 0; x < 1080; x++) {
									if (bin.at<uchar>(y, x) < 128) { hasBlack = true; break; }
								}
								if (hasBlack) { top = y; break; }
							}
							for (int y = 1079; y >= 0; y--) {
								bool hasBlack = false;
								for (int x = 0; x < 1080; x++) {
									if (bin.at<uchar>(y, x) < 128) { hasBlack = true; break; }
								}
								if (hasBlack) { bottom = y; break; }
							}
							for (int x = 0; x < 1080; x++) {
								bool hasBlack = false;
								for (int y = top; y <= bottom; y++) {
									if (bin.at<uchar>(y, x) < 128) { hasBlack = true; break; }
								}
								if (hasBlack) { left = x; break; }
							}
							for (int x = 1079; x >= 0; x--) {
								bool hasBlack = false;
								for (int y = top; y <= bottom; y++) {
									if (bin.at<uchar>(y, x) < 128) { hasBlack = true; break; }
								}
								if (hasBlack) { right = x; break; }
							}

							int margin = 10;
							top = max(0, top - margin);
							bottom = min(1079, bottom + margin);
							left = max(0, left - margin);
							right = min(1079, right + margin);

							int size = max(right - left, bottom - top);
							if (size > 50 && size <= 1080) {
								Mat finalCrop = cropped2(Rect(left, top, size, size));
								resize(finalCrop, dstImg, Size(108, 108));
								return true;
							}
						}

						resize(cropped1, dstImg, Size(108, 108));
						return true;
					}
				}
			}
		}

		return false;
	}
}
