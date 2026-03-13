# Project-1-LightLink

基于OpenCV和FFmpeg的视频编解码项目，用于计算机网络课程实验。

## 项目状态

- **当前阶段**：环境配置完成，编码，解码方案开发中，图像预处理敲了一遍但是定位不太行，测试结果返回了一张模糊的课程表
- **依赖项**：需要用户自行配置 OpenCV 和 FFmpeg

## 目录结构

```
Project-1-LightLink/
├── bin/                          # 运行时目录（暂时需用户创建）
│   ├── encoder.exe               # 编码器（待开发）
│   ├── decoder.exe               # 解码器（待开发）
│   ├── inputImg/                 # 输入图片目录
│   ├── inputVideo/               # 输入视频目录
│   ├── output/                   # 输出文件目录
│   ├── ffmpeg/                   # FFmpeg工具（需用户放置）
│   └── opencv_*.dll              # OpenCV库（需用户放置）
├── Project-1-LightLink/          # VS2026项目文件
│   ├── Project-1-LightLink.cpp   # 主源代码
│   ├── Project-1-LightLink.vcxproj
│   └── ...
├── src/                          # 源代码和依赖头文件
│   ├── opencv/                   # OpenCV头文件
│   │   └── build/
│   │       └── include/          # OpenCV头文件目录
│   └── utils/                    # 工具函数（待开发）
├── .gitignore
├── README.md
└── Project-1-LightLink.slnx
```

## 环境配置（拉取项目后必须操作）

### 第一步：下载依赖库

#### 1. 下载 OpenCV（版本 4.12.0）

- 官网：https://opencv.org/releases/
- 选择 Windows 版本
- 下载后解压，将以下文件复制到 `bin/` 目录：
  - `opencv_world4120.dll`（约60MB）
  - `opencv_videoio_ffmpeg4120_64.dll`

#### 2. 下载 FFmpeg（版本 8.0.1）

- 官网：https://ffmpeg.org/download.html
- 选择 "essentials" 版本 for Windows
- 下载后解压，将 `bin` 文件夹内的文件复制到 `bin/ffmpeg/` 目录

### 第二步：创建 bin 目录并放置文件

```
bin/
├── Project-1-LightLink.exe       # 编译后生成
├── ffmpeg/                       # FFmpeg目录
│   ├── ffmpeg.exe
│   ├── ffplay.exe
│   └── ffprobe.exe
├── opencv_world4120.dll          # OpenCV运行时
├── opencv_videoio_ffmpeg4120_64.dll
└── inputImg/                     # 输入图片目录
```

### 第三步：配置 Visual Studio 项目

1. 用 VS2026 打开 `Project-1-LightLink.slnx`
2. 右键项目 → 属性
3. 配置以下路径（Debug | x64）：

| 配置项 | 路径 |
|--------|------|
| 包含目录 | `$(SolutionDir)src\opencv\build\include` |
| 库目录 | `$(SolutionDir)src\opencv\build\x64\vc16\lib` |
| 附加依赖项 | `opencv_world4120.lib` |
| 输出目录 | `$(SolutionDir)bin\` |

### 第四步：编译运行

1. 生成 → 生成解决方案
2. 生成的 exe 会在 `bin/` 目录
3. 运行 `bin/Project-1-LightLink.exe` 测试环境

## 当前功能

- [x] OpenCV 环境配置
- [x] FFmpeg 环境配置
- [x] 图像预处理模块（pic.h, pic.cpp）
- [ ] 编码器开发
- [ ] 解码器开发

## 图像预处理模块说明

### 当前状态
- **问题**：pic.h 和 pic.cpp 目前不能准确定位到定位点
- **测试结果**：返回了一张模糊的课程表，说明定位算法需要改进

### 后续改进计划
1. **改进定位算法**：
   - 优化边缘检测算法，提高定位点识别的准确性
   - 增加定位点的特征提取和匹配逻辑
   - 处理不同光照和角度下的定位问题

2. **增强鲁棒性**：
   - 添加异常处理机制，处理定位失败的情况
   - 实现多尺度搜索，提高定位的可靠性

3. **性能优化**：
   - 优化图像处理流程，减少计算时间
   - 考虑使用并行处理提高处理速度

## 解码方案说明

参考学长项目（project-1-eg-lzz-master），解码方案核心参数：

| 参数 | 值 |
|------|-----|
| 每帧数据量 | 1242 字节 |
| 二维码尺寸 | 108 像素 |
| 数据区数量 | 7 个 |

## 开发注意事项

1. **不要提交 bin 目录** - 包含大量二进制文件，会导致 GitHub 推送失败
2. **使用 .gitignore** - 已配置自动忽略编译产物
3. **定期提交** - 源代码需要及时提交到 GitHub

## 依赖项下载链接

- OpenCV 4.12.0: https://opencv.org/releases/
- FFmpeg 8.0.1: https://github.com/BtbN/FFmpeg-Builds/releases

## 许可证

供学习使用
