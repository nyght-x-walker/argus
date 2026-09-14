#include "PlateOCR.h"

PlateOCR::PlateOCR() {

}

PlateOCR::~PlateOCR() {
    tess.End();
}

bool PlateOCR::setup() {

    if (tess.Init("C:/msys64/mingw64/share/tessdata", "eng")) {
        std::cout << "ERROR: Could not initialize Tesseract." << std::endl;
        return false;
    }

    // tess.SetPageSegMode(tesseract::PSM_SINGLE_LINE);
    // tess.SetPageSegMode(tesseract::PSM_SINGLE_BLOCK);
    tess.SetPageSegMode(tesseract::PSM_SPARSE_TEXT);
    // tess.SetPageSegMode(tesseract::PSM_RAW_LINE);
    
    tess.SetVariable("tessedit_char_whitelist", "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789");

    std::cout << "Tesseract initialized successfully." << std::endl;

    return true;
}

std::string PlateOCR::recognize(const cv::Mat &image) {

    if (image.empty()) {
        return "";
    }

    cv::Mat borderedImage;

    cv::copyMakeBorder(image,borderedImage,20,20,30,30,cv::BORDER_CONSTANT,cv::Scalar(255));

    tess.Clear();

    tess.SetImage(borderedImage.data,borderedImage.cols,borderedImage.rows,borderedImage.channels(),static_cast<int>(borderedImage.step));

    char *result = tess.GetUTF8Text();

    if (result == nullptr) {
        return "";
    }

    std::string text(result);

    delete[] result;

    return text;
}


