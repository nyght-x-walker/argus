// Argus plate candidate detection on OF images.
// Contour heuristic stub over OpenCV edges.
#pragma once

#include "ofMain.h"

#include <vector>

namespace argus
{

/// Single plate-like region with a heuristic confidence score.
struct PlateCandidate
{
    ofRectangle rect;
    float confidence = 0.0f;
};

/// PlateDetector finds plate-like rectangles with an edge heuristic.
class PlateDetector
{
public:
    /// Detects up to MAX_CANDIDATES plate-like regions, largest first.
    std::vector<PlateCandidate> detect(const ofImage& input);

private:
    // Lower floor admits small plates in 612x408 samples; the 0.5 percent
    // architecture floor targets full-HD frames instead.
    float minAreaFraction = 0.0005f;
    float maxAreaFraction = 0.15f;
    float minAspect = 2.0f;
    float maxAspect = 5.0f;
    int cannyLow = 50;
    int cannyHigh = 150;
};

} // namespace argus
