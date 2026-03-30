// 解码工具头文件
// 提供单张二维码图片解码功能，将二维码图像转换为原始数据
#pragma once
#include <opencv2/opencv.hpp>
#include <string>
#include <vector>
#include <cstdint>

namespace decode {

    // 每帧包含的字节数
    constexpr int BytesPerFrame = 1242;
    // 二维码帧尺寸
    constexpr int FrameSize = 1080;

    // 图像帧信息结构体
    struct ImageInfo
    {
        std::vector<unsigned char> Info;  // 解码出的数据
        uint16_t CheckCode;                // 校验码
        uint16_t FrameBase;                // 帧编号
        bool IsStart;                      // 是否是起始帧
        bool IsEnd;                        // 是否是终止帧
        int dataLength;                    // 数据长度
    };

    // 生成随机种子
    unsigned int getRandomSeed(int areaId, int row, int col);

    // XOR逆操作还原数据位
    int randomizeBit(int bit, int areaId, int row, int col);

    // 读取帧标志位
    bool readFrameFlag(const cv::Mat& frame, bool& isStart, bool& isEnd, float frame_avg_gray);

    // 读取帧编号
    uint16_t readFrameNumber(const cv::Mat& frame, float frame_avg_gray);

    // 读取校验码
    uint16_t readCheckCode(const cv::Mat& frame, float frame_avg_gray);

    // 读取数据长度
    int readDataLength(const cv::Mat& frame, float frame_avg_gray);

    // 读取信息区数据
    bool readInfoData(const cv::Mat& frame, int areaId, int len, unsigned char* output, float frame_avg_gray);

    // 计算校验码
    uint16_t calculateCheckCode(int data_len, const unsigned char* data, int frame_id, bool isStart, bool isEnd);

    // 校验单帧数据
    bool verifyCheckCode(const ImageInfo& info);

    // 解码单张二维码图片
    // frame: 输入的二维码图像(108x108)
    // info: 输出解码后的帧信息
    // 返回是否解码成功
    bool decodeFrame(const cv::Mat& frame, ImageInfo& info);
}
