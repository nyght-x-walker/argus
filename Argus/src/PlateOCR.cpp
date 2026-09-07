// Argus optical character reading for plate regions.
// Tesseract LSTM wrapper with managed API lifetime.
#include "PlateOCR.h"

#include <tesseract/baseapi.h>

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

#include <cctype>

namespace
{

// Recognition language and single-line segmentation for plates.
constexpr char OCR_LANGUAGE[] = "eng";

// Extra tessdata prefixes tried when the default lookup fails.
constexpr const char* FALLBACK_PREFIXES[] = {
    "/usr/share/tessdata",
    "/usr/share/tesseract-ocr/5/tessdata",
    "/usr/share/tesseract-ocr/4/tessdata",
};

// Builds a dense string without Tesseract padding whitespace.
std::string stripWhitespace(const std::string& raw)
{
    std::string clean;
    for (char glyph : raw)
    {
        unsigned char code = static_cast<unsigned char>(glyph);
        if (std::isspace(code) == 0)
        {
            clean.push_back(glyph);
        }
    }
    return clean;
}

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

} // namespace

namespace argus
{

void PlateOCR::TessApiDeleter::operator()(tesseract::TessBaseAPI* engine) const
{
    if (engine != nullptr)
    {
        engine->End();
        delete engine;
    }
}

PlateOCR::PlateOCR()
{
    api.reset(new tesseract::TessBaseAPI());
    if (tryInit(nullptr))
    {
        initialized = true;
    }
    else
    {
        for (const char* prefix : FALLBACK_PREFIXES)
        {
            if (tryInit(prefix))
            {
                initialized = true;
                break;
            }
        }
    }
    if (initialized)
    {
        api->SetPageSegMode(tesseract::PSM_SINGLE_LINE);
    }
}

PlateOCR::~PlateOCR() = default;

bool PlateOCR::isReady() const
{
    return initialized && api != nullptr;
}

bool PlateOCR::tryInit(const char* datapath)
{
    api->End();
    return api->Init(datapath, OCR_LANGUAGE, tesseract::OEM_LSTM_ONLY) == 0;
}

OcrResult PlateOCR::recognize(const ofImage& plateRoi)
{
    OcrResult result;
    if (!isReady() || !plateRoi.isAllocated())
    {
        return result;
    }

    cv::Mat gray;
    toGrayscale(plateRoi, gray);
    if (gray.empty())
    {
        return result;
    }

    api->SetImage(gray.data, gray.cols, gray.rows, 1, static_cast<int>(gray.step));
    if (api->Recognize(nullptr) != 0)
    {
        return result;
    }

    std::unique_ptr<char[]> out(api->GetUTF8Text());
    if (out == nullptr)
    {
        return result;
    }
    result.text = stripWhitespace(out.get());
    if (result.text.empty())
    {
        return result;
    }

    int mean = api->MeanTextConf();
    result.meanConf = static_cast<float>(ofClamp(mean, 0, 100));

    std::unique_ptr<tesseract::ResultIterator> iterator(api->GetIterator());
    if (iterator != nullptr)
    {
        do
        {
            std::unique_ptr<char[]> symbol(iterator->GetUTF8Text(tesseract::RIL_SYMBOL));
            if (symbol == nullptr)
            {
                continue;
            }
            bool visible =
                symbol[0] != '\0' && std::isspace(static_cast<unsigned char>(symbol[0])) == 0;
            if (visible)
            {
                float conf = iterator->Confidence(tesseract::RIL_SYMBOL);
                result.perCharConf.push_back(ofClamp(conf, 0.0f, 100.0f));
            }
        } while (iterator->Next(tesseract::RIL_SYMBOL));
    }
    if (result.perCharConf.empty())
    {
        result.perCharConf.assign(result.text.size(), result.meanConf);
    }
    return result;
}

} // namespace argus
