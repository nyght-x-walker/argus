// Argus plate candidate detection on OF images.
// Bilateral smoothing, Canny edges, contour blobs at two scales.
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
/// Narrow and wide closings catch small and large plates respectively.
class PlateDetector
{
public:
    /// Lower floor admits distant plates in 612px-wide samples.
    float minAreaFraction = 0.002f;
    float maxAreaFraction = 0.15f;
    float minAspectRatio = 2.0f;
    float maxAspectRatio = 6.0f;
    int cannyLow = 40;
    int cannyHigh = 120;

    /// Extra contour statistics on the console when enabled.
    bool debugMode = false;

    /// Detects up to MAX_CANDIDATES plate-like regions, best first.
    std::vector<PlateCandidate> detect(const ofImage& input);
};

} // namespace argus
