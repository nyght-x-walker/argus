// Argus plate candidate detection on OF images.
// Contour blobs at two scales, ranked by interior text evidence.
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

// Contour batches shared by both scale passes.
using Contours = std::vector<std::vector<cv::Point>>;

// Bilateral window preserving plate borders while calming noise.
constexpr int BILATERAL_DIAMETER = 7;
constexpr double BILATERAL_SIGMA = 75.0;

// Narrow horizontal close joining strokes without merging distant plates.
constexpr int CLOSE_WIDTH = 7;
constexpr int CLOSE_HEIGHT = 3;

// Wide close joining whole character groups on larger plates.
constexpr int WIDE_CLOSE_WIDTH = 11;
constexpr int WIDE_CLOSE_HEIGHT = 5;

// Borderless inner rect margin for the text density check.
constexpr int TEXT_INNER_MARGIN = 2;

// Floor rejecting smooth panels, scale mapping dense text to one.
constexpr float MIN_TEXT_DENSITY = 0.06f;
constexpr float TEXT_DENSITY_SCALE = 0.30f;

// Aspect scoring peaks at typical plate proportions.
constexpr float IDEAL_ASPECT = 3.6f;
constexpr float ASPECT_TOLERANCE = 1.8f;

// Upper bound on returned candidates, best confidence first.
constexpr std::size_t MAX_CANDIDATES = 5;

// Overlap above which the weaker box is dropped.
constexpr float NMS_OVERLAP = 0.35f;

// Extent floor rejecting degenerate open contours.
constexpr float MIN_EXTENT = 0.1f;

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

// Smooths with edge-preserving filter, then extracts Canny edges.
void detectEdges(const cv::Mat& gray, int low, int high, cv::Mat& rawEdges)
{
    cv::Mat smooth;
    cv::bilateralFilter(gray, smooth, BILATERAL_DIAMETER, BILATERAL_SIGMA, BILATERAL_SIGMA);
    cv::Canny(smooth, rawEdges, low, high);
}

// Closes stroke gaps so characters and borders form single contours.
void closeEdges(const cv::Mat& rawEdges, int width, int height, cv::Mat& closed)
{
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(width, height));
    cv::morphologyEx(rawEdges, closed, cv::MORPH_CLOSE, kernel);
}

// Raw edge share inside the borderless inner rect, the text cue.
double interiorTextDensity(const cv::Mat& rawEdges, const cv::Rect& box)
{
    cv::Rect inner(box.x + TEXT_INNER_MARGIN, box.y + TEXT_INNER_MARGIN,
                   std::max(0, box.width - 2 * TEXT_INNER_MARGIN),
                   std::max(0, box.height - 2 * TEXT_INNER_MARGIN));
    inner &= cv::Rect(0, 0, rawEdges.cols, rawEdges.rows);
    if (inner.area() <= 0)
    {
        return 0.0;
    }
    return static_cast<double>(cv::countNonZero(rawEdges(inner))) / inner.area();
}

// Scores aspect closeness to plate proportions in the unit range.
float scoreAspect(float aspect)
{
    float distance = std::fabs(aspect - IDEAL_ASPECT) / ASPECT_TOLERANCE;
    return ofClamp(1.0f - distance, 0.0f, 1.0f);
}

// Weighs text evidence first and plate proportions second.
float scoreCandidate(float aspect, double innerDensity)
{
    float textScore = ofClamp(static_cast<float>(innerDensity / TEXT_DENSITY_SCALE), 0.0f, 1.0f);
    return 0.55f * textScore + 0.45f * scoreAspect(aspect);
}

// Stores one gated box with its confidence score.
void pushCandidate(const cv::Rect& box, double innerDensity,
                   std::vector<argus::PlateCandidate>& out)
{
    argus::PlateCandidate candidate;
    candidate.rect.set(static_cast<float>(box.x), static_cast<float>(box.y),
                       static_cast<float>(box.width), static_cast<float>(box.height));
    float aspect = static_cast<float>(box.width) / static_cast<float>(box.height);
    candidate.confidence = scoreCandidate(aspect, innerDensity);
    out.push_back(candidate);
}

// Collects closed-boundary blobs passing the geometry gates.
void collectBlobs(const Contours& contours, const cv::Mat& rawEdges, double imageArea,
                  float minAreaFraction, float maxAreaFraction, float minAspect, float maxAspect,
                  std::vector<argus::PlateCandidate>& out)
{
    for (const auto& contour : contours)
    {
        cv::Rect box = cv::boundingRect(contour);
        if (box.height <= 0)
        {
            continue;
        }
        double rectFraction = static_cast<double>(box.width) * box.height / imageArea;
        if (rectFraction < minAreaFraction || rectFraction > maxAreaFraction)
        {
            continue;
        }
        float aspect = static_cast<float>(box.width) / static_cast<float>(box.height);
        if (aspect < minAspect || aspect > maxAspect)
        {
            continue;
        }
        double rectArea = static_cast<double>(box.width) * box.height;
        if (cv::contourArea(contour) / rectArea < MIN_EXTENT)
        {
            continue;
        }
        double innerDensity = interiorTextDensity(rawEdges, box);
        if (innerDensity < MIN_TEXT_DENSITY)
        {
            continue;
        }
        pushCandidate(box, innerDensity, out);
    }
}

// Overlap share of the smaller box covered by the intersection.
double overlapRatio(const cv::Rect& first, const cv::Rect& second)
{
    double inter = static_cast<double>((first & second).area());
    double smaller = static_cast<double>(std::min(first.area(), second.area()));
    return smaller > 0.0 ? inter / smaller : 0.0;
}

// Gathers blobs from narrow and wide closings into one list.
void collectScaleBlobs(const cv::Mat& rawEdges, double imageArea, float minAreaFraction,
                       float maxAreaFraction, float minAspect, float maxAspect,
                       std::vector<argus::PlateCandidate>& out)
{
    cv::Mat closedNarrow;
    closeEdges(rawEdges, CLOSE_WIDTH, CLOSE_HEIGHT, closedNarrow);
    cv::Mat closedWide;
    closeEdges(rawEdges, WIDE_CLOSE_WIDTH, WIDE_CLOSE_HEIGHT, closedWide);
    Contours contours;
    cv::findContours(closedNarrow, contours, cv::RETR_LIST, cv::CHAIN_APPROX_SIMPLE);
    Contours wideContours;
    cv::findContours(closedWide, wideContours, cv::RETR_LIST, cv::CHAIN_APPROX_SIMPLE);
    collectBlobs(contours, rawEdges, imageArea, minAreaFraction, maxAreaFraction, minAspect,
                 maxAspect, out);
    collectBlobs(wideContours, rawEdges, imageArea, minAreaFraction, maxAreaFraction, minAspect,
                 maxAspect, out);
}

// Drops weaker boxes overlapping a stronger one.
void suppressOverlaps(std::vector<argus::PlateCandidate>& candidates)
{
    std::vector<argus::PlateCandidate> kept;
    for (const auto& candidate : candidates)
    {
        cv::Rect box(static_cast<int>(candidate.rect.x), static_cast<int>(candidate.rect.y),
                     static_cast<int>(candidate.rect.width),
                     static_cast<int>(candidate.rect.height));
        bool overlaps = false;
        for (const auto& keep : kept)
        {
            cv::Rect keptBox(static_cast<int>(keep.rect.x), static_cast<int>(keep.rect.y),
                             static_cast<int>(keep.rect.width), static_cast<int>(keep.rect.height));
            if (overlapRatio(box, keptBox) > NMS_OVERLAP)
            {
                overlaps = true;
                break;
            }
        }
        if (!overlaps)
        {
            kept.push_back(candidate);
        }
    }
    candidates.swap(kept);
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

    cv::Mat rawEdges;
    detectEdges(gray, cannyLow, cannyHigh, rawEdges);
    double imageArea = static_cast<double>(gray.cols) * static_cast<double>(gray.rows);

    // Narrow pass keeps small plates separate, wide pass joins large ones.
    collectScaleBlobs(rawEdges, imageArea, minAreaFraction, maxAreaFraction, minAspectRatio,
                      maxAspectRatio, candidates);
    std::size_t blobCount = candidates.size();

    std::sort(candidates.begin(), candidates.end(),
              [](const PlateCandidate& left, const PlateCandidate& right)
              { return left.confidence > right.confidence; });
    suppressOverlaps(candidates);
    if (candidates.size() > MAX_CANDIDATES)
    {
        candidates.resize(MAX_CANDIDATES);
    }

    // Contour counts aid tuning without affecting the result.
    if (debugMode)
    {
        ofLogNotice("PlateDetector") << "blobs=" << blobCount;
    }
    return candidates;
}

} // namespace argus
