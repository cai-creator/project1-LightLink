#define _CRT_SECURE_NO_WARNINGS
#include "codeTool.h"
#include "ffmpegTool.h"
#include "decodeTool.h"
#include "pic.h"
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

#define _DECODE // 解码模式改成：   _DECODE，编码模式：_ENCODE
using namespace code;
using namespace decode;
using namespace ImgPraseV2;
using namespace cv;
std::string fix_path(const std::string& input_path) {
	std::string fixed = input_path;

	std::replace(fixed.begin(), fixed.end(), '/', '\\');
	return fixed;
}

void encodeEXE(std::string source_path, std::string target_path) {
	source_path = fix_path(getRootdir_impl() + "\\" + source_path);
	target_path = fix_path(getRootdir_impl() + "\\" + target_path);
	std::string mid = fix_path(getRootdir_impl()) + "\\mid_operation";


	std::cout << source_path << std::endl;
	std::cout << target_path << std::endl;
	std::cout << mid << std::endl;

	Mat source = imread(source_path);
	std::cout << "Source image: " << source.cols << "x" << source.rows
		<< ", channels=" << source.channels()
		<< ", total=" << source.total()
		<< ", elemSize=" << source.elemSize() << std::endl;
	//if (source.empty()) std::cout << "empty" << std::endl;
	//else {
	//	imshow("out", source);
	//	waitKey(0);
	//}

	// 准备包含图像尺寸信息的数据
	int width = source.cols;
	int height = source.rows;
	int dataSize = source.total() * source.elemSize();
	
	// 分配内存存储尺寸信息和图像数据
	char* dataWithSize = new char[8 + dataSize];
	// 保存宽度和高度信息（前4字节是宽度，接下来4字节是高度）
	*((int*)dataWithSize) = width;
	*((int*)(dataWithSize + 4)) = height;
	// 复制图像数据
	memcpy(dataWithSize + 8, source.data, dataSize);
	
	// 编码包含尺寸信息的数据
	SaveMutiFrame(dataWithSize, mid.c_str(), 8 + dataSize, width);
	
	// 释放内存
	delete[] dataWithSize;
	int fps = 20; // 设置视频帧率为20
	picTovideo_impl((mid + "/frame%1d.jpg").c_str(), target_path, fps, 1080);


}
void decodeEXE(std::string video_path, std::string output_path) {
	video_path = fix_path(getRootdir_impl() + "\\" + video_path);
	std::string mid = fix_path(getRootdir_impl()) + "\\mid_operation";

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
	std::cout << "\nStep 2: Preprocessing frames..." << std::endl;

	// 步骤3: 遍历处理后的图片，解码每帧
	std::cout << "\nStep 3: Decoding frames..." << std::endl;

	std::map<uint16_t, ImageInfo> frameMap;
	int decode_fail_count = 0;
	int decode_success_count = 0;
	int valid_qr_count = 0;

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
		Mat processedFrame;
		std::string debugPath = mid + "\\debug_frame" + std::to_string(i);
		bool processed = Main(frame, processedFrame, debugPath);
		
		// 先尝试使用处理后的帧
		if (processed) {
			valid_qr_count++;
			std::cout << "Frame " << i << ": Using processed frame for decoding" << std::endl;

			// 直接使用1080x1080图像进行解码
			Mat resizedFrame = processedFrame;

			// 保存1080x1080图像
			if (!resizedFrame.empty()) {
					std::string resizedPath = mid + "\\resized_frame" + std::to_string(i) + ".png";
					imwrite(resizedPath, resizedFrame);
				}

			// 解码单帧
			ImageInfo info;
			if (decodeFrame(resizedFrame, info)) {
				decode_success_count++;
				frameMap[info.FrameBase] = info;

				std::cout << "Frame " << i << ": No." << info.FrameBase
					<< " Start=" << info.IsStart << " End=" << info.IsEnd
					<< " CheckCode=" << info.CheckCode;

				// 校验（暂时禁用）
				// if (verifyCheckCode(info)) {
				std::cout << " [OK]" << std::endl;
				// }
				// else {
				// 	std::cout << " [CHECK FAILED]" << std::endl;
				// }
			} else {
				// 处理后的帧解码失败，直接标记为失败
				decode_fail_count++;
				std::cerr << "Frame " << i << ": Decode failed (Invalid frame flag)" << std::endl;
			}
		} else {
			// 预处理失败，跳过此帧
			decode_fail_count++;
			std::cerr << "Frame " << i << ": Preprocessing failed, skipping frame" << std::endl;
		}
	}
	std::cout << "Step 3: Decode success: " << decode_success_count << ", Failed: " << decode_fail_count << std::endl;
	std::cout << "Total frames processed: " << (decode_success_count + decode_fail_count) << std::endl;
	std::cout << "Valid QR frames found: " << valid_qr_count << std::endl;

	// 步骤4: 合并所有帧数据
	std::cout << "\nStep 4: Merging frames..." << std::endl;

	std::vector<unsigned char> outputData;

	// 按帧编号顺序合并数据
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
	// 从起始帧中获取原始图像的尺寸
	// 查找起始帧
	int DECODED_WIDTH = 1024; // 默认宽度
	int DECODED_HEIGHT = 574; // 默认高度
	bool foundStartFrame = false;
	
	for (auto& pair : frameMap) {
		ImageInfo& info = pair.second;
		if (info.IsStart) {
			// 从起始帧的Info数据中提取原始图像尺寸
			// 假设前4字节是宽度，接下来4字节是高度
			if (info.Info.size() >= 8) {
				DECODED_WIDTH = *((int*)info.Info.data());
				DECODED_HEIGHT = *((int*)(info.Info.data() + 4));
				foundStartFrame = true;
				std::cout << "Found start frame with image size: " << DECODED_WIDTH << "x" << DECODED_HEIGHT << std::endl;
				break;
			}
		}
	}
	
	if (!foundStartFrame) {
		std::cout << "Warning: Start frame not found, using default image size: " << DECODED_WIDTH << "x" << DECODED_HEIGHT << std::endl;
	}
	
	const int CHANNELS = 3;
	const int EXPECTED_SIZE = DECODED_WIDTH * DECODED_HEIGHT * CHANNELS;

	output_path = fix_path(getRootdir_impl() + "\\" + output_path);

	// 跳过前8字节的尺寸信息，使用实际的图像数据
	const size_t sizeInfoOffset = 8;
	std::vector<unsigned char> actualImageData;
	
	if (outputData.size() > sizeInfoOffset) {
		actualImageData.assign(outputData.begin() + sizeInfoOffset, outputData.end());
	} else {
		actualImageData = outputData;
		std::cout << "Warning: No size information found in data" << std::endl;
	}

	// 只取原始图片大小的数据（忽略填充）
	int saveSize = std::min((int)actualImageData.size(), EXPECTED_SIZE);

	// 检查数据完整性
	if (actualImageData.size() < EXPECTED_SIZE) {
		std::cout << "Warning: Insufficient data. Expected " << EXPECTED_SIZE << " bytes, got " << actualImageData.size() << " bytes." << std::endl;
		// 填充剩余数据为黑色
		actualImageData.resize(EXPECTED_SIZE, 0);
		saveSize = EXPECTED_SIZE;
	}

	// 验证数据是否有效
	bool dataValid = true;
	int zeroCount = 0;
	for (size_t i = 0; i < std::min((size_t)1000, actualImageData.size()); i++) {
		if (actualImageData[i] == 0) zeroCount++;
	}
	if (zeroCount > 900) {
		std::cout << "Warning: Too many zero bytes in data, possible decoding error." << std::endl;
		dataValid = false;
	}

	// 显示数据统计信息
	int minVal = 255, maxVal = 0;
	for (size_t i = 0; i < std::min((size_t)1000, actualImageData.size()); i++) {
		if (actualImageData[i] < minVal) minVal = actualImageData[i];
		if (actualImageData[i] > maxVal) maxVal = actualImageData[i];
	}
	std::cout << "Data range: " << minVal << " - " << maxVal << std::endl;

	// 创建与编码时相同尺寸的图像
	Mat decodedImg(DECODED_HEIGHT, DECODED_WIDTH, CV_8UC3);
	memcpy(decodedImg.data, actualImageData.data(), saveSize);

	// 直接保存，不需要通道转换
	if (imwrite(output_path, decodedImg)) {
		std::cout << "Image saved to: " << output_path << std::endl;
		if (!dataValid) {
			std::cout << "Note: Image saved but data may be corrupted." << std::endl;
		}
	}
	else {
		std::cerr << "Failed to save image" << std::endl;
	}
}


int main(int argc, char* argv[]) {
	std::string source_path, target_path;

	// 从命令行参数获取路径
	if (argc >= 3) {
		source_path = argv[1];
		target_path = argv[2];
		std::cout << "Using command line arguments: " << std::endl;
		std::cout << "Source path: " << source_path << std::endl;
		std::cout << "Target path: " << target_path << std::endl;
	} else {
		// 手动输入路径
		std::cout << "Please enter paths manually:" << std::endl;
		std::cout << "Enter source path: ";
		std::getline(std::cin, source_path);
		std::cout << "Enter target path: ";
		std::getline(std::cin, target_path);
	}

#ifdef _ENCODE
	encodeEXE(source_path, target_path);

#elif defined(_DECODE)
	decodeEXE(source_path, target_path);
#else
	std::cerr << "Error: Please define either _ENCODE or _DECODE before compiling." << std::endl;

#endif
	return 0;
}
