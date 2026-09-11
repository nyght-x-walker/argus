// Argus optical character reading for plate regions.
// Tesseract LSTM wrapper with managed API lifetime.
#pragma once

#include "ofMain.h"

#include <memory>
#include <string>
#include <vector>

namespace tesseract
{
class TessBaseAPI;
} // namespace tesseract

namespace argus
{

/// Text read from one plate region with confidence scores.
struct OcrResult
{
    std::string text;
    float meanConf = 0.0f;
    std::vector<float> perCharConf;
};

/// PlateOCR recognizes plate text through a reused Tesseract instance.
/// Grayscale inner crop plus optional doubling feeds the LSTM engine.
class PlateOCR
{
public:
    PlateOCR();
    ~PlateOCR();

    PlateOCR(const PlateOCR&) = delete;
    PlateOCR& operator=(const PlateOCR&) = delete;

    /// Reliability floor, below which reads count as unusable.
    float minConfidence = 30.0f;

    /// Master switch for the crop and upscale preparation.
    bool preprocessEnable = true;

    /// Segmentation mode for short single-line plate text.
    int tesseractPsm = 7;

    /// True when the Tesseract engine initialized correctly.
    bool isReady() const;

    /// Reads text from a plate ROI, empty text when unreadable.
    OcrResult recognize(const ofImage& plateRoi);

private:
    /// Attempts engine init against one tessdata prefix.
    bool tryInit(const char* datapath);

    /// Closes the engine before releasing it.
    struct TessApiDeleter
    {
        void operator()(tesseract::TessBaseAPI* engine) const;
    };

    std::unique_ptr<tesseract::TessBaseAPI, TessApiDeleter> api;
    bool initialized = false;
};

} // namespace argus
