// Argus optical character reading for plate regions.
// Cropped grayscale ROIs with plate-only alphabet and no dictionary.
#include "PlateOCR.h"

#include <tesseract/baseapi.h>

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

#include <cctype>

namespace
{

// Recognition language and single-line segmentation for plates.
constexpr char OCR_LANGUAGE[] = "eng";

// Plate alphabet, punctuation never belongs on a plate.
constexpr char PLATE_WHITELIST[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

// Short ROI height below which doubling helps the line finder.
constexpr int SMALL_ROI_HEIGHT = 50;

// Inner crop margin divisor trimming plate borders and tight edges.
constexpr int BORDER_MARGIN_DIVISOR = 10;

// Smallest usable margin so tiny ROIs keep their characters.
constexpr int MIN_BORDER_MARGIN = 1;

// Largest margin so small plates are not eaten away.
constexpr int MAX_BORDER_MARGIN = 4;

// Whitelist variables apply after init, dictionary flags before it.
constexpr char WHITELIST_KEY[] = "tessedit_char_whitelist";
constexpr char SYSTEM_DAWG_KEY[] = "load_system_dawg";
constexpr char FREQ_DAWG_KEY[] = "load_freq_dawg";
constexpr char DAWG_OFF[] = "0";

// Extra tessdata prefixes tried when the default lookup fails.
constexpr const char* FALLBACK_PREFIXES[] = {
    "/usr/share/tessdata",
    "/usr/share/tesseract-ocr/5/tessdata",
    "/usr/share/tesseract-ocr/4/tessdata",
};

// Builds a dense string without Tesseract padding whitespace.
std::string stripNoise(const std::string& raw)
{
    std::string clean;
    for (char glyph : raw)
    {
        unsigned char code = static_cast<unsigned char>(glyph);
        if (code >= 32 && code < 127 && std::isspace(code) == 0)
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

// Trims plate borders, then doubles tiny ROIs for the line finder.
void prepareRoi(const cv::Mat& gray, cv::Mat& ready)
{
    int margin = std::min(gray.cols, gray.rows) / BORDER_MARGIN_DIVISOR;
    margin = ofClamp(margin, MIN_BORDER_MARGIN, MAX_BORDER_MARGIN);
    cv::Rect inner(margin, margin, gray.cols - 2 * margin, gray.rows - 2 * margin);
    inner &= cv::Rect(0, 0, gray.cols, gray.rows);
    cv::Mat cropped = gray(inner).clone();
    if (cropped.rows < SMALL_ROI_HEIGHT)
    {
        cv::resize(cropped, ready, cv::Size(), 2.0, 2.0, cv::INTER_LINEAR);
        return;
    }
    ready = cropped;
}

// Averages symbol confidences for a self-computed reliability score.
float averageCharConf(const std::vector<float>& perChar)
{
    float total = 0.0f;
    for (float conf : perChar)
    {
        total += conf;
    }
    return total / static_cast<float>(perChar.size());
}

// Collects per-symbol confidences through the result iterator.
void collectCharConfidences(tesseract::TessBaseAPI& api, const std::string& text,
                            std::vector<float>& perChar)
{
    std::unique_ptr<tesseract::ResultIterator> iterator(api.GetIterator());
    if (iterator == nullptr)
    {
        perChar.assign(text.size(), 0.0f);
        return;
    }
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
            perChar.push_back(ofClamp(conf, 0.0f, 100.0f));
        }
    } while (iterator->Next(tesseract::RIL_SYMBOL));
    if (perChar.empty())
    {
        perChar.assign(text.size(), 0.0f);
    }
}

// Runs the engine on a prepared mat, returning stripped text.
bool readPreparedText(tesseract::TessBaseAPI& api, const cv::Mat& ready, std::string& text)
{
    // Fresh state stops previous ROIs leaking into this reading.
    api.Clear();
    api.SetImage(ready.data, ready.cols, ready.rows, 1, static_cast<int>(ready.step));
    if (api.Recognize(nullptr) != 0)
    {
        return false;
    }
    std::unique_ptr<char[]> out(api.GetUTF8Text());
    if (out == nullptr)
    {
        return false;
    }
    text = stripNoise(out.get());
    return true;
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
        // Recognition-time tuning applied after a working engine exists.
        api->SetPageSegMode(static_cast<tesseract::PageSegMode>(tesseractPsm));
        api->SetVariable(WHITELIST_KEY, PLATE_WHITELIST);
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
    // Dictionary flags are init-time, so they precede engine creation.
    api->SetVariable(SYSTEM_DAWG_KEY, DAWG_OFF);
    api->SetVariable(FREQ_DAWG_KEY, DAWG_OFF);
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
    cv::Mat ready = gray;
    if (preprocessEnable)
    {
        prepareRoi(gray, ready);
    }

    std::string rawText;
    if (!readPreparedText(*api, ready, rawText))
    {
        return result;
    }
    result.text = rawText;
    if (result.text.size() <= 1)
    {
        // Degenerate reads stay empty instead of confident garbage.
        result.text.clear();
        result.meanConf = 0.0f;
        return result;
    }

    collectCharConfidences(*api, result.text, result.perCharConf);
    // Word means stay near zero on LSTM reads, so average the symbols.
    result.meanConf = averageCharConf(result.perCharConf);
    return result;
}

} // namespace argus
