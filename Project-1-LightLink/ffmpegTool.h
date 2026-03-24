#pragma once
#include <string>

// Return root directory of the application (string)
std::string getRootdir_impl();

// 图片合成视频
void picTovideo_impl(std::string pic_path, std::string video_path, int fps, int size, int Vquality = 18);

// 视频拆帧：将视频文件拆分为帧图片
int VideotoPic(const char* videoPath, const char* imagePath, const char* imageFormat);
