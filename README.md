# Project-1-LightLink

基于OpenCV和FFmpeg的视频编解码项目，用于计算机网络课程实验。

## 项目状态

- **当前阶段**：环境配置完成，图像预处理模块开发完毕，解码方案开发中
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
- [x] 图像预处理模块开发
- [ ] 编码器开发
- [ ] 解码器开发

## 图像预处理模块说明

### 功能概述

图像预处理模块位于 `src/pic.cpp` 和 `src/pic.h` 中，负责从视频帧中提取二维码区域。主要功能包括：

1. **图像预处理**：灰度化 → 高斯模糊 → 二值化
2. **轮廓检测**：找到所有可能的定位点轮廓
3. **定位点筛选**：根据黑白比例、大小、对称性等特征筛选出3个大定位点
4. **第四点计算**：根据3个定位点计算出第4个点（透视变换用）
5. **透视变换**：将斜的二维码矫正为正的108×108图像

### 与参考项目对比

本项目的图像预处理模块基于参考项目 [project-1-eg-lzz-master](https://github.com/....) 开发，在其基础上进行了以下改进：

#### 1. 代码结构优化

| 特性 | 参考项目 | 本项目 |
|------|----------|--------|
| 命名空间 | `ImgParse` | `ImgPrase` (注：拼写错误待修正) |
| 模块组织 | 混合在一个命名空间 | 使用 `helpFunction` 子命名空间 |
| 代码风格 | 部分注释 | 详细的函数注释 |

#### 2. 定位点验证增强

| 特性 | 参考项目 | 本项目 |
|------|----------|--------|
| 黑白比例验证 | ✅ 基础验证 | ✅ 基础验证 |
| 尺寸验证 | ✅ 基础验证 | ✅ 基础验证 |
| 对称性验证 | ❌ 无 | ✅ 新增 CheckQrSymmetry 函数 |
| 三层结构验证 | ❌ 无 | ✅ 新增边缘区域检查 |

**详细说明**：
- 本项目新增了 `CheckQrSymmetry` 函数，用于验证二维码定位点的对称性和中心特征
- 该函数检查定位点的：
  - 中心区域是否为黑色（定位点中心应该是黑色）
  - 水平和垂直对称性
  - 三层结构特征（中心黑、边缘白）

#### 3. 定位点组合验证

| 特性 | 参考项目 | 本项目 |
|------|----------|--------|
| 直角验证 | ✅ isRightAngleExist | ✅ isRightAngleExist |
| 面积方差验证 | ✅ Cal3NumVariance | ✅ Cal3NumVariance |
| 距离比例验证 | ❌ 无 | ✅ IsValidQrTriple 函数 |
| 角度关系验证 | 基础 | ✅ 使用余弦定理精确计算 |
| 大小比例验证 | ❌ 无 | ✅ 新增尺寸比例检查 |

**详细说明**：
- 本项目新增了 `IsValidQrTriple` 函数，用于验证三个定位点是否构成有效的二维码定位点组合
- 该函数检查：
  - 三个定位点之间的距离是否合理
  - 两个较短边是否大致相等（等腰直角三角形）
  - 是否存在接近90度的角
  - 三个定位点的大小是否相近

#### 4. 函数命名规范化

| 参考项目 | 本项目 |
|----------|--------|
| `ImgPreprocessing` | `preprocessImg` |
| `ScreenQrPoint` | `findPositionPoints` |
| `DumpExcessQrPoint` | 保留相同 |
| `AdjustPointsOrder` | 保留相同 |
| `CropParallelRect` | `cropParallelRect` |

#### 5. 主要算法流程对比

**参考项目流程**（三阶段处理）：
```
阶段一：一阶定位
  输入图像 → 预处理 → 轮廓检测 → 定位点筛选 → DumpExcess → 调整顺序
         → 计算第四点 → 一阶裁剪（粗定位）

阶段二：二阶裁剪
  一阶裁剪结果 → 再次预处理 → 轮廓检测 → 定位点筛选 → 二阶裁剪（精定位）

阶段三：三阶微调
  二阶裁剪结果 → 放大到1080×1080 → 二值化 → 边缘检测(FindConner) → 最终矫正
             → 再次二值化 → 调整大小到108×108 → 输出
```

**本项目流程**（单阶段处理）：
```
输入图像 → 预处理 → 轮廓检测 → 定位点筛选 → DumpExcess → 调整顺序
       → 计算第四点 → 透视变换 → 输出108×108图像
```

#### 6. 整体架构对比

| 阶段 | 参考项目 | 本项目 |
|------|----------|--------|
| 预处理 | ✅ 多模糊率尝试 | ✅ 多模糊率尝试 |
| 定位点检测 | ✅ 基础检测 | ✅ 基础检测 + 对称性验证 |
| 定位点筛选 | ✅ 基础筛选 | ✅ 基础筛选 + 组合有效性验证 |
| 一阶裁剪 | ✅ 有 | ❌ 无 |
| 二阶裁剪 | ✅ 有 | ❌ 无 |
| 三阶微调 | ✅ 有（FindConner + GetVec + Resize） | ❌ 无 |
| 最终输出 | 108×108 二值化图像 | 108×108 图像 |

#### 7. 关键函数对比

| 功能 | 参考项目 | 本项目 |
|------|----------|--------|
| 主函数 | `Main` (多重重载) | `Main` (单一定义) |
| 预处理 | `ImgPreprocessing` | `preprocessImg` |
| 定位点筛选 | `ScreenQrPoint` | `findPositionPoints` |
| 定位点验证 | `IsQrPoint` (黑白比例+尺寸) | `IsQrPoint` + `CheckQrSymmetry` |
| 三点有效性验证 | `DumpExcessQrPoint` (仅角度+方差) | `DumpExcessQrPoint` + `IsValidQrTriple` |
| 第四点计算 | `Adjust3PointsToParallelogram` + `AdjustForthPoint` | `adjustPositionPoints` |
| 透视变换 | `CropParallelRect` | `cropParallelRect` |
| 边缘检测 | `FindConner` (DFS算法) | ❌ 无 |
| 二值化 | `GetVec` | ❌ 无 |
| 大小调整 | `Resize` | ❌ 无 |

### 技术参数对比

| 参数 | 参考项目 | 本项目 |
|------|----------|--------|
| 最小定位点尺寸 | 10 像素 | 10 像素 |
| 定位点最大比例 | 0.25 | 0.25 |
| 长宽比范围 | 5/6 ~ 6/5 | 5/6 ~ 6/5 |
| 黑白比例范围 | 0.40 ~ 2.25 | 0.40 ~ 2.25 |
| 直角角度范围 | 75° ~ 105° | 75° ~ 105° |
| 模糊率参数 | 0.0005, 0.0000, 0.00025, 0.001, 0.0001 | 0.0005, 0.0000, 0.00025, 0.001, 0.0001 |
| 输出图像尺寸 | 108 × 108 | 108 × 108 |

### 后续改进方向

1. **修正拼写错误**：将 `ImgPrase` 修正为 `ImgParse`
2. **性能优化**：优化轮廓检测算法，提高处理速度
3. **鲁棒性增强**：增加对倾斜、模糊、遮挡二维码的处理能力
4. **错误处理**：完善错误处理机制，提供更详细的调试信息

## 编码器使用说明

### 运行步骤

1. **进入编码器目录**：
   ```bash
   cd bin
   ```

2. **使用编码器**：
   ```bash
   encode [测试图片] [视频存放处]
   ```

### 参数说明

- `[测试图片]`：输入图片的路径（支持相对路径）
- `[视频存放处]`：输出视频的路径（支持相对路径）

### 使用示例

```bash
# 示例：将 asset/test2.jpg 编码为 my_video/res.mp4  复制可用
encode asset/test2.jpg my_video/res.mp4
```

这会在 `my_video` 目录中生成编码后的视频文件 `res.mp4`。

### 注意事项

- 确保输入图片存在
- 确保输出目录存在（如果不存在会自动创建）
- 支持的图片格式：jpg、png、bmp等
- 输出视频格式：mp4

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
