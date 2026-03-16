#pragma once
#include<cstdio>
#include<opencv2/opencv.hpp>

namespace ImgPraseV2 {
	using namespace cv;
	using namespace std;

	namespace helpFunction {
		struct ParseInfo;
		Point2f CalRectCenter(const vector<Point>& pointSet);
		float distance(const Point2f& a, const Point2f& b);
		float Cal3PointAngle(const Point& point0, const Point& point1, const Point& point2);
		bool isRightAngle(float angle);
		bool IsQrBWRateLegal(const float rate);
		bool BWRatePreprocessing(Mat& image, vector<int>& vValueCount);
		bool IsQrBWRateXLegal(Mat& image);
		bool IsQrBWRate(Mat& image);
		bool IsQrSizeLegal(const Size2f& qrSize, const Size2f& imgSize);
		Mat CropRect(const Mat& srcImg, const RotatedRect& rotatedRect);
		bool IsQrPoint(const vector<Point>& contour, const Mat& img);
		bool isRightAngleExist(const Point& point0, const Point& point1, const Point& point2);
		double Cal3NumVariance(const int a, const int b, const int c);
		bool IsClockwise(const Point& basePoint, const Point& point1, const Point& point2);
		Point CalFourthPoint(const Point& point0, const Point& point1, const Point& point2);
		pair<float, float> CalExtendVec(const Point2f& point0, const Point2f& point1, const Point2f& point2, float bias);
	}

	Mat preprocessImgV2_OTSU(const Mat& srcImg, float blurRate = 0.001);
	Mat preprocessImgV2_Adaptive(const Mat& srcImg, int blockSize = 11, double C = 2.0);
	Mat preprocessImgV2_Morphology(const Mat& srcImg);
	Mat preprocessImgV2_Combined(const Mat& srcImg);

	bool findPositionPoints(const Mat& binaryImg, vector<vector<Point>>& qrPoint);
	vector<Point2f> adjustPositionPoints(const vector<Point2f>& positionPoints);
	Mat cropParallelRect(const Mat& srcImg, const vector<Point2f>& srcPoints);
	bool Main(const Mat& srcImg, Mat& dstImg);
}
