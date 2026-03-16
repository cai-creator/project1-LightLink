#include "pic_v2.h"
#include <iostream>
#include <filesystem>

using namespace cv;
using namespace std;
namespace fs = std::filesystem;

int main() {
    string testDir = "asset";
    string outputDir = "test_output";

    if (!fs::exists(outputDir)) {
        fs::create_directory(outputDir);
    }

    vector<string> imageFiles = {
        "test.png",
        "test2.jpg",
        "test3.jpg",
        "jnsj.webp"
    };

    for (const auto& filename : imageFiles) {
        string imgPath = testDir + "/" + filename;
        cout << "Testing: " << imgPath << endl;

        Mat srcImg = imread(imgPath);
        if (srcImg.empty()) {
            cout << "  Failed to load image!" << endl;
            continue;
        }

        cout << "  Image size: " << srcImg.cols << "x" << srcImg.rows << endl;

        Mat dstImg;
        bool success = ImgPraseV2::Main(srcImg, dstImg);

        if (success) {
            cout << "  [SUCCESS] QR code detected!" << endl;
            string outPath = outputDir + "/v2_" + filename;
            imwrite(outPath, dstImg);
            cout << "  Saved to: " << outPath << endl;
        }
        else {
            cout << "  [FAILED] QR code not detected" << endl;

            Mat otsuResult = ImgPraseV2::preprocessImgV2_OTSU(srcImg);
            string debugPath = outputDir + "/debug_otsu_" + filename;
            imwrite(debugPath, otsuResult);

            Mat adaptiveResult = ImgPraseV2::preprocessImgV2_Adaptive(srcImg);
            debugPath = outputDir + "/debug_adaptive_" + filename;
            imwrite(debugPath, adaptiveResult);

            cout << "  Debug images saved to: " << outputDir << endl;
        }

        cout << "  ---" << endl;
    }

    cout << "Test completed!" << endl;
    return 0;
}
