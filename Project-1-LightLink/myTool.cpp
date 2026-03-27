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

	SaveMutiFrame((char*)source.data, mid.c_str(), source.total() * source.elemSize());
	picTovideo_impl((mid + "\\frame%d.jpg").c_str(), target_path, 30, 1080);


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
				std::string resizedPath = mid + "\resized_frame" + std::to_string(i) + ".png";
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
				// 处理后的帧解码失败，尝试使用原始帧
		std::cout << "Frame " << i << ": Processed frame failed, trying original frame" << std::endl;
		
		// 将原始图像调整为1080x1080
		Mat resizedFrame;
		resize(frame, resizedFrame, Size(1080, 1080), 0, 0, INTER_NEAREST);

			// 保存1080x1080图像
			if (!resizedFrame.empty()) {
				std::string resizedPath = mid + "\\resized_frame" + std::to_string(i) + "_original.png";
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
				decode_fail_count++;
				std::cerr << "Frame " << i << ": Decode failed (Invalid frame flag)" << std::endl;
			}
			}
		} else {
			// 预处理失败，使用原始帧
		std::cout << "Frame " << i << ": Using original frame for decoding" << std::endl;

		// 将原始图像调整为1080x1080
		Mat resizedFrame;
		resize(frame, resizedFrame, Size(1080, 1080), 0, 0, INTER_NEAREST);

			// 保存1080x1080图像
			if (!resizedFrame.empty()) {
				std::string resizedPath = mid + "\\resized_frame" + std::to_string(i) + "_original.png";
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
				decode_fail_count++;
				std::cerr << "Frame " << i << ": Decode failed (Invalid frame flag)" << std::endl;
			}
		}
	}
	std::cout << "Step 3: Decode success: " << decode_success_count << ", Failed: " << decode_fail_count << std::endl;
	std::cout << "Total frames processed: " << (decode_success_count + decode_fail_count) << std::endl;
	std::cout << "Valid QR frames found: " << valid_qr_count << std::endl;

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


int main(int argc, char* argv[]) {
	// 测试模式：使用固定路径
	std::string source_path = "output/15.mp4";
	std::string target_path = "output/decoded_test.png";

#ifdef _ENCODE
	encodeEXE(source_path, target_path);

#elif defined(_DECODE)
	decodeEXE(source_path, target_path);
#else
	std::cerr << "Error: Please define either _ENCODE or _DECODE before compiling." << std::endl;

#endif
	return 0;
}
