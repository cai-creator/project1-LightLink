#include "codeTool.h"
#include "ffmpegTool.h"
#include<ctime>
#include<cstdlib>
#include <iostream>
#define _ENCODE // 解码模式改成：   _DECODE
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
void decodeEXE() {

}


int main(int argc, char *argv[])
{
	if (argc != 3) {
		std::cout << "Format error" << std::endl;
	}

	std::string source_path(argv[1]);
	std::string target_path(argv[2]);
	
#ifdef _ENCODE
	encodeEXE(source_path,target_path);

#elif defined(_DECODE)
	decodeEXE();
#else
	std::cerr << "Error: Please define either _ENCODE or _DECODE before compiling." << std::endl;
   
#endif
	return 0;
}