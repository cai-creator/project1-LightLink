#include "codeTool.h"
#include "ffmpegTool.h"
#include<ctime>
#include<cstdlib>
#include <iostream>
#include <map>
#include <iomanip>
#include <sstream>

#define DEBUG 1

#if DEBUG
#define DEBUG_PRINT(msg) std::cerr << "[DEBUG] " << msg << std::endl
#else
#define DEBUG_PRINT(msg)
#endif

// 统计校验失败的个数
#define CHECK_FAIL_COUNT() static int check_fail_count = 0; check_fail_count++;

#define _DECODE // 解码模式改成：   _DECODE，编码模式：_ENCODE
using namespace code;
using namespace cv;
std::string fix_path(const std::string& input_path) {
	std::string fixed = input_path;

	std::replace(fixed.begin(), fixed.end(), '/', '\\');
	return fixed;
}

void encodeEXE(std::string source_path, std::string target_path) {
	source_path = fix_path(getRootdir_impl() + "\\" + source_path);
	target_path = fix_path(getRootdir_impl() +"\\" +  target_path);
	std::string mid = fix_path(getRootdir_impl()) + "\\mid_operation";


	std::cout << source_path << std::endl;
	std::cout << target_path << std::endl;
	std::cout << mid << std::endl;

	Mat source = imread(source_path);
	//if (source.empty()) std::cout << "empty" << std::endl;
	//else {
	//	imshow("out", source);
	//	waitKey(0);
	//}
	
	SaveMutiFrame((char*)source.data, mid.c_str(), source.total() * source.elemSize());
	picTovideo_impl((mid+"\\frame%d.jpg").c_str(), target_path, 30, 1080);
	
	
}
void decodeEXE(std::string video_path, std::string output_path) {
	video_path = fix_path(getRootdir_impl() + "\\" + video_path);
	std::string mid = fix_path(getRootdir_impl()) + "\\mid_operation";
	
	// 创建预处理图片保存目录
	std::string preprocessDir = mid + "\\preprocess";
	std::string grayDir = preprocessDir + "\\gray";
	std::string binaryDir = preprocessDir + "\\binary";
	std::string denoiseDir = preprocessDir + "\\denoise";
	std::string cropDir = preprocessDir + "\\crop";
	
	// 创建目录
	system(("mkdir " + preprocessDir).c_str());
	system(("mkdir " + grayDir).c_str());
	system(("mkdir " + binaryDir).c_str());
	system(("mkdir " + denoiseDir).c_str());
	system(("mkdir " + cropDir).c_str());

	std::cout << "Input video: " << video_path << std::endl;
	std::cout << "Output path: " << output_path << std::endl;

	// 清空 mid_operation 文件夹
	std::cout << "Cleaning mid_operation folder..." << std::endl;
	std::string clean_cmd = "del /Q " + mid + "\\*.*";
	system(clean_cmd.c_str());

	// 步骤1: 视频拆帧
	std::cout << "\nStep 1: Video to frames..." << std::endl;
	VideotoPic(video_path.c_str(), (mid + "\\frame").c_str(), "jpg");

	// 步骤2: 图像预处理
	std::cout << "\nStep 2: Image preprocessing..." << std::endl;

	// 步骤3: 遍历处理后的图片，解码每帧
	std::cout << "\nStep 3: Decoding frames..." << std::endl;

	std::map<uint16_t, ImageInfo> frameMap;
	int decode_fail_count = 0;
	int decode_success_count = 0;
	int check_fail_count = 0;

	// 遍历mid_operation目录下的所有帧图片
	for (int i = 1; ; i++) {
		std::ostringstream oss;
		oss << mid << "\\frame" << std::setw(4) << std::setfill('0') << i << ".jpg";
		std::string framePath = oss.str();
		Mat frame = imread(framePath);

		if (frame.empty()) {
			break;
		}

		// 图像预处理
		Mat grayFrame;
		cvtColor(frame, grayFrame, COLOR_BGR2GRAY);
		
		Mat binaryFrame = ImgPraseV2::preprocessImgV2_OTSU(frame);
		Mat denoiseFrame = ImgPraseV2::preprocessImgV2_Morphology(binaryFrame);
		
		Mat cropFrame;
		bool cropSuccess = ImgPraseV2::Main(frame, cropFrame);
		
		// 保存预处理图片
		std::ostringstream filename;
		filename << std::setw(4) << std::setfill('0') << i << ".jpg";
		
		imwrite(grayDir + "\\" + filename.str(), grayFrame);
		imwrite(binaryDir + "\\" + filename.str(), binaryFrame);
		imwrite(denoiseDir + "\\" + filename.str(), denoiseFrame);
		if (cropSuccess) {
			imwrite(cropDir + "\\" + filename.str(), cropFrame);
		}

		// 使用二值化后的图像进行解码
		Mat processFrame = binaryFrame;
		
		// 确保是三通道图像
		if (processFrame.channels() == 1) {
			cvtColor(processFrame, processFrame, COLOR_GRAY2BGR);
		}
		
		// 确保尺寸正确
		if (processFrame.cols != FrameSize || processFrame.rows != FrameSize) {
			resize(processFrame, processFrame, Size(FrameSize, FrameSize));
		}

		// 解码单帧
		ImageInfo info;
		if (decodeFrame(processFrame, info)) {
			decode_success_count++;
			frameMap[info.FrameBase] = info;

			std::cout << "Frame " << i << ": No." << info.FrameBase
				<< " Start=" << info.IsStart << " End=" << info.IsEnd
				<< " CheckCode=" << info.CheckCode;

			// 校验
			if (verifyCheckCode(info)) {
				std::cout << " [OK]" << std::endl;
			}
			else {
				std::cout << " [CHECK FAILED]" << std::endl;
				check_fail_count++;
			}
		}
		else {
			decode_fail_count++;
			std::cerr << "Frame " << i << ": Decode failed (Invalid frame flag)" << std::endl;
		}
	}
	std::cout << "Step 3: Decode success: " << decode_success_count << ", Failed: " << decode_fail_count << std::endl;
	std::cout << "Check failed: " << check_fail_count << std::endl;

	// 步骤4: 合并所有帧数据
	std::cout << "\nStep 4: Merging frames..." << std::endl;

	std::vector<unsigned char> outputData;

	for (auto& pair : frameMap) {
		ImageInfo& info = pair.second;
		outputData.insert(outputData.end(), info.Info.begin(), info.Info.end());
	}

	// 调试：显示前16字节
	std::cout << "First 16 bytes of decoded data: ";
	for (size_t i = 0; i < std::min((size_t)16, outputData.size()); i++) {
		printf("%02X ", outputData[i]);
	}
	std::cout << std::endl;

	std::cout << "Total decoded data size: " << outputData.size() << " bytes" << std::endl;

	// 步骤5: 保存解码后的数据为PNG图片
	const int DECODED_WIDTH = 1024;
	const int DECODED_HEIGHT = 574;
	const int CHANNELS = 3;
	const int EXPECTED_SIZE = DECODED_WIDTH * DECODED_HEIGHT * CHANNELS;

	output_path = fix_path(getRootdir_impl() + "\\" + output_path);

	// 只取原始图片大小的数据（忽略填充）
	int saveSize = std::min((int)outputData.size(), EXPECTED_SIZE);

	Mat decodedImg(DECODED_HEIGHT, DECODED_WIDTH, CV_8UC3);
	memcpy(decodedImg.data, outputData.data(), saveSize);

	// 直接保存，不需要通道转换
	if (imwrite(output_path, decodedImg)) {
		std::cout << "Image saved to: " << output_path << std::endl;
	}
	else {
		std::cerr << "Failed to save image" << std::endl;
	}
}


int main(int argc, char* argv[])
{
//	if (argc != 3) {
//		std::cout << "Format error" << std::endl;
//	}
//
//	std::string source_path(argv[1]);
//	std::string target_path(argv[2]);
//	
//#ifdef _ENCODE
//	encodeEXE(source_path,target_path);
//
//#elif defined(_DECODE)
//	decodeEXE();
//#else
//	std::cerr << "Error: Please define either _ENCODE or _DECODE before compiling." << std::endl;
//   
//#endif

	uint16_t x = GetCheckCode(3, "ABC", 1, frameStyle::First);
	uint16_t a[16];
	for (int i = 0;i < 16;i++) {
		a[i] = x & 1;
		x >>= 1;
	}

	for (int i = 15;i >=0 ;i--) {
		std::cout << a[i];
	}
	return 0;
}