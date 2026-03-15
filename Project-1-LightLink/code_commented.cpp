// 这个文件负责将数据编码成二维码图像
#include"code.h"

// 定义一个调试宏，用于显示生成的二维码图像
//#define Code_DEBUG
#define Show_Scale_Img(src) do\
{\
	Mat temp=ScaleToDisSize(src);\
	imshow("Code_DEBUG", temp);\
	waitKey();\
}while (0);

namespace Code
{
	// ==================== 常量定义 ====================
	
	// 每一帧可以编码多少字节的数据（1242字节）
	constexpr int BytesPerFrame = 1242;
	
	// 二维码的大小是 108x108 像素
	constexpr int FrameSize = 108;
	
	// 输出时放大倍数（108x108 放大10倍变成 1080x1080）
	constexpr int FrameOutputRate = 10;
	
	// 二维码外圈的安全区域宽度（白色边框）
	constexpr int SafeAreaWidth = 2;
	
	// 定位点的大小（左上、右上、左下三个角的方块）
	constexpr int QrPointSize = 18;
	
	// 右下角小定位点的偏移量
	constexpr int SmallQrPointbias = 6;
	
	// 信息区域分成7个部分
	constexpr int RectAreaCount = 7;
	
	// 定义8种颜色（用于编码数据）
	// 每个颜色用BGR格式表示（蓝、绿、红）
	const Vec3b pixel[8] = 
	{ 
		Vec3b(0,0,0),      // 0: 黑色
		Vec3b(0,0,255),    // 1: 红色
		Vec3b(0,255,0),    // 2: 绿色
		Vec3b(0,255,255),  // 3: 黄色
		Vec3b(255,0,0),    // 4: 蓝色
		Vec3b(255,0,255),  // 5: 紫色
		Vec3b(255,255,0),  // 6: 青色
		Vec3b(255,255,255) // 7: 白色
	};
	
	// 每个信息区域可以存储多少字节
	const int lenlim[RectAreaCount] = { 138,144,648,144,144,16,8 };
	
	// 每个信息区域的位置和大小
	// 格式：{{高度范围}, {起始位置}}
	const int areapos[RectAreaCount][2][2] =
	{
		{{69,16},{QrPointSize + 3,SafeAreaWidth}},           // 区域1：左侧
		{{16,72},{SafeAreaWidth,QrPointSize}},                // 区域2：上侧
		{{72,72},{QrPointSize,QrPointSize}},                  // 区域3：中央
		{{72,16},{QrPointSize,FrameSize - QrPointSize}},      // 区域4：右侧
		{{16,72},{FrameSize - QrPointSize,QrPointSize}},      // 区域5：下侧
		{{8,16},{FrameSize  - QrPointSize,FrameSize - QrPointSize}},  // 区域6：右下角
		{{8,8},{FrameSize - QrPointSize + 8,FrameSize - QrPointSize}} // 区域7：最右下角
	};
	
	// 定义颜色常量（方便代码阅读）
	enum color
	{
		Black = 0,  // 黑色
		White = 7   // 白色
	};
	
	// 定义帧类型（标识这一帧是开始、结束还是中间帧）
	enum class FrameType
	{
		Start = 0,        // 起始帧
		End = 1,          // 结束帧
		StartAndEnd = 2,  // 既是开始也是结束（文件很小，一帧就够）
		Normal = 3        // 普通中间帧
	};

	// ==================== 图像放大函数 ====================
	
	// 将 108x108 的小二维码放大成 1080x1080 的大图像
	// 这样在视频播放时更清晰，更容易被摄像头拍摄
	Mat ScaleToDisSize(const Mat& src)
	{
		Mat dis;
		constexpr int FrameOutputSize = FrameSize * FrameOutputRate;  // 108 * 10 = 1080
		
		// 创建一个 1080x1080 的空白图像
		dis = Mat(FrameOutputSize, FrameOutputSize, CV_8UC3);
		
		// 将小图像的每个像素复制到大图像的 10x10 区域
		// 例如：小图像的(0,0)像素会被复制到大图像的(0-9, 0-9)区域
		for (int i = 0; i < FrameOutputSize; ++i)
		{
			for (int j = 0; j < FrameOutputSize; ++j)
			{
				// i/10 和 j/10 会得到小图像中对应的坐标
				dis.at<Vec3b>(i,j) = src.at<Vec3b>(i/10, j/10);
			}
		}
		return dis;
	}

	// ==================== 校验码计算函数 ====================
		
	// 计算校验码（用于验证数据是否正确） - 用于校验数据是否被正确编码
	// 使用异或运算（XOR）来生成校验码
	uint16_t CalCheckCode(const unsigned char* info, int len, bool isStart, bool isEnd, uint16_t frameBase)
	{
		uint16_t ans = 0;
		int cutlen = (len / 2)*2;  // 确保是偶数长度
		
		// 将数据每两个字节一组进行异或运算
		for (int i = 0; i < cutlen; i += 2)
			ans ^= ((uint16_t)info[i] << 8) | info[i + 1];
		
		// 如果数据长度是奇数，处理最后一个字节
		if (len & 1)
			ans ^= (uint16_t)info[cutlen] << 8;
		
		// 将数据长度也加入校验
		ans ^= len;
		
		// 将帧编号也加入校验
		ans ^= frameBase;
		
		// 将帧类型（开始/结束）也加入校验
		uint16_t temp = (isStart << 1) + isEnd;
		ans ^= temp;
		
		return ans;
	}

	// ==================== 安全区生成函数 ====================
	
	// 在二维码周围生成白色边框（安全区）
	// 作用：防止二维码边缘被裁剪，提高识别率
	void BulidSafeArea(Mat& mat)
	{
		// 定义四个边框的位置 {x{起始位置，结束位置}，y{起始位置，结束位置}}
		// 上边框、下边框、左边框、右边框
		constexpr int pos[4][2][2] =
		{
			{{0,FrameSize},{0,SafeAreaWidth}},                              // 上边框
			{{0,FrameSize},{FrameSize - SafeAreaWidth,FrameSize}},          // 下边框
			{{0, SafeAreaWidth },{0,FrameSize}},                            // 左边框
			{{FrameSize - SafeAreaWidth,FrameSize},{0,FrameSize}}           // 右边框
		};
		
		// 将四个边框区域填充为白色
		for (int k=0;k<4;++k)
			for (int i = pos[k][0][0]; i < pos[k][0][1]; ++i)
				for (int j = pos[k][1][0]; j < pos[k][1][1]; ++j)
					mat.at<Vec3b>(i,j)=pixel[White];
#ifdef Code_DEBUG
		Show_Scale_Img(mat);
#endif
		return;
	}

	// ==================== 定位点生成函数 ====================
	
	// 生成二维码的定位点（三个大方块 + 一个小方块）
	// 作用：帮助摄像头识别二维码的方向和位置
	void BulidQrPoint(Mat& mat)
	{
		// ==================== 生成三个大定位点 ====================
		// 位置：左上角、右上角、左下角
		constexpr int pointPos[4][2] = 
		{ 
			{0,0},                              // 左上角
			{0,FrameSize- QrPointSize},         // 右上角
			{FrameSize - QrPointSize,0}         // 左下角
		};
		
		// 定位点的黑白模式（从中心到边缘）
		// 黑-黑-黑-白-白-黑-黑-白-白
		const Vec3b vec3bBig[9] =
		{
			pixel[Black],  // 最中心：黑色
			pixel[Black],
			pixel[Black],
			pixel[White],  // 第一圈：白色
			pixel[White],
			pixel[Black],  // 第二圈：黑色
			pixel[Black],
			pixel[White],  // 最外圈：白色
			pixel[White]
		};
		
		// 绘制三个大定位点
		for (int i = 0; i < 3; ++i)
			for (int j = 0; j < QrPointSize; ++j)
				for (int k = 0; k < QrPointSize; ++k)
					// 根据距离中心的远近选择颜色
					mat.at<Vec3b>(pointPos[i][0] + j, pointPos[i][1] + k) =
						vec3bBig[(int)max(fabs(j-8.5), fabs(k-8.5))];
		
		// ==================== 生成右下角小定位点 ====================
		constexpr int posCenter[2] = { FrameSize - SmallQrPointbias,FrameSize - SmallQrPointbias };
		
		// 小定位点的黑白模式
		const Vec3b vec3bsmall[5] =
		{ 
			pixel[Black],   // 最中心：黑色
			pixel[Black],
			pixel[White],   // 第一圈：白色
			pixel[Black],   // 第二圈：黑色
			pixel[White],   // 最外圈：白色
		};
		
		// 绘制小定位点
		for (int i = -4; i <= 4; ++i)
			for (int j = -4; j <= 4; ++j)
				mat.at<Vec3b>(posCenter[0] + i, posCenter[1] + j) = 
							  vec3bsmall[max(abs(i),abs(j))];
#ifdef Code_DEBUG
		Show_Scale_Img(mat);
#endif
	}

	// ==================== 校验码和帧编号生成函数 ====================
	
	// 在二维码中写入校验码和帧编号
	// 作用：解码时验证数据正确性，检测是否跳帧
	void BulidCheckCodeAndFrameNo(Mat& mat,uint16_t checkcode,uint16_t FrameNo)
	{
		// 写入16位的校验码（每个位用黑白表示）
		for (int i = 0; i < 16; ++i)
		{
			// checkcode & 1 取出最低位
			// 如果是1，用白色；如果是0，用黑色
			mat.at<Vec3b>(QrPointSize+1, SafeAreaWidth + i) = pixel[(checkcode & 1)?7:0];
			checkcode >>= 1;  // 右移一位，处理下一位
		}
		
		// 写入16位的帧编号
		for (int i = 0; i < 16; ++i)
		{
			mat.at<Vec3b>(QrPointSize + 2, SafeAreaWidth + i) = pixel[(FrameNo & 1) ? 7 : 0];
			FrameNo >>= 1;
		}
#ifdef Code_DEBUG
		Show_Scale_Img(mat);
#endif
	}

	// ==================== 信息区域生成函数 ====================
	
	// 将数据编码到二维码的信息区域
	// 每个字节用8个黑白点表示（白色=0，黑色=1）
	void BulidInfoRect(Mat& mat, const char* info, int len,int areaID)
	{
		const unsigned char* pos = (const unsigned char*)info;
		const unsigned char* end = pos + len;
		
		// 遍历信息区域的每一行
		for (int i = 0; i < areapos[areaID][0][0]; ++i)
		{
			uint32_t outputCode = 0;
			
			// 每行分成多个8位组
			for (int j = 0; j < areapos[areaID][0][1]/8; ++j)
			{
				outputCode |= *pos++;  // 读取一个字节
				
				// 将这个字节的8个位写入图像
				for (int k = areapos[areaID][1][1]; k < areapos[areaID][1][1]+8; ++k)
				{
					// outputCode & 1 取出最低位
					// 如果是1，用白色；如果是0，用黑色
					mat.at<Vec3b>(i+areapos[areaID][1][0], j*8+k) = pixel[(outputCode&1)?7:0];
					outputCode >>= 1;  // 右移一位，处理下一位
				}
				
				if (pos == end) break;  // 数据写完了就退出
			}
			if (pos == end) break;
		}
#ifdef Code_DEBUG
		Show_Scale_Img(mat);
#endif
	}

	// ==================== 帧标志生成函数 ====================
	
	// 在二维码中写入帧标志（标识这是第几帧，是开始还是结束）
	void BulidFrameFlag(Mat& mat, FrameType frameType, int tailLen)
	{
		// 根据帧类型设置前4个点
		switch (frameType)
		{
		case FrameType::Start:  // 起始帧：白白黑黑
			mat.at<Vec3b>(QrPointSize, SafeAreaWidth) = pixel[White];
			mat.at<Vec3b>(QrPointSize, SafeAreaWidth + 1) = pixel[White];
			mat.at<Vec3b>(QrPointSize, SafeAreaWidth + 2) = pixel[Black];
			mat.at<Vec3b>(QrPointSize, SafeAreaWidth + 3) = pixel[Black];
			break;
		case FrameType::End:  // 结束帧：黑黑白白
			mat.at<Vec3b>(QrPointSize, SafeAreaWidth) = pixel[Black];
			mat.at<Vec3b>(QrPointSize, SafeAreaWidth + 1) = pixel[Black];
			mat.at<Vec3b>(QrPointSize, SafeAreaWidth + 2) = pixel[White];
			mat.at<Vec3b>(QrPointSize, SafeAreaWidth + 3) = pixel[White];
			break;
		case FrameType::StartAndEnd:  // 既是开始也是结束：白白白白
			mat.at<Vec3b>(QrPointSize, SafeAreaWidth) = pixel[White];
			mat.at<Vec3b>(QrPointSize, SafeAreaWidth + 1) = pixel[White];
			mat.at<Vec3b>(QrPointSize, SafeAreaWidth + 2) = pixel[White];
			mat.at<Vec3b>(QrPointSize, SafeAreaWidth + 3) = pixel[White];
			break;
		default:  // 普通帧：黑黑黑黑
			mat.at<Vec3b>(QrPointSize, SafeAreaWidth) = pixel[Black];
			mat.at<Vec3b>(QrPointSize, SafeAreaWidth + 1) = pixel[Black];
			mat.at<Vec3b>(QrPointSize, SafeAreaWidth + 2) = pixel[Black];
			mat.at<Vec3b>(QrPointSize, SafeAreaWidth + 3) = pixel[Black];
			break;
		}
		
		// 写入剩余数据长度（12位，用12个点表示）
		for (int i = 4; i < 16; ++i)
		{
			mat.at<Vec3b>(QrPointSize, SafeAreaWidth + i) = pixel[(tailLen&1)?7:0];
			tailLen >>= 1;
		}
#ifdef Code_DEBUG
		Show_Scale_Img(mat);
#endif
	}

	// ==================== 单帧编码函数 ====================
	
	// 生成一个完整的二维码帧
	// 输入：帧类型、数据、数据长度、帧编号
	// 输出：108x108的二维码图像
	Mat CodeFrame(FrameType frameType, const char* info, int tailLen,int FrameNo)
	{
		// 创建一个白色的空白图像
		Mat codeMat = Mat(FrameSize, FrameSize, CV_8UC3,Vec3d(255,255,255));
		
		// 如果不是结束帧，数据长度就是满帧（1242字节）
		if (frameType != FrameType::End&&frameType!=FrameType::StartAndEnd) 
			tailLen = BytesPerFrame;
		
		// 1. 生成安全区（白色边框）
		BulidSafeArea(codeMat);
		
		// 2. 生成定位点（三个大方块 + 一个小方块）
		BulidQrPoint(codeMat);
		
		// 3. 计算校验码
		int checkCode=CalCheckCode((const unsigned char*)info, tailLen,
									frameType==FrameType::Start|| frameType == FrameType::StartAndEnd,
			                        frameType==FrameType::End|| frameType == FrameType::StartAndEnd,FrameNo);
		
		// 4. 生成帧标志
		BulidFrameFlag(codeMat, frameType, tailLen);
		
		// 5. 写入校验码和帧编号
		BulidCheckCodeAndFrameNo(codeMat, checkCode, FrameNo % 65536);
		
		// 6. 将数据编码到信息区域
		if (tailLen != BytesPerFrame) 
			tailLen = BytesPerFrame;
		for (int i = 0; i < RectAreaCount&&tailLen>0; ++i)
		{
			int lennow = std::min(tailLen, lenlim[i]); // lenlim[i]数据区域最大长度（第i个信息位区域）
			BulidInfoRect(codeMat, info, lennow,i);
			tailLen -= lennow;
			info += lennow;
		}
		
		return codeMat;
	}

	// ==================== 主编码函数 ====================
	
	// 将整个文件编码成多帧二维码
	// 输入：文件数据、数据长度、保存路径、图片格式、帧数限制
	void Main(const char* info, int len,const char * savePath,const char * outputFormat,int FrameCountLimit)
	{
		Mat output;
		char fileName[128];
		int counter = 0;
		
		// 如果帧数限制为0，直接返回
		if (FrameCountLimit == 0) return;
		
		// 情况1：文件为空，不处理
		if (len <= 0);
		
		// 情况2：文件很小，一帧就能装下
		else if (len <= BytesPerFrame)
		{
			// 创建一个缓冲区，填充满1242字节
			unsigned char BUF[BytesPerFrame + 5];
			memcpy(BUF, info, sizeof(unsigned char) * len);
			
			// 剩余部分用随机数填充
			for (int i = len; i <= BytesPerFrame; ++i)
				BUF[i] = rand() % 256;
			
			// 生成一个"既是开始也是结束"的帧
			output = ScaleToDisSize(CodeFrame(FrameType::StartAndEnd, (char*)BUF, len, 0));
			
			// 保存图片
			sprintf_s(fileName, "%s\\%05d.%s", savePath, counter++, outputFormat);
			imwrite(fileName, output);
		}
		
		// 情况3：文件很大，需要多帧
		else
		{
			int i = 0;
			len -= BytesPerFrame;
			
			// 生成第一帧（起始帧）
			output= ScaleToDisSize(CodeFrame(FrameType::Start, info, len, 0));
			--FrameCountLimit;
			
			// 保存第一帧
			sprintf_s(fileName, "%s\\%05d.%s", savePath, counter++, outputFormat);
			imwrite(fileName, output);

			// 生成中间帧和结束帧
			while (len > 0 && FrameCountLimit > 0)
			{
				info += BytesPerFrame;  // 移动到下一个1242字节
				--FrameCountLimit;
				
				// 如果还有数据，生成普通帧或结束帧
				if (len - BytesPerFrame > 0)
				{
					if (FrameCountLimit>0)
						output = ScaleToDisSize(CodeFrame(FrameType::Normal, info, BytesPerFrame, ++i));
					else 
						output = ScaleToDisSize(CodeFrame(FrameType::End, info, BytesPerFrame, ++i));
				}
				
				// 如果这是最后一帧
				else
				{
					unsigned char BUF[BytesPerFrame + 5];
					memcpy(BUF, info, sizeof(unsigned char) * len);
					for (int i = len; i <= BytesPerFrame; ++i)
						BUF[i] = rand() % 256;
					output = ScaleToDisSize(CodeFrame(FrameType::End, (char*)BUF, len, ++i));
				}
				
				len -= BytesPerFrame;
				
				// 保存图片
				sprintf_s(fileName, "%s\\%05d.%s", savePath, counter++, outputFormat);
				imwrite(fileName, output);
			};
		}
		return;
	}
}
