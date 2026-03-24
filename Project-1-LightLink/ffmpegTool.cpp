#include "ffmpegTool.h"
#include<windows.h>
#include<filesystem>
#include<iostream>

// Keep functions in anonymous namespace to limit linkage to this translation unit


std::string getRootdir_impl() {
    constexpr int MAXSIZE = 260;
    wchar_t path_buf[MAXSIZE + 1] = { 0 };
    int status = GetModuleFileNameW(NULL, path_buf, MAXSIZE);
    if (status == 0) {
        throw std::runtime_error(std::string("GetModuleFileNameW :") + std::to_string(GetLastError()));
    }

    std::filesystem::path temp(path_buf);
    std::filesystem::path direct_path = std::filesystem::canonical(path_buf);

    std::filesystem::path root_path = direct_path.parent_path().parent_path();

    return root_path.string();
}

void picTovideo_impl(std::string pic_path, std::string video_path, int fps, int size, int Vquality) {
    std::string command_cd = "cd " + getRootdir_impl();

    std::string command = "bin\\ffmpeg\\ffmpeg.exe -r " + std::to_string(fps)
        + " -i " + pic_path + " -vf scale=-1:" + std::to_string(size)
        + ":flags=neighbor -c:v libx264 -crf " + std::to_string(Vquality)
        + " -pix_fmt yuv420p -y " + video_path;

    std::cout << "excute command:" << command << std::endl;
    command = command_cd + " && " + command;
    system(command.c_str());
}

// ==================== 视频拆帧 ====================
// 视频转图片：将视频文件拆分为帧图片
int VideotoPic(const char* videoPath, const char* imagePath, const char* imageFormat) {
    std::string rootDir = getRootdir_impl();
    std::string command = "cd " + rootDir + " && ";
    command += "bin\\ffmpeg\\ffmpeg.exe -i " + std::string(videoPath);
    command += " -vf scale=1080:1080:force_original_aspect_ratio=decrease,pad=1080:1080:(ow-iw)/2:(oh-ih)/2 ";
    command += std::string(imagePath) + "%04d." + std::string(imageFormat);
    command += " -y"; // 覆盖已存在的文件

    std::cout << "Video to pictures command: " << command << std::endl;
    return system(command.c_str());
}



