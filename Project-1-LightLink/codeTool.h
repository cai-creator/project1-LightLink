#pragma once
#include <iostream>
#include <cstdint>
#include "opencv2/opencv.hpp"

namespace code
{ 
    // ================== 变量声明 ====================
    enum class frameStyle
    {
        First = 0,
        Normal = 1,
        Finall = 2,
        FirstAndFinall = 3
    };

    // ================== 函数声明 ====================
    void CreateWhiteFrame(cv::Mat& frame, int size);// 建立安全区域 --1
    void CreateLocationQr(cv::Mat& frame);// 建立二维码定位位置   --2
    void InputInfoData(cv::Mat& frame, int len, int area_id, const char *data);// 建立信息区并写入信息  -- 3
    void InputFrameFlag(cv::Mat& frame);// 写入帧标志位  -- 4
    void InputFrameNumber(cv::Mat& frame, int number);// 写入编号帧  -- 5
    uint16_t GetCheckCode(int data_len, const char* Info, int frame_id, frameStyle style);// 获取校验码  -- 6
    void InputCheckCode(cv::Mat& frame, int checkcode);// 写入校验码  -- 7
    cv::Mat EnlargePic(cv::Mat& frame, int size); // 放大图片
    cv::Mat CreateFrame(int data_len, const char* data, frameStyle style, int frame_id);// 8编码一帧数据 等待设计
    void SaveMutiFrame(char* data, const char* savepath, int len, int width);// 9保存多帧数据 等待设计
    cv::Mat ScaleToTen(cv::Mat& frame, int B);
}