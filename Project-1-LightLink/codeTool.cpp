#include "codeTool.h"
#include<ctime>
//#define CODE_DEBUG

using namespace cv; // only in cpp to avoid polluting global namespace from headers

void test_show(Mat &frame) {
    imshow("结果展示", frame);
    waitKey(0);
}


namespace code
{
    // ================== 变量定义 ===================
    constexpr int BytesPerFrame = 1242;// 每一帧可以编码多少字节的数据（1242字节）
    constexpr int BigQrSize = 18; // 大"回"形定位点的默认大小
    constexpr int smallQrSize = 6; // 小"回"形定位点的默认大小
    constexpr int defaultFrameSize = 108; // 帧的默认大小

    // ---- color definition ----
    const Vec3b pixcolor[2] = {
        Vec3b(255,255,255), // white
        Vec3b(0,0,0)       // black
    };

    enum color
    {
        white = 0,
        black = 1
    };

    const int infoAreaSize[10][4] = { // 行坐标， 列坐标，行数，列数
        {2,18,16,34},     // 左上角区域 1
        {2,52,16,35},     // 左上角区域 2
        {18,2,36,52},     // 左侧区域 1
        {18,54,36,52},    // 左侧区域 2
        {54,2,36,52},     // 右侧区域 1
        {54,54,36,52},    // 右侧区域 2
        {90,18,16,36},    // 右下角区域 1
        {90,54,16,36},    // 右下角区域 2
        {90,90,8,16},     // 右下角小区域 1
        {98,90,8,8}       // 右下角小区域 2
    }; 

    const int  lenmin[10] = { 68, 70, 234, 234, 234, 234, 72, 72, 16, 8 }; // 1242字节的10个信息区分别能存储的最小字节数
    //  ===================== 定位位置定义 =========================
    constexpr int LocatePosition[4][3] = {
        {0,0,BigQrSize},
        {0,defaultFrameSize - BigQrSize,BigQrSize},
        {defaultFrameSize - BigQrSize,0,BigQrSize},
        {defaultFrameSize - smallQrSize, defaultFrameSize - smallQrSize, smallQrSize}
    };

    const Vec3b BigLocateColor[9] = { // 大定位点颜色
        pixcolor[color::white],
        pixcolor[color::white],
        pixcolor[color::black],
        pixcolor[color::black],
        pixcolor[color::white],
        pixcolor[color::white],
        pixcolor[color::black],
        pixcolor[color::black],
        pixcolor[color::black]
    };
    const Vec3b SmallLocateColor[5] = { // 小定位点颜色
        pixcolor[color::white],
        pixcolor[color::black],
        pixcolor[color::white],
        pixcolor[color::black],
        pixcolor[color::black],
    };


    // ================== 函数定义 ====================


    // --- 创建白色边框 (安全区域) ---
    void CreateWhiteFrame(Mat& frame, int size ) {

        const Rect safeArea[] = {// Rect(列，行，宽，高)
            Rect(0,0,size,2),
            Rect(0,0,2,size),
            Rect(size - 2,0,2,size),
            Rect(0,size - 2,size,2)
        };// 安全区域，四条边界
        Scalar s_white = Scalar(pixcolor[color::white]);

        // 在安全区域内填充白色
        for (int i = 0; i < 4; i++)
            frame(safeArea[i]).setTo(s_white);
#ifdef CODE_DEBUG
        test_show(frame);
#endif
    }



    // --- 创建定位点 ---
    void CreateLocationQr(Mat& frame){
        // 绘制大二维码识别点（颜色反转）
        constexpr int pointPos[3][2] = {
            {0, 0},
            {0, defaultFrameSize - BigQrSize},
            {defaultFrameSize - BigQrSize, 0}
        };
        
       
        const Vec3b BigLocateColorReversed[9] = {
            pixcolor[color::black],   
            pixcolor[color::black],    
            pixcolor[color::black],    
            pixcolor[color::white],    
            pixcolor[color::white],   
            pixcolor[color::black],    
            pixcolor[color::black],    
            pixcolor[color::white],     
            pixcolor[color::white]
        };
        
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < BigQrSize; ++j) {
                for (int k = 0; k < BigQrSize; ++k) {
                    int dist = (int)std::max(std::fabs(j - (BigQrSize - 1) / 2.0), std::fabs(k - (BigQrSize - 1) / 2.0));
                    frame.at<Vec3b>(pointPos[i][0] + j, pointPos[i][1] + k) = BigLocateColorReversed[dist];
                }
            }
        }
        
        
        constexpr int posCenter[2] = {defaultFrameSize - smallQrSize, defaultFrameSize - smallQrSize};
        
        // 小定位点颜色
        const Vec3b SmallLocateColorReversed[5] = {
            pixcolor[color::black],    
            pixcolor[color::black],    
            pixcolor[color::white],    
            pixcolor[color::black],    
            pixcolor[color::white]    
        };
        
        for (int i = -4; i <= 4; ++i) {
            for (int j = -4; j <= 4; ++j) {
                int dist = std::max(std::abs(i), std::abs(j));
                frame.at<Vec3b>(posCenter[0] + i, posCenter[1] + j) = SmallLocateColorReversed[dist];
            }
        }
#ifdef CODE_DEBUG
        test_show(frame);
#endif
        
    }

    // --- 简单的伪随机数生成器（可逆） ---
    unsigned int getRandomSeed(int areaId, int row, int col) {
        // 使用固定的种子，确保每次生成相同的序列
        unsigned int seed = 12345;
        seed = seed * 1103515245 + areaId;
        seed = seed * 1103515245 + row;
        seed = seed * 1103515245 + col;
        return seed;
    }
    
    // --- 可逆的随机化函数 ---
    int randomizeBit(int bit, int areaId, int row, int col) {
        unsigned int seed = getRandomSeed(areaId, row, col);
        // 使用异或操作进行随机化，异或操作是可逆的
        // 因为：bit ^ random_bit ^ random_bit = bit
        // 解码时，只需对读取的位再次调用此函数即可还原
        return bit ^ ((seed >> 8) & 1);
    }
    
    // --- 解码时的还原函数（与randomizeBit相同） ---
    // 由于异或操作的可逆性，解码时直接调用randomizeBit即可
    // 示例：
    // int reading_bit = ...; // 从二维码读取的位
    // int original_bit = randomizeBit(reading_bit, areaId, row, col); // 还原原始位

    // --- 写入信息区 ---
    void InputInfoData(Mat& frame, int len, int areaId, const char *Information) { // （原帧，信息长度,信息区域号， 信息内容）
        const unsigned char* data = (const unsigned char*)Information;
        const unsigned char* data_end = data + len;
        
        int start_x = infoAreaSize[areaId][0];  // 起始行
        int start_y = infoAreaSize[areaId][1];  // 起始列
        int rows = infoAreaSize[areaId][2];     // 行数
        int cols = infoAreaSize[areaId][3];     // 列数
        
        int total_bits = len * 8;
        int bit_index = 0;
        
        // 遍历信息区的每个像素
        
        for (int row = 0; row < rows; row++) {
            for (int col = 0; col < cols; col++) {
                if (bit_index >= total_bits) {
                    // 数据不足，用随机数填充
                    int random_bit = randomizeBit(rand()&1, areaId, row, col);
                    frame.at<Vec3b>(start_x + row, start_y + col) = pixcolor[random_bit];
                } else {
                    // 计算当前位对应的字节和偏移
                    int byte_index = bit_index / 8;
                    int bit_offset = 7 - (bit_index % 8);  // 高位在前
                    unsigned char current_byte = (data + byte_index < data_end) ? *(data + byte_index) : 0;
                    int bit = (current_byte >> bit_offset) & 1;
                    int randomized_bit = randomizeBit(bit, areaId, row, col);
                    frame.at<Vec3b>(start_x + row, start_y + col) = pixcolor[randomized_bit];
                    
                    bit_index++;
                }
            }
        }
#ifdef CODE_DEBUG
        test_show(frame);
#endif

    }

    // --- 写入帧标志位 ---
    void InputFrameFlag(Mat& frame,frameStyle style,int data_len) { // 87列起
        // --- 帧的颜色定义 ---
        const Vec3b stylecolor[4][4] = {
            { pixcolor[color::black],pixcolor[color::white],pixcolor[color::white],pixcolor[color::black]}, // 黑白白黑 - 第一帧
            { pixcolor[color::black],pixcolor[color::white],pixcolor[color::black],pixcolor[color::white]}, // 黑白黑白 - 中间帧
            { pixcolor[color::white],pixcolor[color::black],pixcolor[color::black],pixcolor[color::white]}, // 白黑黑白 - 最后一帧
            { pixcolor[color::white],pixcolor[color::black],pixcolor[color::white],pixcolor[color::black]} // 白黑白黑 - 既是第一帧又是最后一帧
        };
        constexpr int fixed_col = 87; // 帧标志位固定在第87列

        for (int i = 0;i < 4;i++) {
            frame.at<Vec3b>(i + 2, fixed_col) = stylecolor[(int)style][i];
        }

        for (int i = 0;i < 12;i++) { // 写入数据长度 - 主要对于结束帧有用
            frame.at<Vec3b>(i + 6, fixed_col) = pixcolor[data_len & 1];
            data_len >>= 1;
        }
#ifdef CODE_DEBUG
        test_show(frame);
#endif
    }

    // --- 写入编号帧 ---
    void InputFrameNumber(Mat& frame, int number) {
        constexpr int fixed_col = 88; // 编号帧固定在第88列
        for (int i = 0;i < 16;i++) {
            frame.at<Vec3b>(i + 2, fixed_col) = pixcolor[number & 1];
            number >>= 1;
        }
#ifdef CODE_DEBUG
        test_show(frame);
#endif
    }

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

    // --- 获取校验码 ---
    uint16_t GetCheckCode(int data_len, const char*Info, int frame_id, frameStyle style ) {
        uint16_t crc ;
        // ----- 用来寄存crc输入的 -----
        uint8_t data[2048];
        if(2048<data_len + 10) return 0;

        memcpy(data, Info, data_len);

        int idx = data_len;

        for(int i = 0;i<(int)(sizeof(int));i++){
            data[idx++] = (uint8_t) ((frame_id >> i * 8)&0xFF);
        }

        for(int i = 0;i<(int)(sizeof(int));i++){
            data[idx++] = (uint8_t) ((data_len >> i * 8)&0xFF);
        }

        data[idx++] = (uint8_t) ((uint8_t)style&0xFF);
        crc = crc16_maxim(data, idx);
        return crc;
    }
    
    // --- 写入校验码 ---
    void InputCheckCode(Mat& frame, uint16_t checkcode) {
        constexpr int fixed_col = 89; // 编号帧固定在第88列
        for (int i = 0;i < 16;i++) {
            frame.at<Vec3b>(i + 2, fixed_col) = pixcolor[checkcode & 1];
            checkcode >>= 1;
        }
#ifdef CODE_DEBUG
        test_show(frame);
#endif
    }

    Mat ScaleToTen(Mat& frame, int B) { // （原帧， 放大倍数）        
          int ScaleFrameSize = defaultFrameSize * B;
          Mat res(ScaleFrameSize,ScaleFrameSize, CV_8UC3, Scalar(255,255,255));
          for (int i = 0;i < ScaleFrameSize;i++) {
              for (int j = 0;j < ScaleFrameSize;j++) {
                  res.at<Vec3b>(i, j) = frame.at<Vec3b>(i / B, j / B);
              }
          }

          return res;
    }

    Mat CreateFrame(int data_len, const char* data, frameStyle style, int frame_id) {
        Mat frame(defaultFrameSize, defaultFrameSize, CV_8UC3, Scalar(0,0,0));
        CreateWhiteFrame(frame, defaultFrameSize);
        CreateLocationQr(frame);
        int len;
        // --- 写入信息区 ---
        InputFrameFlag(frame, style, data_len);
        InputFrameNumber(frame, frame_id);
        uint16_t chekcode = GetCheckCode(data_len, data, frame_id, style);
        InputCheckCode(frame, chekcode);

        for (int i = 0;i < 10;i++) {
            len = min(lenmin[i], data_len);
            data_len -= len;
            InputInfoData(frame, len, i, data);
            data += len;
         }

        return frame;
    }

 void SaveMutiFrame(char* data, const char* savepath, int len, int width) {
     Mat frame;
     srand(time(0));
     
     if (len < BytesPerFrame) {
         frame = CreateFrame(len, data, frameStyle::FirstAndFinall, width);
         if (frame.empty()) {
             std::cout << "Error: image generation failed" << std::endl;
         }
         else if (!imwrite(std::string(savepath) + "/frame1.jpg", frame)) {
             std::cout << "Error: image save failed" << std::endl;
         }
     }

     else {
         int frame_id = width;
         while (len > 0) {
             int current_len = min(len, BytesPerFrame);
             len -= current_len;
             frameStyle style = (len == 0) ? frameStyle::Finall : ((frame_id == 1) ? frameStyle::First : frameStyle::Normal);
             frame = CreateFrame(current_len, data, style, frame_id);
             if (frame.empty()) {
                 std::cout << "Error: image generation failed" << std::endl;
             }
             else if (!imwrite(std::string(savepath) + "/frame" + std::to_string(frame_id) + ".jpg", frame)) {
                 std::cout << "Error: image save failed" << std::endl;
             }
             data += current_len;
             frame_id++;
         }
     }
     std::cout << "The picture has been ready" <<std::endl;
 }
}
