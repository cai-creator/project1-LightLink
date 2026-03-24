#pragma once
#include <string>

// Return root directory of the application (string)
std::string getRootdir_impl();

// 
void picTovideo_impl(std::string pic_path, std::string video_path, int fps, int size, int Vquality = 18);
