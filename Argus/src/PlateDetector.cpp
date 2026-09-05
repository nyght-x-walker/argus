// Argus plate candidate detection on OF images.
// Contour heuristic stub over OpenCV edges.
#include "PlateDetector.h"

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
// OpenCV 5 moved contour helpers out of imgproc into geometry.
#if CV_VERSION_MAJOR >= 5
#include <opencv2/geometry.hpp>
#endif

#include <algorithm>
#include <cmath>

namespace
{

// Gaussian blur kernel width applied before edge detection.
constexpr int BLUR_KERNEL = 5;

// Upper bound on returned candidates, largest area first.
constexpr std::size_t MAX_CANDIDATES = 5;

// Aspect ratio scoring peaks at typical plate proportions.
constexpr float IDEAL_ASPECT = 3.5f;
constexpr float ASPECT_TOLERANCE = 1.5f;

// Confidence range for deterministic aspect scoring.
constexpr float BASE_CONFIDENCE = 0.6f;
constexpr float CONFIDENCE_RANGE = 0.3f;

// Converts any ofImage pixel format into an 8-bit grayscale Mat.
void toGrayscale(const ofImage& input, cv::Mat& gray)
{
    const ofPixels& pixels = input.getPixels();
    int width = input.getWidth();
    int height = input.getHeight();
    // Mat header borrows the pixels below, used read-only in this call.
    unsigned char* raw = const_cast<unsigned char*>(pixels.getData());
    if (pixels.getImageType() == OF_IMAGE_GRAYSCALE)
    {
        gray = cv::Mat(height, width, CV_8UC1, raw).clone();
        return;
    }
    int type = pixels.getImageType() == OF_IMAGE_COLOR_ALPHA ? CV_8UC4 : CV_8UC3;
    int code =
        pixels.getImageType() == OF_IMAGE_COLOR_ALPHA ? cv::COLOR_RGBA2GRAY : cv::COLOR_RGB2GRAY;
    cv::Mat color(height, width, type, raw);
    cv::cvtColor(color, gray, code);
}

// Scores aspect closeness to plate proportions in the unit range.
float scoreAspect(float aspect)
{
    float distance = std::fabs(aspect - IDEAL_ASPECT) / ASPECT_TOLERANCE;
    return ofClamp(1.0f - distance, 0.0f, 1.0f);
}

// Collects contours passing the area and aspect gates.
void collectCandidates(const std::vector<std::vector<cv::Point>>& contours, double imageArea,
                       float minAreaFraction, float maxAreaFraction, float minAspect,
                       float maxAspect, std::vector<argus::PlateCandidate>& out)
{
    for (const auto& contour : contours)
    {
        cv::Rect box = cv::boundingRect(contour);
        double areaFraction = cv::contourArea(contour) / imageArea;
        if (areaFraction < minAreaFraction || areaFraction > maxAreaFraction)
        {
            continue;
        }
        float aspect = static_cast<float>(box.width) / static_cast<float>(box.height);
        if (aspect < minAspect || aspect > maxAspect)
        {
            continue;
        }
        argus::PlateCandidate candidate;
        candidate.rect.set(static_cast<float>(box.x), static_cast<float>(box.y),
                           static_cast<float>(box.width), static_cast<float>(box.height));
        candidate.confidence = BASE_CONFIDENCE + CONFIDENCE_RANGE * scoreAspect(aspect);
        out.push_back(candidate);
    }
}

} // namespace

namespace argus
{

std::vector<PlateCandidate> PlateDetector::detect(const ofImage& input)
{
    std::vector<PlateCandidate> candidates;
    if (!input.isAllocated())
    {
        return candidates;
    }

    cv::Mat gray;
    toGrayscale(input, gray);
    if (gray.empty())
    {
        return candidates;
    }

    cv::Mat blurred;
    cv::GaussianBlur(gray, blurred, cv::Size(BLUR_KERNEL, BLUR_KERNEL), 0.0);
    cv::Mat edges;
    cv::Canny(blurred, edges, cannyLow, cannyHigh);

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(edges, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    double imageArea = static_cast<double>(gray.cols) * static_cast<double>(gray.rows);
    collectCandidates(contours, imageArea, minAreaFraction, maxAreaFraction, minAspect, maxAspect,
                      candidates);

    std::sort(candidates.begin(), candidates.end(),
              [](const PlateCandidate& left, const PlateCandidate& right)
              { return left.rect.getArea() > right.rect.getArea(); });
    if (candidates.size() > MAX_CANDIDATES)
    {
        candidates.resize(MAX_CANDIDATES);
    }
    return candidates;
}

} // namespace argus
