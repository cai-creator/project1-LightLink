#include <pic.h>
namespace ImgPrase {
	
	// 常量定义
	constexpr float MinRightAngle = 75.0;
	constexpr float MaxRightAngle = 105.0;
	constexpr float MaxQRBWRate = 2.25;//最大黑白比例
	constexpr float MinQRBWRate = 0.40;//最小黑白比例
	constexpr int MinQRSize = 10;//最小定位点尺寸
	constexpr float MaxQRScale = 0.25;//定位点占图像的最大比例
	constexpr float MinQRXYRate = 5.0 / 6.0;//最小长宽比
	constexpr float MaxQRXYRate = 6.0 / 5.0;//最大长宽比
	

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

		bool IsQrBWRateXLable(Mat& image){
			vector<int> vValueCount;
			if (!BWRatePreprocessing(image, vValueCount)) return false;

			int index = -1, maxCount = -1;
			//找到最多的块
			for (int i = 0; i < vValueCount.size(); i++) {
				if (i == 0) {
					index = i;
					maxCount = vValueCount[i];
				}
				else if(vValueCount[i] > maxCount){
					maxCount = vValueCount[i];
					index = i;
				}
			}
			//1:1:3:1:1，不会出现在最前面和最后面
			if (index < 2 || index > vValueCount.size() - 3) return false;

			float rate = (float)maxCount / 3.00;
			bool checkTag = true;
			for(int i = index - 2; i <= index + 2; i++) {
				if (i == index) continue;
				if (!IsQrBWRateLegal((float)vValueCount[i] / rate)) {
					checkTag = false;
					break;
				}
			}
			return checkTag;
		}

		bool IsQrBWRate(Mat& image){
			bool xTest = IsQrBWRateXLable(image);
			if (!xTest) return false;

			Mat imageT;
			transpose(image, imageT);
			flip(imageT, imageT, 1);
			bool yTest = IsQrBWRateXLable(imageT);
			return yTest;

		}

		bool IsQrSizeLegal(const Size2f& qrSize, const Size2f& imgSize) {
			float xYScale = qrSize.width / qrSize.height;
			if (qrSize.height < MinQRSize || qrSize.width < MinQRSize) return false;
			if (qrSize.width / imgSize.width > MaxQRScale || qrSize.height / imgSize.height > MaxQRScale) return false;
			if (xYScale < MinQRXYRate || xYScale > MaxQRXYRate) return false;
			return true;
		}

		Mat CropRect(const Mat& image, const RotatedRect& rect){
			Mat srcPoints, disImg;
			boxPoints(rect, srcPoints);
			vector<Point2f> disPoints ={
				Point2f(0,rect.size.height - 1),//左下
				Point2f(0,0),//左上
				Point2f(rect.size.width - 1,0),
				Point2f(rect.size.width - 1, rect.size.height - 1) };
			auto M = getPerspectiveTransform(srcPoints, disPoints);
			warpPerspective(image, disImg, M, rect.size);
			return disImg;
		}
		//验证定位点
		bool IsQrPoint(const vector<Point>& contour, const Mat& img) {
			RotatedRect rotatedRect = minAreaRect(contour);//最小包围矩形
			Mat cropImg = CropRect(img, rotatedRect);
			if (!IsQrSizeLegal(rotatedRect.size, img.size())) return false;
			return true;
		}

		bool isRightAngleExist(const Point& point0, const Point& point1, const Point& point2) {
			return isRightAngle(Cal3PointAngle(point0, point1, point2)) ||
				isRightAngle(Cal3PointAngle(point1, point0, point2)) ||
				isRightAngle(Cal3PointAngle(point2, point0, point1));
		}

		double Cal3NumVariance(const int a, const int b, const int c){
			double avg = (a + b + c) / 3.0;
			return ((a - avg) * (a - avg) + (b - avg) * (b - avg) + (c - avg) * (c - avg)) / 3.0;
		}

		//true表示顺时针。
		bool IsClockWise(const Point& basePoint, const Point& point1, const Point& point2){
			float ax = point1.x - basePoint.x, ay = point1.y - basePoint.y;
			float bx = point2.x - basePoint.x, by = point2.y - basePoint.y;
			return (ax * by - ay * bx) > 0;
		}


	}
	//预处理
	Mat preprocessImg(const Mat& srcImg,float blurRate) {
		Mat tempImg;
		// 1. 灰度化
		cvtColor(srcImg, tempImg, COLOR_BGR2GRAY);
		// 2. 中值滤波
		float blurSize = 1.0 + blurRate * srcImg.cols;
		int kernelSize = (int)blurSize % 2 == 0 ? (int)blurSize + 1 :(int)blurSize;
		blur(tempImg, tempImg, Size(kernelSize, kernelSize));
		// 3. 二值化
		threshold(tempImg, tempImg, 0, 255, THRESH_BINARY | THRESH_OTSU);
		return tempImg;
	}

	//定位点筛选

}