// Argus license plate recognition and flagging system.
// Image loading, center viewport and basic docked panels.
#include "ofApp.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <unordered_map>

#include "imgui_internal.h"
#include "ofJson.h"

namespace
{

// Confidence slider range shown in the pipeline panel.
constexpr float MIN_CONFIDENCE = 0.5f;
constexpr float MAX_CONFIDENCE = 0.95f;

// Vertical space reserved below the image for the viewport toolbar.
constexpr float VIEWPORT_TOOLBAR_HEIGHT = 90.0f;

// Fallback image height when the reserved toolbar space does not fit.
constexpr float MIN_IMAGE_AREA_HEIGHT = 120.0f;

// Menu bar placement of the frame rate chip.
constexpr float MENU_FPS_OFFSET = 90.0f;
constexpr float MENU_FPS_MIN_X = 200.0f;

// Height of the operator notes input and console autoscroll margin.
constexpr float NOTES_INPUT_HEIGHT = 60.0f;
constexpr float CONSOLE_AUTOSCROLL_MARGIN = 20.0f;

// Overlay box colors for plain and watchlist-matched candidates.
const ofColor BOX_OK_COLOR(126, 202, 156);
const ofColor BOX_FLAG_COLOR(224, 108, 91);

// Vertical offset of the watchlist label below a matched box.
constexpr float FLAG_LABEL_OFFSET_Y = 14.0f;

// Column count of the watchlist table in the Flagged tab.
constexpr int FLAG_TABLE_COLUMNS = 5;

// Column count of the scan event table in the Logs tab.
constexpr int LOG_TABLE_COLUMNS = 7;

// Mean confidence below which a near-miss watchlist hit still counts.
constexpr float FUZZY_MAX_CONF = 85.0f;

// Startup offset so the first alert can fire immediately.
constexpr int ALERT_INIT_OFFSET_MINUTES = 10;

// Test log path for the logger self-check round-trip.
constexpr char LOGGER_TEST_LOG_PATH[] = "resources/test_logs.jsonl";

// Maps a flag verdict to its uppercase log label.
std::string flagTypeTag(argus::FlagType type)
{
    switch (type)
    {
    case argus::FlagType::Blocked:
        return "BLOCKED";
    case argus::FlagType::Authorized:
        return "AUTHORIZED";
    default:
        return "SUSPICIOUS";
    }
}

// Reads the last non-empty line from a text file.
bool readLastNonEmptyLine(const std::string& absolutePath, std::string& lastLine)
{
    std::ifstream input(absolutePath);
    if (!input.is_open())
    {
        return false;
    }
    std::string line;
    lastLine.clear();
    while (std::getline(input, line))
    {
        if (!line.empty())
        {
            lastLine = line;
        }
    }
    return !lastLine.empty();
}

// Checks one probe JSON line for the expected plate value.
bool isProbeLineValid(const std::string& lastLine)
{
    try
    {
        ofJson parsed = ofJson::parse(lastLine);
        return parsed.value("plateNorm", "") == "AB123CD";
    }
    catch (const std::exception&)
    {
        return false;
    }
}

// Maps a region verdict to its log label.
std::string regionName(argus::Region region)
{
    switch (region)
    {
    case argus::Region::EU:
        return "EU";
    case argus::Region::US:
        return "US";
    case argus::Region::UK:
        return "UK";
    default:
        return "Unknown";
    }
}

// Pixel size of the synthetic probe image used by the OCR checks.
constexpr int OCR_PROBE_SIZE = 10;

// Measured plate bounds in the sample image for the quality check.
constexpr float SAMPLE_PLATE_X = 231.0f;
constexpr float SAMPLE_PLATE_Y = 191.0f;
constexpr float SAMPLE_PLATE_W = 108.0f;
constexpr float SAMPLE_PLATE_H = 31.0f;

// First-run dock proportions: left, right and bottom panels.
constexpr float DOCK_LEFT_RATIO = 0.20f;
constexpr float DOCK_RIGHT_RATIO = 0.28f;
constexpr float DOCK_BOTTOM_RATIO = 0.28f;

// Docked window titles shared by layout building and drawing.
constexpr char DOCKSPACE_ID[] = "ArgusDockSpace";
constexpr char PIPELINE_WINDOW_TITLE[] = "Pipeline — ofApp::update()";
constexpr char VIEWPORT_WINDOW_TITLE[] = "Viewport — ofApp::draw()";
constexpr char INSPECTOR_WINDOW_TITLE[] = "Inspector — ofxImGui";
constexpr char CONSOLE_WINDOW_TITLE[] = "Console — ofLog";

// Sample image bundled for checks and one-click demo loading.
constexpr char SAMPLE_IMAGE_PATH[] = "resources/images/car_01.jpg";

// Lowercase extension without the dot, empty when missing.
std::string mediaExtension(const std::string& path)
{
    std::string ext = ofFilePath::getFileExt(path);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return ext;
}

// Still images accepted from drops, dialogs and samples.
bool isImagePath(const std::string& path)
{
    std::string ext = mediaExtension(path);
    return ext == "jpg" || ext == "jpeg" || ext == "png" || ext == "bmp";
}

// Video containers accepted from drops, dialogs and samples.
bool isVideoPath(const std::string& path)
{
    return mediaExtension(path) == "mp4";
}

// Majority plate across recent reads, first seen winning ties.
std::pair<std::string, int> majorityVote(const std::vector<std::string>& reads)
{
    std::unordered_map<std::string, int> counts;
    std::pair<std::string, int> best;
    for (const auto& read : reads)
    {
        int count = ++counts[read];
        if (count > best.second)
        {
            best = {read, count};
        }
    }
    return best;
}

// Loads the bundled sample into a local image for self-checks.
bool loadSampleImage(ofImage& sample, std::string& reason)
{
    if (sample.load(SAMPLE_IMAGE_PATH))
    {
        return true;
    }
    reason = "sample image missing";
    return false;
}

// True when every candidate sits inside the frame with a valid score.
bool candidatesAreSane(const std::vector<argus::PlateCandidate>& candidates, int width, int height)
{
    for (const auto& candidate : candidates)
    {
        bool inside = candidate.rect.x >= 0.0f && candidate.rect.y >= 0.0f &&
                      candidate.rect.x + candidate.rect.width <= width &&
                      candidate.rect.y + candidate.rect.height <= height;
        bool scored = candidate.confidence >= 0.0f && candidate.confidence <= 1.0f;
        if (!inside || !scored)
        {
            return false;
        }
    }
    return true;
}

} // namespace

void ofApp::logConsole(const std::string& message, const std::string& level)
{
    std::string line = "[" + ofGetTimestampString("%H:%M:%S") + "] [" + level + "] " + message;
    consoleLines.push_back(line);
    while (consoleLines.size() > MAX_CONSOLE_LINES)
    {
        consoleLines.erase(consoleLines.begin());
    }
}

bool ofApp::runStartupChecks()
{
    std::string reason;
    if (confidenceThreshold < MIN_CONFIDENCE || confidenceThreshold > MAX_CONFIDENCE)
    {
        reason = "threshold out of range";
    }
    else if (consoleLines.empty())
    {
        reason = "console log is empty";
    }

    // Mirror the verdict to stdout so headless runs can check it.
    if (!reason.empty())
    {
        logConsole("Startup checks: FAILED, " + reason, "ERROR");
        ofLogNotice("Startup") << "Startup checks: FAILED, " << reason;
        return false;
    }
    logConsole("Startup checks: OK", "INFO");
    ofLogNotice("Startup") << "Startup checks: OK";
    return true;
}

void ofApp::setup()
{
    ofSetWindowTitle("Argus – License Plate Recognition");
    ofSetWindowShape(1280, 720);
    ofSetVerticalSync(true);
    ofSetFrameRate(60);

    std::memset(notesBuffer, 0, sizeof(notesBuffer));
    bViewportRectValid = false;
    bDockLayoutBuilt = false;

    gui.setup();
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    logConsole("Argus OF v0.1.0, student project", "INFO");
    logConsole("Status: empty viewport, drop media or press O", "INFO");

    runStartupChecks();

    logConsole("PlateDetector initialized", "INFO");
    detector.minAreaFraction = detMinArea;
    detector.maxAreaFraction = detMaxArea;
    detector.minAspectRatio = detMinAR;
    detector.maxAspectRatio = detMaxAR;
    runDetectorChecks();
    runDetectionQualityChecks();

    if (ocr.isReady())
    {
        logConsole("PlateOCR initialized (eng, LSTM)", "INFO");
    }
    else
    {
        logConsole("PlateOCR engine unavailable", "ERROR");
    }
    ocr.minConfidence = ocrMinConf;
    ocr.preprocessEnable = ocrPreprocess;
    runOcrChecks();
    runOcrQualityChecks();

    logConsole("PlateValidator initialized", "INFO");
    runValidatorChecks();

    if (!ofFile::doesFileExist(flaggedJsonPath))
    {
        logConsole("[FlagStore] No flagged.json found, empty watchlist", "INFO");
    }
    else if (flagStore.load(flaggedJsonPath))
    {
        std::string count = ofToString(flagStore.entries().size());
        logConsole("[FlagStore] Loaded " + count + " entries from flagged.json", "INFO");
    }
    else
    {
        logConsole("[FlagStore] Failed to parse flagged.json", "ERROR");
    }
    runFlagChecks();

    lastAlertTime =
        std::chrono::steady_clock::now() - std::chrono::minutes(ALERT_INIT_OFFSET_MINUTES);
    logConsole("[AlertService] Initialized with 5min cooldown", "INFO");
    logConsole("[Logger] Will log to resources/logs.jsonl", "INFO");
    runAlertChecks();
    runLoggerChecks();
    runPipelineChecks();
    runMediaChecks();
}

void ofApp::runDetection()
{
    if (!img.isAllocated())
    {
        logConsole("PlateDetector: no image loaded", "ERROR");
        ofLogNotice("PlateDetector") << "no image loaded";
        return;
    }

    logConsole("[PlateDetector] Running detection on img_01.jpg", "INFO");
    applyScanResult(processFrame(img), "img_01.jpg");
}

ScanResult ofApp::processFrame(const ofImage& frame)
{
    ScanResult result;
    if (!frame.isAllocated())
    {
        return result;
    }
    try
    {
        result.candidates = detector.detect(frame);
    }
    catch (const std::exception&)
    {
        result.candidates.clear();
    }
    if (result.candidates.empty())
    {
        return result;
    }
    std::vector<argus::PlateCandidate> kept = result.candidates;
    result = voteReading(frame, result.candidates);
    result.candidates = kept;
    lookupWatchlistMatch(result);
    return result;
}

ScanResult ofApp::voteReading(const ofImage& frame,
                              const std::vector<argus::PlateCandidate>& candidates)
{
    ScanResult best;
    float bestScore = -1.0f;
    int voteCount = std::min(ocrVoteCount, static_cast<int>(candidates.size()));
    // Valid reads win, confidence breaks ties and salvages misses.
    for (int vote = 0; vote < voteCount; ++vote)
    {
        ScanResult attempt = readVoteBox(frame, candidates[vote]);
        float score = attempt.plateValid ? 1000.0f + attempt.ocr.meanConf : attempt.ocr.meanConf;
        if (score <= bestScore)
        {
            continue;
        }
        bestScore = score;
        best = attempt;
    }
    return best;
}

void ofApp::lookupWatchlistMatch(ScanResult& result)
{
    if (result.normalizedPlate.empty())
    {
        return;
    }
    result.match = flagStore.lookup(result.normalizedPlate);
    if (result.match.has_value() || result.ocr.meanConf >= FUZZY_MAX_CONF)
    {
        return;
    }
    // Low-confidence reads may err by one glyph, allow near miss.
    result.match = flagStore.lookupFuzzy(result.normalizedPlate);
}

ScanResult ofApp::readVoteBox(const ofImage& frame, const argus::PlateCandidate& box)
{
    ScanResult vote;
    vote.bestCandidate = box;
    vote.hasBest = true;
    float frameWidth = static_cast<float>(frame.getWidth());
    float frameHeight = static_cast<float>(frame.getHeight());
    float roiX = ofClamp(box.rect.x, 0.0f, frameWidth - 1.0f);
    float roiY = ofClamp(box.rect.y, 0.0f, frameHeight - 1.0f);
    float roiW = ofClamp(box.rect.width, 1.0f, frameWidth - roiX);
    float roiH = ofClamp(box.rect.height, 1.0f, frameHeight - roiY);
    vote.roiImage.cropFrom(frame, roiX, roiY, roiW, roiH);
    vote.ocr = ocr.recognize(vote.roiImage);
    vote.rawPlate = vote.ocr.text;
    std::string plain = validator.normalize(vote.rawPlate);
    vote.normalizedPlate = validator.repair(plain);
    vote.wasRepaired = (vote.normalizedPlate != plain);
    vote.plateValid = validator.isValid(vote.normalizedPlate, vote.region);
    return vote;
}

void ofApp::applyScanResult(const ScanResult& result, const std::string& label)
{
    bDetectorRan = true;
    candidates = result.candidates;
    std::string found = "Found " + ofToString(candidates.size()) + " candidate(s)";
    logConsole("[PlateDetector] " + found, candidates.empty() ? "WARNING" : "INFO");
    ofLogNotice("PlateDetector") << found;
    if (pipelineDebug)
    {
        logCandidateDetails();
    }
    bHasBest = result.hasBest;
    bOcrRan = true;
    lastOcrResult = result.ocr;
    if (result.hasBest)
    {
        bestCandidate = result.bestCandidate;
        plateRoiImg = result.roiImage;
    }
    if (lastOcrResult.text.empty())
    {
        logConsole("[PlateOCR] empty result", "WARNING");
        ofLogNotice("PlateOCR") << "empty result";
    }
    else
    {
        std::string reading = "Recognized: '" + lastOcrResult.text + "' mean " +
                              ofToString(lastOcrResult.meanConf, 1);
        std::string level = lastOcrResult.meanConf < ocrReviewConf ? "WARNING" : "INFO";
        logConsole("[PlateOCR] " + reading, level);
        ofLogNotice("PlateOCR") << reading;
    }
    logValidationDetails(result);
    logFlagDetails(result);
    logPipelineSummary(result, label);
    checkAlert();
}

bool ofApp::runPipelineChecks()
{
    std::string reason;
    ofImage tinyImage;
    tinyImage.allocate(1, 1, OF_IMAGE_COLOR);
    ScanResult tinyResult = processFrame(tinyImage);
    if (!tinyResult.candidates.empty())
    {
        reason = "tiny frame returned candidates";
    }
    else
    {
        ofImage sample;
        loadSampleImage(sample, reason);
        if (reason.empty())
        {
            ScanResult sampleResult = processFrame(sample);
            bool countOk = !sampleResult.candidates.empty() && sampleResult.candidates.size() <= 5;
            bool textOk = sampleResult.ocr.text.size() >= 5 && sampleResult.ocr.text.size() <= 8;
            if (!countOk || !textOk)
            {
                reason = "sample frame result implausible";
            }
            // Report the full chain so headless runs show end-to-end health.
            logPipelineSummary(sampleResult, "check");
        }
    }

    // Mirror the verdict to stdout so headless runs can check it.
    if (!reason.empty())
    {
        logConsole("Pipeline checks: FAILED, " + reason, "ERROR");
        ofLogNotice("Pipeline") << "Pipeline checks: FAILED, " << reason;
        return false;
    }
    logConsole("Pipeline checks: OK", "INFO");
    ofLogNotice("Pipeline") << "Pipeline checks: OK";
    return true;
}

void ofApp::logCandidateDetails()
{
    for (const auto& candidate : candidates)
    {
        std::string detail =
            "candidate xywh=" + ofToString(candidate.rect.x, 0) + "," +
            ofToString(candidate.rect.y, 0) + "," + ofToString(candidate.rect.width, 0) + "," +
            ofToString(candidate.rect.height, 0) + " conf=" + ofToString(candidate.confidence, 2);
        logConsole("[Pipeline] " + detail, "INFO");
    }
}

void ofApp::logValidationDetails(const ScanResult& result)
{
    rawPlateText = result.rawPlate;
    normalizedPlateText = result.normalizedPlate;
    plateValid = result.plateValid;
    detectedRegion = result.region;
    bValidatorRan = true;
    if (normalizedPlateText.empty())
    {
        logConsole("[PlateValidator] normalization empty", "ERROR");
        ofLogNotice("PlateValidator") << "normalization empty";
        return;
    }
    logConsole("[PlateValidator] Raw: '" + rawPlateText + "' -> Normalized: '" +
                   normalizedPlateText + "'",
               "INFO");
    if (result.wasRepaired)
    {
        logConsole("[PlateValidator] Corrected -> '" + normalizedPlateText + "'", "INFO");
    }
    std::string verdict = std::string("Valid: ") + (plateValid ? "yes" : "no") +
                          " Region: " + regionName(detectedRegion);
    std::string level = "INFO";
    if (!plateValid && result.ocr.meanConf >= ocrReviewConf)
    {
        level = "WARNING";
    }
    logConsole("[PlateValidator] " + verdict, level);
    ofLogNotice("PlateValidator") << verdict;
}

void ofApp::logFlagDetails(const ScanResult& result)
{
    currentMatch.reset();
    if (normalizedPlateText.empty())
    {
        return;
    }
    currentMatch = result.match;
    bFlagRan = true;
    std::string outcome = currentMatch.has_value() ? "found" : "not found";
    logConsole("[FlagStore] Lookup '" + normalizedPlateText + "' -> " + outcome, "INFO");
    ofLogNotice("FlagStore") << "Lookup '" << normalizedPlateText << "' -> " << outcome;
    if (!currentMatch.has_value())
    {
        return;
    }
    const argus::FlagEntry& entry = currentMatch.value();
    std::string detail =
        "Type: " + argus::flagTypeToString(entry.type) + ", Reason: " + entry.reason;
    bool alertWorthy = entry.type != argus::FlagType::Authorized;
    logConsole("[FlagStore] " + detail, alertWorthy ? "WARNING" : "INFO");
    ofLogNotice("FlagStore") << detail;
}

void ofApp::logPipelineSummary(const ScanResult& result, const std::string& label)
{
    std::string summary = "[Pipeline] " + label +
                          ": candidates=" + ofToString(result.candidates.size()) + ", OCR='" +
                          result.ocr.text + "' conf=" + ofToString(result.ocr.meanConf, 1) +
                          " valid=" + (result.plateValid ? "Y" : "N") +
                          " flag=" + (result.match.has_value() ? "Y" : "N");
    logConsole(summary, "INFO");
    ofLogNotice("Pipeline") << summary;
}

bool ofApp::runValidatorChecks()
{
    std::string reason;
    bool normalizeOk = validator.normalize("ab123cd") == "AB123CD" &&
                       validator.normalize("aB0O1I") == "AB0011" &&
                       validator.normalize("!!!").empty();
    auto checkValid = [&](const std::string& text, bool want, argus::Region wantRegion)
    {
        argus::Region region = argus::Region::Unknown;
        return validator.isValid(text, region) == want && region == wantRegion;
    };
    bool validOk = normalizeOk && checkValid("AB123CD", true, argus::Region::EU) &&
                   checkValid("A1B", true, argus::Region::EU) &&
                   checkValid("ABC1234XY", true, argus::Region::EU) &&
                   checkValid("123", false, argus::Region::Unknown) &&
                   checkValid("ABCD12345", false, argus::Region::Unknown) &&
                   checkValid("", false, argus::Region::Unknown);
    bool repairOk = validator.repair("SN6GXMZ") == "SN66XMZ" &&
                    validator.repair("AB123CD") == "AB123CD" && validator.repair("").empty();
    if (!validOk || !repairOk)
    {
        reason = "fixed case mismatch";
    }

    // Mirror the verdict to stdout so headless runs can check it.
    if (!reason.empty())
    {
        logConsole("Validator checks: FAILED, " + reason, "ERROR");
        ofLogNotice("Validator") << "Validator checks: FAILED, " << reason;
        return false;
    }
    logConsole("Validator checks: OK", "INFO");
    ofLogNotice("Validator") << "Validator checks: OK";
    return true;
}

bool ofApp::runFlagChecks()
{
    std::string reason;
    argus::FlagStore probe;
    if (ofFile::doesFileExist(flaggedJsonPath))
    {
        if (!probe.load(flaggedJsonPath))
        {
            reason = "seed file failed to load";
        }
        else
        {
            std::optional<argus::FlagEntry> hit = probe.lookup("AB123CD");
            std::optional<argus::FlagEntry> miss = probe.lookup("ZZ999ZZ");
            bool hitOk = hit.has_value() && hit->type == argus::FlagType::Blocked;
            std::optional<argus::FlagEntry> nearHit = probe.lookupFuzzy("AB123CE");
            std::optional<argus::FlagEntry> farMiss = probe.lookupFuzzy("ZZ999ZZ");
            bool fuzzyOk = nearHit.has_value() && nearHit->type == argus::FlagType::Blocked &&
                           !farMiss.has_value();
            if (!hitOk || miss.has_value() || !fuzzyOk)
            {
                reason = "lookup mismatch on fixed cases";
            }
        }
    }
    else if (probe.lookup("AB123CD").has_value())
    {
        reason = "empty store returned a hit";
    }

    // Mirror the verdict to stdout so headless runs can check it.
    if (!reason.empty())
    {
        logConsole("Flag checks: FAILED, " + reason, "ERROR");
        ofLogNotice("Flag") << "Flag checks: FAILED, " << reason;
        return false;
    }
    logConsole("Flag checks: OK", "INFO");
    ofLogNotice("Flag") << "Flag checks: OK";
    return true;
}

std::string ofApp::decisionName(Decision decision)
{
    switch (decision)
    {
    case Decision::Allow:
        return "Allow";
    case Decision::Block:
        return "Block";
    default:
        return "Review";
    }
}

void ofApp::checkAlert()
{
    bAlertRan = true;
    bAlertActive = false;
    alertBannerText.clear();
    if (!currentMatch.has_value())
    {
        return;
    }
    auto now = std::chrono::steady_clock::now();
    double seconds = std::chrono::duration<double>(now - lastAlertTime).count();
    if (!alertService.shouldAlert(currentMatch.value(), seconds))
    {
        return;
    }
    // Cooldown passed, raise the banner and restart the window.
    const argus::FlagEntry& entry = currentMatch.value();
    alertBannerText = "FLAGGED PLATE DETECTED - " + argus::flagTypeToString(entry.type);
    bAlertActive = true;
    lastAlertTime = now;
    std::string warn = "[AlertService] FLAGGED " + entry.plate + " - " +
                       argus::flagTypeToString(entry.type) + " -> alert triggered";
    logConsole(warn, "WARNING");
    ofLogNotice("AlertService") << warn;
}

void ofApp::saveDecisionAndLog()
{
    operatorNotes = std::string(notesBuffer);
    argus::ScanEvent event;
    event.timestamp = ofGetTimestampString("%Y-%m-%d %H:%M:%S");
    event.imagePath = currentMediaPath.empty() ? "(none)" : currentMediaPath;
    event.plateRaw = rawPlateText;
    event.plateNorm = normalizedPlateText;
    event.ocrConf = lastOcrResult.meanConf;
    event.flagMatch = currentMatch.has_value();
    event.region = regionName(detectedRegion);
    event.decision = currentDecision;
    event.operatorNotes = operatorNotes;
    if (event.flagMatch)
    {
        event.flagType = flagTypeTag(currentMatch->type);
    }
    logger.log(event, scanLogPath);
    bLoggerRan = true;
    saveToastText = "Decision saved -> logs.jsonl";
    std::string info = "[Logger] Logged scan event for " + normalizedPlateText + " -> logs.jsonl";
    logConsole(info, "INFO");
    ofLogNotice("Logger") << info;
}

bool ofApp::runAlertChecks()
{
    std::string reason;
    argus::AlertService probe;
    argus::FlagEntry blocked{"AB123CD", argus::FlagType::Blocked, "probe", "", "", true};
    argus::FlagEntry authorized{"KL555MN", argus::FlagType::Authorized, "probe", "", "", true};
    argus::FlagEntry silent = blocked;
    silent.triggerAlert = false;
    if (!probe.shouldAlert(blocked, 1000.0))
    {
        reason = "alert-worthy blocked rejected";
    }
    else if (probe.shouldAlert(blocked, 10.0))
    {
        reason = "cooldown not enforced";
    }
    else if (probe.shouldAlert(authorized, 1000.0))
    {
        reason = "authorized raised alert";
    }
    else if (probe.shouldAlert(silent, 1000.0))
    {
        reason = "disabled trigger raised alert";
    }
    // Mirror the verdict to stdout so headless runs can check it.
    if (!reason.empty())
    {
        logConsole("Alert checks: FAILED, " + reason, "ERROR");
        ofLogNotice("Alert") << "Alert checks: FAILED, " << reason;
        return false;
    }
    logConsole("Alert checks: OK", "INFO");
    ofLogNotice("Alert") << "Alert checks: OK";
    return true;
}

bool ofApp::runLoggerChecks()
{
    std::string reason;
    if (!verifyLoggerRoundTrip(reason))
    {
        reason = "logger round-trip failed: " + reason;
    }
    // Mirror the verdict to stdout so headless runs can check it.
    if (!reason.empty())
    {
        logConsole("Logger checks: FAILED, " + reason, "ERROR");
        ofLogNotice("Logger") << "Logger checks: FAILED, " << reason;
        return false;
    }
    logConsole("Logger checks: OK", "INFO");
    ofLogNotice("Logger") << "Logger checks: OK";
    return true;
}

bool ofApp::verifyLoggerRoundTrip(std::string& reason)
{
    // Fixed probe event keeps the self-check deterministic.
    argus::ScanEvent probeEvent;
    probeEvent.timestamp = "2026-01-01 00:00:00";
    probeEvent.imagePath = SAMPLE_IMAGE_PATH;
    probeEvent.plateRaw = "ab123cd";
    probeEvent.plateNorm = "AB123CD";
    probeEvent.ocrConf = 92.5f;
    probeEvent.flagMatch = true;
    probeEvent.flagType = "BLOCKED";
    probeEvent.region = "EU";
    probeEvent.decision = "Review";
    probeEvent.operatorNotes = "alert probe";
    logger.log(probeEvent, LOGGER_TEST_LOG_PATH);

    // Read back the last line and re-parse it as JSON.
    std::string absolutePath = ofToDataPath(LOGGER_TEST_LOG_PATH, false);
    std::string lastLine;
    if (!readLastNonEmptyLine(absolutePath, lastLine))
    {
        reason = "test log missing or empty";
        return false;
    }
    if (!isProbeLineValid(lastLine))
    {
        reason = "invalid JSON line";
        return false;
    }
    std::remove(absolutePath.c_str());
    return true;
}

bool ofApp::runOcrChecks()
{
    std::string reason;
    if (!ocr.isReady())
    {
        reason = "engine unavailable";
    }
    else
    {
        ofImage probe;
        probe.allocate(OCR_PROBE_SIZE, OCR_PROBE_SIZE, OF_IMAGE_GRAYSCALE);
        argus::OcrResult probeResult = ocr.recognize(probe);
        if (probeResult.meanConf < 0.0f || probeResult.meanConf > 100.0f)
        {
            reason = "probe confidence out of range";
        }
        else if (plateRoiImg.isAllocated())
        {
            argus::OcrResult roiResult = ocr.recognize(plateRoiImg);
            if (roiResult.meanConf < 0.0f || roiResult.meanConf > 100.0f)
            {
                reason = "ROI confidence out of range";
            }
        }
    }

    // Mirror the verdict to stdout so headless runs can check it.
    if (!reason.empty())
    {
        logConsole("OCR checks: FAILED, " + reason, "ERROR");
        ofLogNotice("OCR") << "OCR checks: FAILED, " << reason;
        return false;
    }
    logConsole("OCR checks: OK", "INFO");
    ofLogNotice("OCR") << "OCR checks: OK";
    return true;
}

bool ofApp::runOcrQualityChecks()
{
    std::string reason;
    ofImage sample;
    loadSampleImage(sample, reason);
    if (reason.empty())
    {
        ofImage plateSample;
        plateSample.cropFrom(sample, SAMPLE_PLATE_X, SAMPLE_PLATE_Y, SAMPLE_PLATE_W,
                             SAMPLE_PLATE_H);
        argus::OcrResult sampleResult = ocr.recognize(plateSample);
        bool plausibleLength = sampleResult.text.size() >= 5 && sampleResult.text.size() <= 8;
        if (!plausibleLength || sampleResult.meanConf < ocrMinConf)
        {
            reason = "sample plate read implausible";
        }
    }

    // Mirror the verdict to stdout so headless runs can check it.
    if (!reason.empty())
    {
        logConsole("OCR quality checks: FAILED, " + reason, "ERROR");
        ofLogNotice("OCR") << "OCR quality checks: FAILED, " << reason;
        return false;
    }
    logConsole("OCR quality checks: OK", "INFO");
    ofLogNotice("OCR") << "OCR quality checks: OK";
    return true;
}

bool ofApp::runDetectorChecks()
{
    std::string reason;
    ofImage emptyImage;
    if (!detector.detect(emptyImage).empty())
    {
        reason = "unallocated image returned candidates";
    }
    else
    {
        ofImage tinyImage;
        tinyImage.allocate(1, 1, OF_IMAGE_COLOR);
        detector.detect(tinyImage);
        ofImage sample;
        loadSampleImage(sample, reason);
        if (reason.empty() && detector.detect(sample).empty())
        {
            reason = "no candidates on the sample image";
        }
    }

    // Mirror the verdict to stdout so headless runs can check it.
    if (!reason.empty())
    {
        logConsole("Detector checks: FAILED, " + reason, "ERROR");
        ofLogNotice("Detector") << "Detector checks: FAILED, " << reason;
        return false;
    }
    logConsole("Detector checks: OK", "INFO");
    ofLogNotice("Detector") << "Detector checks: OK";
    return true;
}

bool ofApp::runDetectionQualityChecks()
{
    std::string reason;
    ofImage tinyImage;
    tinyImage.allocate(10, 10, OF_IMAGE_COLOR);
    std::vector<argus::PlateCandidate> tinyOut = detector.detect(tinyImage);
    ofImage sample;
    loadSampleImage(sample, reason);
    std::vector<argus::PlateCandidate> sampleOut;
    if (reason.empty())
    {
        sampleOut = detector.detect(sample);
    }
    if (reason.empty() && (tinyOut.size() > 5 || sampleOut.empty() || sampleOut.size() > 5))
    {
        reason = "candidate count out of range";
    }
    else if (reason.empty() && !candidatesAreSane(sampleOut, sample.getWidth(), sample.getHeight()))
    {
        reason = "candidate out of bounds or unscored";
    }
    // Mirror the verdict to stdout so headless runs can check it.
    if (!reason.empty())
    {
        logConsole("Detection quality checks: FAILED, " + reason, "ERROR");
        ofLogNotice("Detector") << "Detection quality checks: FAILED, " << reason;
        return false;
    }
    logConsole("Detection quality checks: OK", "INFO");
    ofLogNotice("Detector") << "Detection quality checks: OK";
    return true;
}

void ofApp::update()
{
    if (mediaKind == MediaKind::Video)
    {
        videoPlayer.update();
        readVideoFrame();
    }
}

void ofApp::buildDockLayout(ImGuiID dockspaceId, const ImVec2& size)
{
    ImGui::DockBuilderRemoveNode(dockspaceId);
    ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspaceId, size);

    ImGuiID dockLeft = 0;
    ImGuiID dockRight = 0;
    ImGuiID dockBottom = 0;
    ImGuiID dockCenter = dockspaceId;
    ImGui::DockBuilderSplitNode(dockCenter, ImGuiDir_Left, DOCK_LEFT_RATIO, &dockLeft, &dockCenter);
    ImGui::DockBuilderSplitNode(dockCenter, ImGuiDir_Right, DOCK_RIGHT_RATIO, &dockRight,
                                &dockCenter);
    ImGui::DockBuilderSplitNode(dockCenter, ImGuiDir_Down, DOCK_BOTTOM_RATIO, &dockBottom,
                                &dockCenter);

    ImGui::DockBuilderDockWindow(PIPELINE_WINDOW_TITLE, dockLeft);
    ImGui::DockBuilderDockWindow(VIEWPORT_WINDOW_TITLE, dockCenter);
    ImGui::DockBuilderDockWindow(INSPECTOR_WINDOW_TITLE, dockRight);
    ImGui::DockBuilderDockWindow(CONSOLE_WINDOW_TITLE, dockBottom);
    ImGui::DockBuilderFinish(dockspaceId);
}

void ofApp::drawDockspace()
{
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGuiWindowFlags hostFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
                                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                                 ImGuiWindowFlags_NoDocking |
                                 ImGuiWindowFlags_NoBringToFrontOnFocus |
                                 ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBackground;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("ArgusDockSpaceHost", nullptr, hostFlags);

    ImGuiID dockspaceId = ImGui::GetID(DOCKSPACE_ID);
    if (!bDockLayoutBuilt)
    {
        buildDockLayout(dockspaceId, viewport->Size);
        bDockLayoutBuilt = true;
    }
    ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f));
    ImGui::End();
    ImGui::PopStyleVar(3);
}

void ofApp::drawMenuBar()
{
    if (!ImGui::BeginMainMenuBar())
    {
        return;
    }

    ImGui::Text("Argus");
    ImGui::SameLine();
    ImGui::TextDisabled("ofApp · 60fps · ofxImGui · OF 0.12");

    float fpsCursorX = ImGui::GetWindowWidth() - MENU_FPS_OFFSET;
    ImGui::SameLine(ofMax(fpsCursorX, MENU_FPS_MIN_X));
    ImGui::Text("%d fps", static_cast<int>(ofGetFrameRate()));
    ImGui::EndMainMenuBar();
}

void ofApp::handleRunAction()
{
    if (mediaKind == MediaKind::Video)
    {
        processVideoFrame();
        return;
    }
    runDetection();
}

void ofApp::handleFrameAction()
{
    if (mediaKind == MediaKind::Video)
    {
        processVideoFrame();
        return;
    }
    if (!img.isAllocated())
    {
        logConsole("No frame loaded", "ERROR");
        return;
    }
    applyScanResult(processFrame(img), "frame");
}

bool ofApp::loadMedia(const std::string& path)
{
    ofFile file(path);
    if (!file.exists())
    {
        logConsole("Media not found: " + path, "ERROR");
        ofLogNotice("Media") << "Media not found: " << path;
        return false;
    }
    frameVotes.clear();
    if (isImagePath(path))
    {
        return loadImageMedia(path);
    }
    if (isVideoPath(path))
    {
        return loadVideoMedia(path);
    }
    logConsole("Unsupported media type: " + path, "ERROR");
    ofLogNotice("Media") << "Unsupported media type: " << path;
    return false;
}

bool ofApp::loadImageMedia(const std::string& path)
{
    if (!img.load(path))
    {
        logConsole("Failed to load image: " + path, "ERROR");
        ofLogNotice("Media") << "Failed to load image: " << path;
        return false;
    }
    videoPlayer.close();
    mediaKind = MediaKind::Image;
    currentMediaPath = path;
    logConsole("Loaded image: " + path, "INFO");
    ofLogNotice("Media") << "Loaded image: " << path;
    return true;
}

bool ofApp::loadVideoMedia(const std::string& path)
{
    if (!videoPlayer.load(path))
    {
        logConsole("Failed to load video: " + path, "ERROR");
        ofLogNotice("Media") << "Failed to load video: " << path;
        return false;
    }
    mediaKind = MediaKind::Video;
    currentMediaPath = path;
    videoPlayer.play();
    logConsole("Loaded video: " + path, "INFO");
    ofLogNotice("Media") << "Loaded video: " << path;
    return true;
}

void ofApp::browseMedia()
{
    ofFileDialogResult picked = ofSystemLoadDialog("Load image or video", false, "resources/");
    if (!picked.bSuccess)
    {
        logConsole("Browse cancelled", "INFO");
        return;
    }
    loadMedia(picked.getPath());
}

const ofImage& ofApp::displayImage() const
{
    if (mediaKind == MediaKind::Video && frameImage.isAllocated())
    {
        return frameImage;
    }
    return img;
}

void ofApp::readVideoFrame()
{
    if (mediaKind != MediaKind::Video || !videoPlayer.isFrameNew())
    {
        return;
    }
    frameImage.setFromPixels(videoPlayer.getPixels());
}

void ofApp::processVideoFrame()
{
    if (mediaKind != MediaKind::Video || !frameImage.isAllocated())
    {
        logConsole("No video frame decoded yet", "ERROR");
        return;
    }
    ScanResult result = processFrame(frameImage);
    applyScanResult(result, "video");
    recordFrameVote(result.normalizedPlate);
}

void ofApp::recordFrameVote(const std::string& normalizedPlate)
{
    if (normalizedPlate.empty())
    {
        return;
    }
    frameVotes.push_back(normalizedPlate);
    while (frameVotes.size() > MAX_FRAME_VOTES)
    {
        frameVotes.erase(frameVotes.begin());
    }
    auto tally = majorityVote(frameVotes);
    std::string vote = "[Pipeline] Temporal vote: '" + tally.first + "' " +
                       ofToString(tally.second) + "/" + ofToString(frameVotes.size());
    logConsole(vote, "INFO");
    ofLogNotice("Pipeline") << vote;
}

bool ofApp::runMediaChecks()
{
    std::string reason;
    bool extOk = isImagePath("car.JPG") && isVideoPath("clip.mp4") && !isImagePath("clip.mp4") &&
                 !isVideoPath("car.jpg") && !isImagePath("notes.txt") && !isImagePath("");
    auto tally = majorityVote({"SN66XMZ", "SNG6XMZ", "SN66XMZ"});
    auto emptyTally = majorityVote({});
    auto tieTally = majorityVote({"AB12CD", "XY98ZT"});
    bool voteOk = tally.first == "SN66XMZ" && tally.second == 2 && emptyTally.second == 0 &&
                  tieTally.first == "AB12CD" && tieTally.second == 1;
    if (!extOk || !voteOk)
    {
        reason = "extension or tally mismatch";
    }
    else if (loadMedia("resources/no_such_file.jpg"))
    {
        reason = "missing file loaded";
    }

    // Mirror the verdict to stdout so headless runs can check it.
    if (!reason.empty())
    {
        logConsole("Media checks: FAILED, " + reason, "ERROR");
        ofLogNotice("Media") << "Media checks: FAILED, " << reason;
        return false;
    }
    logConsole("Media checks: OK", "INFO");
    ofLogNotice("Media") << "Media checks: OK";
    return true;
}

void ofApp::drawPipelinePanel()
{
    if (!showPipeline)
    {
        return;
    }

    ImGui::Begin(PIPELINE_WINDOW_TITLE, &showPipeline);
    ImGui::TextWrapped("Single OF window. Pipeline runs in update(), "
                       "draws in draw(). No page router.");
    ImGui::Separator();
    ImGui::BulletText("ImageSource");
    if (bDetectorRan)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.49f, 0.79f, 0.61f, 1.0f));
        ImGui::BulletText("PlateDetector -> active");
        ImGui::PopStyleColor();
    }
    else
    {
        ImGui::BulletText("PlateDetector");
    }
    if (bOcrRan)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.49f, 0.79f, 0.61f, 1.0f));
        ImGui::BulletText("PlateOCR -> active");
        ImGui::PopStyleColor();
    }
    else
    {
        ImGui::BulletText("PlateOCR");
    }
    if (bValidatorRan)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.49f, 0.79f, 0.61f, 1.0f));
        ImGui::BulletText("PlateValidator -> active");
        ImGui::PopStyleColor();
    }
    else
    {
        ImGui::BulletText("PlateValidator");
    }
    if (bFlagRan)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.49f, 0.79f, 0.61f, 1.0f));
        ImGui::BulletText("FlagStore -> active");
        ImGui::PopStyleColor();
    }
    else
    {
        ImGui::BulletText("FlagStore");
    }
    if (bAlertActive)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.42f, 0.36f, 1.0f));
        ImGui::BulletText("AlertService -> alert");
        ImGui::PopStyleColor();
    }
    else if (bAlertRan)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.49f, 0.79f, 0.61f, 1.0f));
        ImGui::BulletText("AlertService -> active");
        ImGui::PopStyleColor();
    }
    else
    {
        ImGui::BulletText("AlertService");
    }
    if (bLoggerRan)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.49f, 0.79f, 0.61f, 1.0f));
        ImGui::BulletText("Logger -> active");
        ImGui::PopStyleColor();
    }
    else
    {
        ImGui::BulletText("Logger");
    }
    ImGui::Separator();

    ImGui::SliderFloat("Confidence thr", &confidenceThreshold, MIN_CONFIDENCE, MAX_CONFIDENCE,
                       "%.2f");

    if (ImGui::Button("Run (R)"))
    {
        handleRunAction();
    }
    ImGui::SameLine();
    if (ImGui::Button("Load Media (O)"))
    {
        browseMedia();
    }
    ImGui::SameLine();
    if (ImGui::Button("Sample"))
    {
        loadMedia(SAMPLE_IMAGE_PATH);
    }
    std::string mediaLabel = currentMediaPath.empty() ? "(none - drop, browse or sample)"
                                                      : ofFilePath::getFileName(currentMediaPath);
    ImGui::Text("Media: %s", mediaLabel.c_str());
    ImGui::SameLine();
    if (ImGui::Button("Batch 10"))
    {
        logConsole("Batch 10, stub", "INFO");
    }
    if (ImGui::Button("Process Frame"))
    {
        handleFrameAction();
    }
    ImGui::SameLine();
    if (ImGui::Button("OCR fail"))
    {
        logConsole("Simulated OCR failure, stub", "WARNING");
    }

    ImGui::Separator();
    ImGui::Text("Loaded: 4 imgs · 0 vid");
    ImGui::Text("Video buf: 24 frames, double-buffer");
    ImGui::End();
}

void ofApp::drawViewportToolbar()
{
    ImGui::Separator();
    if (ImGui::Button("Play"))
    {
        if (mediaKind != MediaKind::Video)
        {
            logConsole("No video loaded", "INFO");
        }
        else if (videoPlayer.isPlaying())
        {
            videoPlayer.setPaused(true);
            logConsole("Video paused", "INFO");
        }
        else
        {
            videoPlayer.play();
            logConsole("Video playing", "INFO");
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Demo Flagged"))
    {
        logConsole("Demo flagged plate, stub", "WARNING");
    }
    ImGui::SameLine();
    if (ImGui::Button("Corrupt img"))
    {
        logConsole("Corrupt image rejected, stub", "ERROR");
    }
    ImGui::SameLine();
    ImGui::Checkbox("Show candidates", &showCandidates);
    ImGui::SameLine();
    ImGui::Text("f0001 · 00:00:00");
    ImGui::Text("mat 11.8 MB · 342 MB");
    ImGui::TextDisabled("Space play · seek · R re-run");
}

void ofApp::drawViewportPanel()
{
    if (!showImageViewer)
    {
        bViewportRectValid = false;
        return;
    }

    ImGui::Begin(VIEWPORT_WINDOW_TITLE, &showImageViewer);
    const ofImage& view = displayImage();
    if (!view.isAllocated())
    {
        bViewportRectValid = false;
        ImGui::Text("Drop an image or video, or press O to browse");
        ImGui::End();
        return;
    }

    // Reserve layout space, then draw the pixels with OF after gui.end().
    ImVec2 cursorPos = ImGui::GetCursorScreenPos();
    ImVec2 contentAvail = ImGui::GetContentRegionAvail();
    float imageAvailH = contentAvail.y - VIEWPORT_TOOLBAR_HEIGHT;
    if (imageAvailH < MIN_IMAGE_AREA_HEIGHT)
    {
        imageAvailH = contentAvail.y;
    }
    float imageWidth = static_cast<float>(view.getWidth());
    float imageHeight = static_cast<float>(view.getHeight());
    float fitScale = ofMin(contentAvail.x / imageWidth, imageAvailH / imageHeight);
    if (fitScale <= 0.0f)
    {
        fitScale = 1.0f;
    }
    float drawWidth = imageWidth * fitScale;
    float drawHeight = imageHeight * fitScale;
    float imageX = cursorPos.x + (contentAvail.x - drawWidth) * 0.5f;
    float imageY = cursorPos.y + (imageAvailH - drawHeight) * 0.5f;
    viewportImageRect.set(imageX, imageY, drawWidth, drawHeight);
    bViewportRectValid = true;
    ImGui::Dummy(ImVec2(contentAvail.x, imageAvailH));

    drawAlertBanner();
    drawViewportToolbar();
    ImGui::End();
}

void ofApp::drawAlertBanner()
{
    if (!bAlertActive || alertBannerText.empty())
    {
        return;
    }
    // Simple banner label over the viewport area.
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.42f, 0.36f, 1.0f));
    ImGui::TextWrapped("%s", alertBannerText.c_str());
    ImGui::PopStyleColor();
}

void ofApp::drawDecisionSection()
{
    if (ImGui::Button("Allow"))
    {
        currentDecision = decisionName(Decision::Allow);
        logConsole("Decision: Allow", "INFO");
    }
    ImGui::SameLine();
    if (ImGui::Button("Block"))
    {
        currentDecision = decisionName(Decision::Block);
        logConsole("Decision: Block", "WARNING");
    }
    ImGui::SameLine();
    if (ImGui::Button("Review"))
    {
        currentDecision = decisionName(Decision::Review);
        logConsole("Decision: Review", "INFO");
    }
    ImGui::Text("Current: %s", currentDecision.c_str());
    ImGui::InputTextMultiline("notes", notesBuffer, sizeof(notesBuffer),
                              ImVec2(-1.0f, NOTES_INPUT_HEIGHT));
    if (ImGui::Button("Save Decision & Log (Enter)"))
    {
        saveDecisionAndLog();
    }
    if (!saveToastText.empty())
    {
        ImGui::TextDisabled("%s", saveToastText.c_str());
    }
}

void ofApp::drawOcrChips()
{
    std::size_t count = std::min(lastOcrResult.text.size(), lastOcrResult.perCharConf.size());
    std::string chips;
    for (std::size_t i = 0; i < count; ++i)
    {
        if (i > 0)
        {
            chips += " · ";
        }
        chips += lastOcrResult.text[i];
        int percent = static_cast<int>(lastOcrResult.perCharConf[i]);
        chips += " " + ofToString(percent) + "%";
    }
    if (chips.empty())
    {
        ImGui::Text("Per-char confidences unavailable, showing mean.");
    }
    else
    {
        ImGui::TextWrapped("%s", chips.c_str());
    }
}

void ofApp::drawInspectorPanel()
{
    if (!showInspector)
    {
        return;
    }

    ImGui::Begin(INSPECTOR_WINDOW_TITLE, &showInspector);
    if (ImGui::CollapsingHeader("Match Info", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (currentMatch.has_value())
        {
            const argus::FlagEntry& entry = currentMatch.value();
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.49f, 0.79f, 0.61f, 1.0f));
            ImGui::Text("Watchlist Match: YES");
            ImGui::PopStyleColor();
            ImGui::Text("Flag type: %s", argus::flagTypeToString(entry.type).c_str());
            ImGui::Text("Reason: %s", entry.reason.c_str());
            ImGui::Text("Tags: %s", entry.tags.c_str());
        }
        else
        {
            ImGui::Text("Watchlist Match: NO");
            ImGui::Text("Flag type: -");
            ImGui::Text("Reason: -");
        }
        ImGui::Text("Detection candidates: %d", static_cast<int>(candidates.size()));
        ImGui::Text("Detected plate (raw): %s", rawPlateText.empty() ? "-" : rawPlateText.c_str());
        ImGui::Text("Normalized: %s",
                    normalizedPlateText.empty() ? "-" : normalizedPlateText.c_str());
        if (plateValid)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.49f, 0.79f, 0.61f, 1.0f));
            ImGui::Text("Valid: Yes");
            ImGui::PopStyleColor();
        }
        else
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.42f, 0.36f, 1.0f));
            ImGui::Text("Valid: No");
            ImGui::PopStyleColor();
        }
        ImGui::Text("Region: %s", regionName(detectedRegion).c_str());
    }
    if (ImGui::CollapsingHeader("OCR, per-char", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (lastOcrResult.text.empty())
        {
            ImGui::Text("Mean: -");
            ImGui::Text("No text recognized.");
        }
        else
        {
            ImGui::Text("Mean: %.1f%%", lastOcrResult.meanConf);
            drawOcrChips();
            ImGui::Text("Normalization: O->0, I->1, uppercase, strip.");
        }
    }
    if (ImGui::CollapsingHeader("Decision and notes", ImGuiTreeNodeFlags_DefaultOpen))
    {
        drawDecisionSection();
    }
    if (ImGui::CollapsingHeader("Memory (this view)"))
    {
        ImGui::Text("Image: 4.2 MB");
        ImGui::Text("Decoded mat: 11.8 MB");
        ImGui::Text("Video cache: 0 MB");
        if (ImGui::Button("Purge caches"))
        {
            logConsole("Purge caches, stub", "INFO");
        }
    }
    if (ImGui::CollapsingHeader("Flag editor"))
    {
        if (ImGui::Button("Add and Edit flagged"))
        {
            logConsole("Flag editor opened, stub", "INFO");
        }
    }
    ImGui::End();
}

void ofApp::drawConsoleTab()
{
    ImGui::BeginChild("ConsoleScroll", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);
    for (const auto& line : consoleLines)
    {
        if (line.find("[ERROR]") != std::string::npos)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.42f, 0.36f, 1.0f));
            ImGui::TextUnformatted(line.c_str());
            ImGui::PopStyleColor();
        }
        else if (line.find("[WARNING]") != std::string::npos)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.90f, 0.71f, 0.34f, 1.0f));
            ImGui::TextUnformatted(line.c_str());
            ImGui::PopStyleColor();
        }
        else
        {
            ImGui::TextUnformatted(line.c_str());
        }
    }
    bool nearBottom = ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - CONSOLE_AUTOSCROLL_MARGIN;
    if (nearBottom)
    {
        ImGui::SetScrollHereY(1.0f);
    }
    ImGui::EndChild();
}

void ofApp::drawFlaggedTab()
{
    std::vector<argus::FlagEntry> rows = flagStore.entries();
    if (rows.empty())
    {
        ImGui::Text("No flagged plates loaded");
        return;
    }
    ImGui::Columns(FLAG_TABLE_COLUMNS, "FlaggedColumns", true);
    ImGui::Text("Plate");
    ImGui::NextColumn();
    ImGui::Text("Type");
    ImGui::NextColumn();
    ImGui::Text("Reason");
    ImGui::NextColumn();
    ImGui::Text("Added");
    ImGui::NextColumn();
    ImGui::Text("Status");
    ImGui::NextColumn();
    ImGui::Separator();
    for (const auto& row : rows)
    {
        ImGui::Text("%s", row.plate.c_str());
        ImGui::NextColumn();
        ImGui::Text("%s", argus::flagTypeToString(row.type).c_str());
        ImGui::NextColumn();
        ImGui::Text("%s", row.reason.c_str());
        ImGui::NextColumn();
        ImGui::Text("%s", row.addedDate.c_str());
        ImGui::NextColumn();
        ImGui::Text("Active");
        ImGui::NextColumn();
    }
    ImGui::Columns(1);
}

void ofApp::drawLogsTab()
{
    if (logger.recentEvents.empty())
    {
        ImGui::Text("No log entries yet");
        return;
    }
    ImGui::Columns(LOG_TABLE_COLUMNS, "LogsColumns", true);
    drawLogsHeader();
    ImGui::Separator();
    for (const auto& event : logger.recentEvents)
    {
        drawLogsRow(event);
    }
    ImGui::Columns(1);
}

void ofApp::drawLogsHeader()
{
    ImGui::Text("Time");
    ImGui::NextColumn();
    ImGui::Text("Plate");
    ImGui::NextColumn();
    ImGui::Text("Conf");
    ImGui::NextColumn();
    ImGui::Text("Flag");
    ImGui::NextColumn();
    ImGui::Text("Type");
    ImGui::NextColumn();
    ImGui::Text("Decision");
    ImGui::NextColumn();
    ImGui::Text("Op");
    ImGui::NextColumn();
}

void ofApp::drawLogsRow(const argus::ScanEvent& event)
{
    // Highlight blocked and suspicious rows by severity.
    bool isBlocked = event.flagType == "BLOCKED";
    bool isSuspicious = event.flagType == "SUSPICIOUS";
    if (isBlocked)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.42f, 0.36f, 1.0f));
    }
    else if (isSuspicious)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.90f, 0.71f, 0.34f, 1.0f));
    }
    ImGui::Text("%s", event.timestamp.c_str());
    ImGui::NextColumn();
    ImGui::Text("%s", event.plateNorm.c_str());
    ImGui::NextColumn();
    ImGui::Text("%.1f", event.ocrConf);
    ImGui::NextColumn();
    ImGui::Text("%s", event.flagMatch ? "YES" : "NO");
    ImGui::NextColumn();
    ImGui::Text("%s", event.flagType.empty() ? "-" : event.flagType.c_str());
    ImGui::NextColumn();
    ImGui::Text("%s", event.decision.c_str());
    ImGui::NextColumn();
    ImGui::Text("%s", event.operatorNotes.c_str());
    ImGui::NextColumn();
    if (isBlocked || isSuspicious)
    {
        ImGui::PopStyleColor();
    }
}

void ofApp::drawConsolePanel()
{
    if (!showConsole)
    {
        return;
    }

    ImGui::Begin(CONSOLE_WINDOW_TITLE, &showConsole);
    if (ImGui::BeginTabBar("BottomTabs"))
    {
        if (ImGui::BeginTabItem("Console"))
        {
            drawConsoleTab();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Flagged"))
        {
            drawFlaggedTab();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Logs and Alerts"))
        {
            drawLogsTab();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Memory"))
        {
            ImGui::Text("Not implemented yet.");
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::End();
}

void ofApp::drawViewportImage()
{
    const ofImage& view = displayImage();
    if (!showImageViewer || !bViewportRectValid || !view.isAllocated())
    {
        return;
    }

    ofPushMatrix();
    view.draw(viewportImageRect.x, viewportImageRect.y, viewportImageRect.width,
              viewportImageRect.height);
    ofPopMatrix();

    ofPushStyle();
    ofNoFill();
    ofSetColor(255, 255, 255, 90);
    ofDrawRectangle(viewportImageRect);
    ofPopStyle();

    if (showCandidates)
    {
        drawCandidateOverlays();
    }
}

void ofApp::drawCandidateOverlays()
{
    const ofImage& view = displayImage();
    if (candidates.empty() || !view.isAllocated())
    {
        return;
    }

    float scaleX = viewportImageRect.width / static_cast<float>(view.getWidth());
    float scaleY = viewportImageRect.height / static_cast<float>(view.getHeight());
    ofPushStyle();
    ofNoFill();
    for (std::size_t i = 0; i < candidates.size(); ++i)
    {
        const argus::PlateCandidate& candidate = candidates[i];
        float boxX = viewportImageRect.x + candidate.rect.x * scaleX;
        float boxY = viewportImageRect.y + candidate.rect.y * scaleY;
        float boxW = candidate.rect.width * scaleX;
        float boxH = candidate.rect.height * scaleY;
        bool flagged = bHasBest && i == 0 && currentMatch.has_value();
        ofSetColor(flagged ? BOX_FLAG_COLOR : BOX_OK_COLOR);
        ofDrawRectangle(boxX, boxY, boxW, boxH);
        ofSetColor(255, 255, 255);
        int percent = static_cast<int>(candidate.confidence * 100.0f);
        std::string label = ofToString(percent) + "%";
        if (bHasBest && i == 0 && !lastOcrResult.text.empty())
        {
            label = lastOcrResult.text + " " + label;
        }
        ofDrawBitmapStringHighlight(label, boxX, boxY - 8.0f);
        if (flagged)
        {
            ofDrawBitmapString("FLAGGED", boxX, boxY + boxH + FLAG_LABEL_OFFSET_Y);
        }
        if (bHasBest && i == 0 && !normalizedPlateText.empty() &&
            normalizedPlateText != lastOcrResult.text)
        {
            std::string normLabel = normalizedPlateText + (plateValid ? " OK" : " ??");
            ofDrawBitmapString(normLabel, boxX, boxY - 24.0f);
        }
    }
    ofPopStyle();
}

void ofApp::draw()
{
    ofBackground(30, 30, 40);

    gui.begin();
    drawDockspace();
    drawMenuBar();
    drawPipelinePanel();
    drawViewportPanel();
    drawInspectorPanel();
    drawConsolePanel();
    gui.end();

    drawViewportImage();
}

void ofApp::exit()
{
    ofLog() << "ofApp::exit() called";
}

void ofApp::keyPressed(int key)
{
    if (key == 'r' || key == 'R')
    {
        handleRunAction();
    }
    if (key == 'o' || key == 'O')
    {
        browseMedia();
    }
}

void ofApp::keyReleased(int key)
{
    (void)key;
}

void ofApp::mouseMoved(int x, int y)
{
    (void)x;
    (void)y;
}

void ofApp::mouseDragged(int x, int y, int button)
{
    (void)x;
    (void)y;
    (void)button;
}

void ofApp::mousePressed(int x, int y, int button)
{
    (void)x;
    (void)y;
    (void)button;
}

void ofApp::mouseReleased(int x, int y, int button)
{
    (void)x;
    (void)y;
    (void)button;
}

void ofApp::mouseEntered(int x, int y)
{
    (void)x;
    (void)y;
}

void ofApp::mouseExited(int x, int y)
{
    (void)x;
    (void)y;
}

void ofApp::windowResized(int w, int h)
{
    (void)w;
    (void)h;
}

void ofApp::dragEvent(ofDragInfo dragInfo)
{
    for (const auto& path : dragInfo.files)
    {
        // First supported drop wins, matching the mockup drop zone.
        if (loadMedia(path))
        {
            return;
        }
    }
    logConsole("No supported media in drop", "WARNING");
}

void ofApp::gotMessage(ofMessage msg)
{
    (void)msg;
}
