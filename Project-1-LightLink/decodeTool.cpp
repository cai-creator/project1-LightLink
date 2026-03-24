// 解码工具实现文件
// 实现单张二维码图片解码功能
#include "decodeTool.h"
#include <iostream>
#include <sstream>

#define DECODE_DEBUG 1

#if DECODE_DEBUG
#define DECODE_DBG(msg) std::cerr << msg << std::endl
#else
#define DECODE_DBG(msg)
#endif

namespace decode {

    // 像素颜色定义：白色和黑色
    const cv::Vec3b PIX_COLOR[2] = {
        cv::Vec3b(255, 255, 255),  // 白色，像素值为0
        cv::Vec3b(0, 0, 0)         // 黑色，像素值为1
    };

    // 判断像素是黑色还是白色（灰度阈值，用于处理JPEG压缩后的质量损失）
    inline bool isBlack(const cv::Vec3b& pixel) {
        // 使用灰度值判断：< 128 视为黑色
        return (pixel[0] + pixel[1] + pixel[2]) / 3 < 128;
    }

    inline bool isWhite(const cv::Vec3b& pixel) {
        // 使用灰度值判断：>= 128 视为白色
        return (pixel[0] + pixel[1] + pixel[2]) / 3 >= 128;
    }

    // 10个信息区的位置和尺寸定义（108版本）
    // 格式：{起始行, 起始列, 行数, 列数}
    const int INFO_AREA_SIZE[10][4] = {
        {2, 18, 16, 34},   // 信息区0: 16×34
        {2, 52, 16, 35},   // 信息区1: 16×35
        {18, 2, 36, 52},   // 信息区2: 36×52
        {18, 54, 36, 52},  // 信息区3: 36×52
        {54, 2, 36, 52},   // 信息区4: 36×52
        {54, 54, 36, 52},  // 信息区5: 36×52
        {90, 18, 16, 36},  // 信息区6: 16×36
        {90, 54, 16, 36},  // 信息区7: 16×36
        {90, 90, 8, 16},   // 信息区8: 8×16
        {98, 90, 8, 8}     // 信息区9: 8×8
    };

    // 每个信息区最多能存储的字节数
    const int LEN_MIN[10] = { 68, 70, 234, 234, 234, 234, 72, 72, 16, 8 };

    // 生成随机种子函数
    unsigned int getRandomSeed(int areaId, int row, int col) {
        unsigned int seed = 12345;
        seed = seed * 1103515245 + areaId;
        seed = seed * 1103515245 + row;
        seed = seed * 1103515245 + col;
        return seed;
    }

    // XOR逆操作还原数据位
    int randomizeBit(int bit, int areaId, int row, int col) {
        unsigned int seed = getRandomSeed(areaId, row, col);
        return bit ^ ((seed >> 8) & 1);
    }

    // 读取帧标志位
    bool readFrameFlag(const cv::Mat& frame, bool& isStart, bool& isEnd) {
        const int fixed_col = 87;

        cv::Vec3b flag[4];
        for (int i = 0; i < 4; i++) {
            flag[i] = frame.at<cv::Vec3b>(i + 2, fixed_col);
        }

        bool b0 = isBlack(flag[0]);
        bool b1 = isBlack(flag[1]);
        bool b2 = isBlack(flag[2]);
        bool b3 = isBlack(flag[3]);

        // 黑白白黑: 起始帧
        if (b0 && !b1 && !b2 && b3) {
            isStart = true;
            isEnd = false;
            return true;
        }
        // 黑白黑白: 中间帧
        else if (b0 && !b1 && b2 && !b3) {
            isStart = false;
            isEnd = false;
            return true;
        }
        // 白黑黑白: 终止帧
        else if (!b0 && b1 && b2 && !b3) {
            isStart = false;
            isEnd = true;
            return true;
        }
        // 白黑白黑: 单帧(既是起始也是终止)
        else if (!b0 && b1 && !b2 && b3) {
            isStart = true;
            isEnd = true;
            return true;
        }

        return false;
    }

    // 读取帧编号
    uint16_t readFrameNumber(const cv::Mat& frame) {
        const int fixed_col = 88;
        uint16_t number = 0;

        for (int i = 0; i < 16; i++) {
            cv::Vec3b pixel = frame.at<cv::Vec3b>(i + 2, fixed_col);
            bool bit = isBlack(pixel);
            number |= (bit ? 1 : 0) << i;
        }

        return number;
    }

    // 读取校验码
    uint16_t readCheckCode(const cv::Mat& frame) {
        const int fixed_col = 89;
        uint16_t code = 0;

        for (int i = 0; i < 16; i++) {
            cv::Vec3b pixel = frame.at<cv::Vec3b>(i + 2, fixed_col);
            bool bit = isBlack(pixel);
            code |= (bit ? 1 : 0) << i;
        }

        return code;
    }

    // 读取数据长度
    int readDataLength(const cv::Mat& frame) {
        const int fixed_col = 87;
        int length = 0;

        for (int i = 0; i < 12; i++) {
            cv::Vec3b pixel = frame.at<cv::Vec3b>(i + 6, fixed_col);
            bool bit = isBlack(pixel);
            length |= (bit ? 1 : 0) << i;
        }

        return length;
    }

    // 读取信息区数据
    bool readInfoData(const cv::Mat& frame, int areaId, int len, unsigned char* output) {
        if (areaId < 0 || areaId >= 10 || len <= 0) {
            return false;
        }

        int start_x = INFO_AREA_SIZE[areaId][0];
        int start_y = INFO_AREA_SIZE[areaId][1];
        int rows = INFO_AREA_SIZE[areaId][2];
        int cols = INFO_AREA_SIZE[areaId][3];

        int total_bits = len * 8;
        int bit_index = 0;

        for (int row = 0; row < rows; row++) {
            for (int col = 0; col < cols; col++) {
                if (bit_index >= total_bits) {
                    break;
                }

                cv::Vec3b pixel = frame.at<cv::Vec3b>(start_x + row, start_y + col);
                int bit = isBlack(pixel) ? 1 : 0;
                int original_bit = randomizeBit(bit, areaId, row, col);

                int byte_index = bit_index / 8;
                int bit_offset = 7 - (bit_index % 8);

                if (bit_offset == 7) {
                    output[byte_index] = 0;
                }
                output[byte_index] |= (original_bit << bit_offset);

                bit_index++;
            }
        }

        return true;
    }

    // 计算校验码
    uint16_t calculateCheckCode(int data_len, const unsigned char* data, int frame_id, bool isStart, bool isEnd) {
        uint16_t res = 0;
        int len = data_len / 2 * 2;

        for (int i = 0; i < len; i += 2) {
            res ^= (uint16_t)data[i] << 8 | (uint16_t)data[i + 1];
        }
        res ^= (uint16_t)frame_id;
        res ^= (isStart ? 1 : 0);
        res ^= (isEnd ? 2 : 0);
        res ^= (uint16_t)data_len;

        return res;
    }

    // 校验单帧数据
    bool verifyCheckCode(const ImageInfo& info) {
        uint16_t calcCode = calculateCheckCode(
            info.Info.size(),
            info.Info.data(),
            info.FrameBase,
            info.IsStart,
            info.IsEnd
        );

        return calcCode == info.CheckCode;
    }

    // 解码单张二维码图片
    bool decodeFrame(const cv::Mat& frame, ImageInfo& info) {
        // 确保帧尺寸正确
        if (frame.cols != FrameSize || frame.rows != FrameSize) {
            std::cerr << "Error: Invalid frame size" << std::endl;
            return false;
        }

        // 读取帧标志位
        if (!readFrameFlag(frame, info.IsStart, info.IsEnd)) {
            std::cerr << "Error: Invalid frame flag" << std::endl;
            return false;
        }

        // 读取帧编号
        info.FrameBase = readFrameNumber(frame);
        DECODE_DBG("[DEBUG] FrameBase = " << info.FrameBase);

        // 读取校验码
        info.CheckCode = readCheckCode(frame);
        DECODE_DBG("[DEBUG] CheckCode = " << info.CheckCode);

        // 读取数据长度
        int dataLength = readDataLength(frame);
        DECODE_DBG("[DEBUG] dataLength = " << dataLength);

        // 读取10个信息区的数据
        info.Info.resize(BytesPerFrame, 0);
        int offset = 0;

        for (int i = 0; i < 10; i++) {
            int len = std::min(LEN_MIN[i], dataLength);
            if (len <= 0) break;

            readInfoData(frame, i, len, info.Info.data() + offset);
            DECODE_DBG("[DEBUG] Area " << i << ": len=" << len << ", offset=" << offset);
            dataLength -= len;
            offset += len;
        }

        return true;
    }
}
