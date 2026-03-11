#include <pic.h>
namespace ImgPrase {
	
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
}