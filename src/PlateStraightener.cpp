#include "PlateStraightener.h"

cv::Mat PlateStraightener::straighten(const cv::Mat &input) {

    if (input.empty()) {
        return input.clone();
    }

    // GRAYSCALE
    cv::Mat gray;
    cv::cvtColor(input, gray, cv::COLOR_RGB2GRAY);

    // BLUR
    cv::Mat blurred;
    cv::GaussianBlur(gray, blurred, cv::Size(3, 3), 0);

    // EDGES
    cv::Mat edges;
    cv::Canny(blurred, edges, 50, 150);

    // FIND CONTOURS
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(edges, contours, cv::RETR_LIST, cv::CHAIN_APPROX_SIMPLE);

    std::vector<cv::Point> bestQuad;
    double bestArea = 0.0;

    // LOOK FOR A FOUR-CORNERED PLATE SHAPE
    for (const auto &contour : contours) {

        double perimeter = cv::arcLength(contour, true);

        std::vector<cv::Point> approx;
        cv::approxPolyDP(contour, approx, 0.02 * perimeter, true);

        if (approx.size() != 4) {
            continue;
        }

        if (!cv::isContourConvex(approx)) {
            continue;
        }

        double area = std::abs(cv::contourArea(approx));

        if (area < 500.0) {
            continue;
        }

        cv::Rect box = cv::boundingRect(approx);

        float aspectRatio = static_cast<float>(box.width) / box.height;

        if (aspectRatio < 2.0f || aspectRatio > 7.0f) {
            continue;
        }

        if (area > bestArea) {
            bestArea = area;
            bestQuad = approx;
        }
    }

    // IF NO GOOD FOUR-CORNER SHAPE WAS FOUND,
    // USE THE OLD ROTATION METHOD
    if (bestQuad.empty()) {
        return rotateFallback(input);
    }

    // ORDER CORNERS
    std::vector<cv::Point2f> corners = orderCorners(bestQuad);

    cv::Point2f topLeft = corners[0];
    cv::Point2f topRight = corners[1];
    cv::Point2f bottomRight = corners[2];
    cv::Point2f bottomLeft = corners[3];

    // CALCULATE OUTPUT SIZE
    float topWidth = cv::norm(topRight - topLeft);
    float bottomWidth = cv::norm(bottomRight - bottomLeft);

    float leftHeight = cv::norm(bottomLeft - topLeft);
    float rightHeight = cv::norm(bottomRight - topRight);

    int outputWidth = static_cast<int>(std::max(topWidth, bottomWidth));
    int outputHeight = static_cast<int>(std::max(leftHeight, rightHeight));

    if (outputWidth <= 0 || outputHeight <= 0) {
        return rotateFallback(input);
    }

    // DESTINATION CORNERS
    std::vector<cv::Point2f> destination = {
        cv::Point2f(0.0f, 0.0f),
        cv::Point2f(static_cast<float>(outputWidth - 1), 0.0f),
        cv::Point2f(static_cast<float>(outputWidth - 1), static_cast<float>(outputHeight - 1)),
        cv::Point2f(0.0f, static_cast<float>(outputHeight - 1))
    };

    // PERSPECTIVE TRANSFORM
    cv::Mat transform = cv::getPerspectiveTransform(corners, destination);

    cv::Mat straightened;
    cv::warpPerspective(input, straightened, transform, cv::Size(outputWidth, outputHeight), cv::INTER_CUBIC, cv::BORDER_CONSTANT, cv::Scalar(255, 255, 255));

    return straightened;
}


std::vector<cv::Point2f> PlateStraightener::orderCorners(const std::vector<cv::Point> &points) {

    std::vector<cv::Point2f> ordered(4);

    float smallestSum = FLT_MAX;
    float largestSum = -FLT_MAX;

    float smallestDiff = FLT_MAX;
    float largestDiff = -FLT_MAX;

    for (const auto &point : points) {

        float sum = static_cast<float>(point.x + point.y);
        float diff = static_cast<float>(point.x - point.y);

        if (sum < smallestSum) {
            smallestSum = sum;
            ordered[0] = point;
        }

        if (diff > largestDiff) {
            largestDiff = diff;
            ordered[1] = point;
        }

        if (sum > largestSum) {
            largestSum = sum;
            ordered[2] = point;
        }

        if (diff < smallestDiff) {
            smallestDiff = diff;
            ordered[3] = point;
        }
    }

    return ordered;
}


cv::Mat PlateStraightener::rotateFallback(const cv::Mat &input) {

    cv::Mat gray;
    cv::cvtColor(input, gray, cv::COLOR_RGB2GRAY);

    cv::Mat blurred;
    cv::GaussianBlur(gray, blurred, cv::Size(3, 3), 0);

    cv::Mat edges;
    cv::Canny(blurred, edges, 50, 150);

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(edges, contours, cv::RETR_LIST, cv::CHAIN_APPROX_SIMPLE);

    std::vector<cv::Point> points;

    for (const auto &contour : contours) {

        cv::Rect box = cv::boundingRect(contour);

        if (box.width > 10 && box.height > 5) {
            points.insert(points.end(), contour.begin(), contour.end());
        }
    }

    if (points.empty()) {
        return input.clone();
    }

    cv::RotatedRect rotatedBox = cv::minAreaRect(points);

    float angle = rotatedBox.angle;

    if (rotatedBox.size.width < rotatedBox.size.height) {
        angle += 90.0f;
    }

    if (angle > 45.0f) {
        angle -= 90.0f;
    }

    if (angle < -45.0f) {
        angle += 90.0f;
    }

    cv::Point2f center(input.cols / 2.0f, input.rows / 2.0f);

    cv::Mat rotationMatrix = cv::getRotationMatrix2D(center, angle, 1.0);

    cv::Mat rotated;
    cv::warpAffine(input, rotated, rotationMatrix, input.size(), cv::INTER_CUBIC, cv::BORDER_CONSTANT, cv::Scalar(255, 255, 255));

    return rotated;
}