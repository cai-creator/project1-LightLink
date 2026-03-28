// 解码工具实现文件
// 实现单张二维码图片解码功能
#include "decodeTool.h"
#include <iostream>
#include <sstream>
#include <cstring>
#include <algorithm>
#include <cmath>
#include <utility>

#define DECODE_DEBUG 1

#if DECODE_DEBUG
#define DECODE_DBG(msg) std::cerr << msg << std::endl
#else
#define DECODE_DBG(msg)
#endif

namespace decode {

    // CRC16-MAXIM算法实现
    uint16_t crc16_maxim(uint8_t *data, uint16_t length){
        uint8_t i;
        uint16_t crc = 0;
        while(length--)
        {
            crc ^= *data++;
            for (i = 0; i < 8; ++i)
            {
                if (crc & 1)
                    crc = (crc >> 1) ^ 0xA001;
                else
                    crc >>= 1;
            }
        }
        return ~crc;
    }

    // 像素颜色定义：白色和黑色
    const cv::Vec3b PIX_COLOR[2] = {
        cv::Vec3b(255, 255, 255),  // 白色，像素值为0
        cv::Vec3b(0, 0, 0)         // 黑色，像素值为1
    };

    // 判断像素是黑色还是白色
    // 预处理后应该是二值图像（0或255），但保留阈值判断作为备用
    // 对于手机拍摄的残影，使用更激进的阈值
    inline bool isBlack(const cv::Mat& frame, int row, int col) {
        if (frame.channels() == 3) {
            // 彩色图像
            cv::Vec3b pixel = frame.at<cv::Vec3b>(row, col);
            return (pixel[0] + pixel[1] + pixel[2]) / 3 < 128;
        } else if (frame.channels() == 1) {
            // 灰度图像（预处理后应该是二值的）
            uchar pixel = frame.at<uchar>(row, col);
            return pixel < 128;
        }
        return false;
    }

    inline bool isWhite(const cv::Mat& frame, int row, int col) {
        if (frame.channels() == 3) {
            // 彩色图像
            cv::Vec3b pixel = frame.at<cv::Vec3b>(row, col);
            return (pixel[0] + pixel[1] + pixel[2]) / 3 >= 128;
        } else if (frame.channels() == 1) {
            // 灰度图像（预处理后应该是二值的）
            uchar pixel = frame.at<uchar>(row, col);
            return pixel >= 128;
        }
        return true;
    }

    // 10个信息区的位置和尺寸定义（1080版本）
    // 格式：{起始行, 起始列, 行数, 列数}
    const int INFO_AREA_SIZE[10][4] = {
        {20, 180, 160, 340},   // 信息区0: 160×340
        {20, 520, 160, 350},   // 信息区1: 160×350
        {180, 20, 360, 520},   // 信息区2: 360×520
        {180, 540, 360, 520},  // 信息区3: 360×520
        {540, 20, 360, 520},   // 信息区4: 360×520
        {540, 540, 360, 520},  // 信息区5: 360×520
        {900, 180, 160, 360},  // 信息区6: 160×360
        {900, 540, 160, 360},  // 信息区7: 160×360
        {900, 900, 80, 160},   // 信息区8: 80×160
        {980, 900, 80, 80}     // 信息区9: 80×80
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

    // 投票判断函数：返回投票结果和置信度
    // 置信度 = |black_ratio - 0.5|，值越大表示越确定
    // threshold: 判定为黑色的阈值（0.0-1.0），默认0.5
    inline std::pair<bool, float> voteBit(int black_count, int total_count, float threshold = 0.5f) {
        float ratio = (float)black_count / total_count;
        bool isBlack = ratio > threshold;
        float confidence = std::abs(ratio - 0.5f);
        return {isBlack, confidence};
    }

    // 使用指定阈值读取信息区数据
    bool readInfoDataWithThreshold(const cv::Mat& frame, int areaId, int len, unsigned char* output, float threshold) {
        if (areaId < 0 || areaId >= 10 || len <= 0) {
            return false;
        }

        int start_x = INFO_AREA_SIZE[areaId][0];
        int start_y = INFO_AREA_SIZE[areaId][1];
        int rows = INFO_AREA_SIZE[areaId][2];
        int cols = INFO_AREA_SIZE[areaId][3];

        // 检查边界
        if (frame.rows < start_x + rows || frame.cols < start_y + cols) {
            return false;
        }

        int total_bits = len * 8;
        int bit_index = 0;

        // 每个像素块的大小（10x10）
        const int block_size = 10;

        for (int row = 0; row < rows; row += block_size) {
            for (int col = 0; col < cols; col += block_size) {
                if (bit_index >= total_bits) {
                    break;
                }

                // 投票机制：统计10x10区域内的黑色像素数
                int black_count = 0;
                int total_count = 0;

                for (int r = 0; r < block_size && row + r < rows; r++) {
                    for (int c = 0; c < block_size && col + c < cols; c++) {
                        if (isBlack(frame, start_x + row + r, start_y + col + c)) {
                            black_count++;
                        }
                        total_count++;
                    }
                }

                // 使用指定的阈值进行投票
                bool bit = voteBit(black_count, total_count, threshold).first;
                int original_bit = randomizeBit(bit, areaId, row / block_size, col / block_size);

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

    // 读取帧标志位（使用投票机制）
    bool readFrameFlag(const cv::Mat& frame, bool& isStart, bool& isEnd) {
        const int fixed_col = 870;

        // 检查边界
        if (frame.rows < 60 || frame.cols <= fixed_col + 10) {
            return false;
        }

        // 每个像素块的大小（10x10）
        const int block_size = 10;
        bool flag[4];
        float confidence[4];

        for (int i = 0; i < 4; i++) {
            int black_count = 0;
            int total_count = 0;

            // 统计10x10区域内的黑色像素数
            for (int r = 0; r < block_size; r++) {
                for (int c = 0; c < block_size; c++) {
                    int row = i * block_size + 20 + r;
                    int col = fixed_col + c;
                    if (row < frame.rows && col < frame.cols) {
                        if (isBlack(frame, row, col)) {
                            black_count++;
                        }
                        total_count++;
                    }
                }
            }

            // 投票决定该位的值，并记录置信度
            auto result = voteBit(black_count, total_count);
            flag[i] = result.first;
            confidence[i] = result.second;
        }

        bool b0 = flag[0];
        bool b1 = flag[1];
        bool b2 = flag[2];
        bool b3 = flag[3];

        // 检查置信度：如果所有位的置信度都很低，可能是有问题的帧
        float minConfidence = *std::min_element(confidence, confidence + 4);
        if (minConfidence < 0.1f) {
            DECODE_DBG("[DEBUG] Frame flag low confidence: " << minConfidence);
        }

        DECODE_DBG("[DEBUG] Frame flag bits: " << b0 << " " << b1 << " " << b2 << " " << b3);

        // 黑白白黑: 起始帧
        if (b0 && !b1 && !b2 && b3) {
            isStart = true;
            isEnd = false;
            DECODE_DBG("[DEBUG] Frame flag: Start frame");
            return true;
        }
        // 黑白黑白: 中间帧
        else if (b0 && !b1 && b2 && !b3) {
            isStart = false;
            isEnd = false;
            DECODE_DBG("[DEBUG] Frame flag: Middle frame");
            return true;
        }
        // 白黑黑白: 终止帧
        else if (!b0 && b1 && b2 && !b3) {
            isStart = false;
            isEnd = true;
            DECODE_DBG("[DEBUG] Frame flag: End frame");
            return true;
        }
        // 白黑白黑: 单帧(既是起始也是终止)
        else if (!b0 && b1 && !b2 && b3) {
            isStart = true;
            isEnd = true;
            DECODE_DBG("[DEBUG] Frame flag: Single frame");
            return true;
        }

        DECODE_DBG("[DEBUG] Frame flag: Invalid pattern");
        return false;
    }

    // 读取帧编号（使用投票机制）
    uint16_t readFrameNumber(const cv::Mat& frame) {
        const int fixed_col = 880;
        uint16_t number = 0;

        // 检查边界
        if (frame.rows < 180 || frame.cols <= fixed_col + 10) {
            return 0;
        }

        // 每个像素块的大小（10x10）
        const int block_size = 10;

        for (int i = 0; i < 16; i++) {
            int black_count = 0;
            int total_count = 0;

            // 统计10x10区域内的黑色像素数
            for (int r = 0; r < block_size; r++) {
                for (int c = 0; c < block_size; c++) {
                    int row = i * block_size + 20 + r;
                    int col = fixed_col + c;
                    if (row < frame.rows && col < frame.cols) {
                        if (isBlack(frame, row, col)) {
                            black_count++;
                        }
                        total_count++;
                    }
                }
            }

            // 使用改进的投票机制
            bool bit = voteBit(black_count, total_count).first;
            number |= (bit ? 1 : 0) << i;
        }

        return number;
    }

    // 读取校验码（使用投票机制）
    uint16_t readCheckCode(const cv::Mat& frame) {
        const int fixed_col = 890;
        uint16_t code = 0;

        // 检查边界
        if (frame.rows < 180 || frame.cols <= fixed_col + 10) {
            return 0;
        }

        // 每个像素块的大小（10x10）
        const int block_size = 10;

        for (int i = 0; i < 16; i++) {
            int black_count = 0;
            int total_count = 0;

            // 统计10x10区域内的黑色像素数
            for (int r = 0; r < block_size; r++) {
                for (int c = 0; c < block_size; c++) {
                    int row = i * block_size + 20 + r;
                    int col = fixed_col + c;
                    if (row < frame.rows && col < frame.cols) {
                        if (isBlack(frame, row, col)) {
                            black_count++;
                        }
                        total_count++;
                    }
                }
            }

            // 使用改进的投票机制
            bool bit = voteBit(black_count, total_count).first;
            code |= (bit ? 1 : 0) << i;
        }

        return code;
    }

    // 读取数据长度（使用投票机制）
    int readDataLength(const cv::Mat& frame) {
        const int fixed_col = 870;
        int length = 0;

        // 检查边界
        if (frame.rows < 180 || frame.cols <= fixed_col + 10) {
            return 0;
        }

        // 每个像素块的大小（10x10）
        const int block_size = 10;

        for (int i = 0; i < 12; i++) {
            int black_count = 0;
            int total_count = 0;

            // 统计10x10区域内的黑色像素数
            for (int r = 0; r < block_size; r++) {
                for (int c = 0; c < block_size; c++) {
                    int row = i * block_size + 60 + r;
                    int col = fixed_col + c;
                    if (row < frame.rows && col < frame.cols) {
                        if (isBlack(frame, row, col)) {
                            black_count++;
                        }
                        total_count++;
                    }
                }
            }

            // 使用改进的投票机制
            bool bit = voteBit(black_count, total_count).first;
            length |= (bit ? 1 : 0) << i;
        }

        return length;
    }

    // 读取信息区数据（使用投票机制）
    bool readInfoData(const cv::Mat& frame, int areaId, int len, unsigned char* output) {
        if (areaId < 0 || areaId >= 10 || len <= 0) {
            return false;
        }

        int start_x = INFO_AREA_SIZE[areaId][0];
        int start_y = INFO_AREA_SIZE[areaId][1];
        int rows = INFO_AREA_SIZE[areaId][2];
        int cols = INFO_AREA_SIZE[areaId][3];

        // 检查边界
        if (frame.rows < start_x + rows || frame.cols < start_y + cols) {
            return false;
        }

        int total_bits = len * 8;
        int bit_index = 0;

        // 每个像素块的大小（10x10）
        const int block_size = 10;

        for (int row = 0; row < rows; row += block_size) {
            for (int col = 0; col < cols; col += block_size) {
                if (bit_index >= total_bits) {
                    break;
                }

                // 投票机制：统计10x10区域内的黑色像素数
                int black_count = 0;
                int total_count = 0;

                for (int r = 0; r < block_size && row + r < rows; r++) {
                    for (int c = 0; c < block_size && col + c < cols; c++) {
                        if (isBlack(frame, start_x + row + r, start_y + col + c)) {
                            black_count++;
                        }
                        total_count++;
                    }
                }

                // 使用改进的投票机制
                bool bit = voteBit(black_count, total_count).first;
                int original_bit = randomizeBit(bit, areaId, row / block_size, col / block_size);

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
        // 计算与编码时一致的 style 值
        int style = 0;
        if (isStart && isEnd) {
            style = 3; // FirstAndFinall
        } else if (isStart) {
            style = 0; // First
        } else if (isEnd) {
            style = 2; // Finall
        } else {
            style = 1; // Normal
        }

        // 构建与编码相同的数据结构
        uint8_t crc_data[2048];
        if (2048 < data_len + 10) return 0;

        // 复制数据
        memcpy(crc_data, data, data_len);

        int idx = data_len;

        // 添加frame_id（4字节）
        for (int i = 0; i < (int)(sizeof(int)); i++) {
            crc_data[idx++] = (uint8_t)((frame_id >> i * 8) & 0xFF);
        }

        // 添加data_len（4字节）
        for (int i = 0; i < (int)(sizeof(int)); i++) {
            crc_data[idx++] = (uint8_t)((data_len >> i * 8) & 0xFF);
        }

        // 添加style（1字节）
        crc_data[idx++] = (uint8_t)(style & 0xFF);

        // 使用CRC16-MAXIM算法计算校验码
        return crc16_maxim(crc_data, idx);
    }

    // 校验单帧数据
    bool verifyCheckCode(const ImageInfo& info) {
        uint16_t calcCode = calculateCheckCode(
            info.dataLength,
            info.Info.data(),
            info.FrameBase,
            info.IsStart,
            info.IsEnd
        );

        return calcCode == info.CheckCode;
    }

    // 使用指定阈值解码单帧
    bool decodeFrameWithThreshold(const cv::Mat& frame, ImageInfo& info, float threshold) {
        // 读取帧标志位
        if (!readFrameFlag(frame, info.IsStart, info.IsEnd)) {
            return false;
        }

        // 读取帧编号
        info.FrameBase = readFrameNumber(frame);

        // 读取校验码
        info.CheckCode = readCheckCode(frame);

        // 读取数据长度
        int dataLength = readDataLength(frame);
        info.dataLength = dataLength;

        // 读取10个信息区的数据（使用指定阈值）
        info.Info.resize(dataLength, 0);
        int offset = 0;
        int remainingLength = dataLength;

        for (int i = 0; i < 10; i++) {
            int len = std::min(LEN_MIN[i], remainingLength);
            if (len <= 0) break;

            readInfoDataWithThreshold(frame, i, len, info.Info.data() + offset, threshold);
            remainingLength -= len;
            offset += len;
        }

        return true;
    }

    // 解码单张二维码图片（带多阈值重试机制）
    bool decodeFrame(const cv::Mat& frame, ImageInfo& info) {
        // 确保帧尺寸正确
        if (frame.cols != FrameSize || frame.rows != FrameSize) {
            std::cerr << "Error: Invalid frame size" << std::endl;
            return false;
        }

        // 尝试不同的阈值
        float thresholds[] = {0.5f, 0.45f, 0.55f, 0.4f, 0.6f};
        
        for (float threshold : thresholds) {
            ImageInfo tempInfo;
            if (decodeFrameWithThreshold(frame, tempInfo, threshold)) {
                // 验证CRC
                if (verifyCheckCode(tempInfo)) {
                    info = tempInfo;
                    DECODE_DBG("[DEBUG] Decoded with threshold=" << threshold);
                    return true;
                }
            }
        }

        // 所有阈值都失败，使用默认阈值0.5返回结果（让上层决定是否接受）
        return decodeFrameWithThreshold(frame, info, 0.5f);
    }
}
