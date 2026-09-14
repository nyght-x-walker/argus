#pragma once

#include <opencv2/opencv.hpp>
#include <opencv2/geometry.hpp>

class PlateStraightener {

public:
    cv::Mat straighten(const cv::Mat &input);

private:
    std::vector<cv::Point2f> orderCorners(const std::vector<cv::Point> &points);
    cv::Mat rotateFallback(const cv::Mat &input);
};