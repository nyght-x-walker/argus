#include "PlateCandidateScorer.h"

float PlateCandidateScorer::score(
    const cv::Rect &box,
    const cv::Mat &edgeImage,
    const cv::Mat &colorImage,
    int imageWidth,
    int imageHeight
) {
    // ASPECT
    float aspectRatio = static_cast<float>(box.width) / box.height;
    float ratioScore = 1.0f - std::abs(aspectRatio - 4.0f) / 4.0f;
    ratioScore = std::max(0.0f, ratioScore);

    // SIZE
    float imageArea = static_cast<float>(imageWidth * imageHeight);
    float candidateArea = static_cast<float>(box.width * box.height);
    float relativeArea = candidateArea / imageArea;
    float sizeScore = 1.0f;

    if (relativeArea < 0.002f || relativeArea > 0.08f) {
        sizeScore = 0.0f;
    }

    // EDGE DENSITY
    cv::Mat edgeRegion = edgeImage(box);
    float edgePixels = static_cast<float>(cv::countNonZero(edgeRegion));
    float totalPixels = static_cast<float>(box.width * box.height);
    float edgeDensity = edgePixels / totalPixels;

    // GRAYSCALE REGION
    cv::Mat colorRegion = colorImage(box);
    cv::Mat grayRegion;
    cv::cvtColor(colorRegion, grayRegion, cv::COLOR_RGB2GRAY);

    // CONTRAST
    cv::Scalar mean;
    cv::Scalar stddev;
    cv::meanStdDev(grayRegion, mean, stddev);
    float contrastScore = static_cast<float>(stddev[0]) / 128.0f;
    contrastScore = std::min(contrastScore, 1.0f);

    // BRIGHT / DARK BALANCE
    cv::Mat brightMask;
    cv::Mat darkMask;
    cv::compare(grayRegion, 180, brightMask, cv::CMP_GT);
    cv::compare(grayRegion, 80, darkMask, cv::CMP_LT);

    float brightPixels = static_cast<float>(cv::countNonZero(brightMask));
    float darkPixels = static_cast<float>(cv::countNonZero(darkMask));

    float brightRatio = brightPixels / totalPixels;
    float darkRatio = darkPixels / totalPixels;

    float lightDarkScore = std::min(brightRatio, darkRatio) * 4.0f;
    lightDarkScore = std::min(lightDarkScore, 1.0f);

    // WHITE / NEUTRAL PIXELS
    cv::Mat hsvRegion;
    cv::cvtColor(colorRegion, hsvRegion, cv::COLOR_RGB2HSV);

    std::vector<cv::Mat> hsvChannels;
    cv::split(hsvRegion, hsvChannels);

    cv::Mat lowSaturation;
    cv::Mat highBrightness;
    cv::Mat whiteMask;

    cv::compare(hsvChannels[1], 80, lowSaturation, cv::CMP_LT);
    cv::compare(hsvChannels[2], 160, highBrightness, cv::CMP_GT);
    cv::bitwise_and(lowSaturation, highBrightness, whiteMask);

    float whitePixels = static_cast<float>(cv::countNonZero(whiteMask));
    float whiteRatio = whitePixels / totalPixels;

    // SCORE
    float totalScore =
        ratioScore * 2.0f +
        sizeScore * 1.0f +
        edgeDensity * 3.0f +
        contrastScore * 2.0f +
        lightDarkScore * 3.0f +
        whiteRatio * 1.0f;

    return totalScore;
}