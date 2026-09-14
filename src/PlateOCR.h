#pragma once

#include <string>
#include <opencv2/opencv.hpp>
#include <tesseract/baseapi.h>

class PlateOCR {

public:
    PlateOCR();
    ~PlateOCR();

    bool setup();
    std::string recognize(const cv::Mat &image);

private:
    tesseract::TessBaseAPI tess;
};




