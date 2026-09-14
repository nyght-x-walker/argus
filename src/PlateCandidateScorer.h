#pragma once

#include <opencv2/opencv.hpp>

class PlateCandidateScorer {

public:

    float score(
        const cv::Rect &box,
        const cv::Mat &edgeImage,
        const cv::Mat &colorImage,
        int imageWidth,
        int imageHeight
    );
};