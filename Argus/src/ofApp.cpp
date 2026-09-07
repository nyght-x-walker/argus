// Argus license plate recognition and flagging system.
// Image loading, center viewport and basic docked panels.
#include "ofApp.h"

#include <algorithm>
#include <cstring>

#include "imgui_internal.h"

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

// Low OCR mean threshold on the 0-100 Tesseract scale.
constexpr float LOW_OCR_CONFIDENCE = 50.0f;

// Overlay box colors for plain and watchlist-matched candidates.
const ofColor BOX_OK_COLOR(126, 202, 156);
const ofColor BOX_FLAG_COLOR(224, 108, 91);

// Vertical offset of the watchlist label below a matched box.
constexpr float FLAG_LABEL_OFFSET_Y = 14.0f;

// Column count of the watchlist table in the Flagged tab.
constexpr int FLAG_TABLE_COLUMNS = 5;

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

// Sample image
constexpr char SAMPLE_IMAGE_PATH[] = "resources/images/car_01.jpg";

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
    else if (ofFile::doesFileExist(SAMPLE_IMAGE_PATH) && !img.isAllocated())
    {
        reason = "image not allocated";
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
    logConsole("Status: image load and draw", "INFO");

    if (!img.load(SAMPLE_IMAGE_PATH))
    {
        logConsole("Failed to load car_01.jpg", "ERROR");
    }
    else
    {
        logConsole("Loaded car_01.jpg (" + ofToString(img.getWidth()) + "x" +
                       ofToString(img.getHeight()) + ")",
                   "INFO");
    }

    runStartupChecks();

    logConsole("PlateDetector initialized", "INFO");
    runDetectorChecks();

    if (ocr.isReady())
    {
        logConsole("PlateOCR initialized (eng, LSTM)", "INFO");
    }
    else
    {
        logConsole("PlateOCR engine unavailable", "ERROR");
    }
    runOcrChecks();

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
    try
    {
        candidates = detector.detect(img);
    }
    catch (const std::exception& error)
    {
        candidates.clear();
        logConsole(std::string("PlateDetector failed: ") + error.what(), "ERROR");
        ofLogNotice("PlateDetector") << "failed: " << error.what();
        return;
    }

    bDetectorRan = true;
    recognizeBestCandidate();

    std::string summary = "Found " + ofToString(candidates.size()) + " candidate(s)";
    logConsole("[PlateDetector] " + summary, candidates.empty() ? "WARNING" : "INFO");
    ofLogNotice("PlateDetector") << summary;
}

void ofApp::recognizeBestCandidate()
{
    bHasBest = false;
    if (candidates.empty() || !img.isAllocated())
    {
        return;
    }
    // Candidates arrive largest first, so the front box is the best.
    bestCandidate = candidates.front();
    bHasBest = true;

    float imageWidth = static_cast<float>(img.getWidth());
    float imageHeight = static_cast<float>(img.getHeight());
    float roiX = ofClamp(bestCandidate.rect.x, 0.0f, imageWidth - 1.0f);
    float roiY = ofClamp(bestCandidate.rect.y, 0.0f, imageHeight - 1.0f);
    float roiW = ofClamp(bestCandidate.rect.width, 1.0f, imageWidth - roiX);
    float roiH = ofClamp(bestCandidate.rect.height, 1.0f, imageHeight - roiY);
    plateRoiImg.cropFrom(img, roiX, roiY, roiW, roiH);

    lastOcrResult = ocr.recognize(plateRoiImg);
    bOcrRan = true;
    if (lastOcrResult.text.empty())
    {
        logConsole("[PlateOCR] empty result", "WARNING");
        ofLogNotice("PlateOCR") << "empty result";
    }
    else
    {
        std::string reading = "Recognized: '" + lastOcrResult.text + "' mean " +
                              ofToString(lastOcrResult.meanConf, 1);
        std::string level = lastOcrResult.meanConf < LOW_OCR_CONFIDENCE ? "WARNING" : "INFO";
        logConsole("[PlateOCR] " + reading, level);
        ofLogNotice("PlateOCR") << reading;
    }
    validatePlateText();
    lookupFlag();
}

void ofApp::validatePlateText()
{
    rawPlateText = lastOcrResult.text;
    normalizedPlateText = validator.normalize(rawPlateText);
    plateValid = validator.isValid(normalizedPlateText, detectedRegion);
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
    std::string verdict = std::string("Valid: ") + (plateValid ? "yes" : "no") +
                          " Region: " + regionName(detectedRegion);
    std::string level = "INFO";
    if (!plateValid && lastOcrResult.meanConf >= LOW_OCR_CONFIDENCE)
    {
        level = "WARNING";
    }
    logConsole("[PlateValidator] " + verdict, level);
    ofLogNotice("PlateValidator") << verdict;
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
    if (!validOk)
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

void ofApp::lookupFlag()
{
    currentMatch.reset();
    if (normalizedPlateText.empty())
    {
        return;
    }
    currentMatch = flagStore.lookup(normalizedPlateText);
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
            if (!hitOk || miss.has_value())
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
        if (img.isAllocated() && detector.detect(img).empty())
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

void ofApp::update()
{
    // Reserved hook for detection and OCR work.
    (void)pipelineRunning;
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
    runDetection();
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
    ImGui::BulletText("FlagStore");
    ImGui::BulletText("AlertService");
    ImGui::BulletText("Logger");
    ImGui::Separator();

    ImGui::SliderFloat("Confidence thr", &confidenceThreshold, MIN_CONFIDENCE, MAX_CONFIDENCE,
                       "%.2f");

    if (ImGui::Button("Run (R)"))
    {
        handleRunAction();
    }
    ImGui::SameLine();
    if (ImGui::Button("Batch 10"))
    {
        logConsole("Batch 10, stub", "INFO");
    }
    if (ImGui::Button("Process Frame"))
    {
        logConsole("Processing frame f0001, stub", "INFO");
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
        logConsole("Video play and pause, stub", "INFO");
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
    if (!img.isAllocated())
    {
        bViewportRectValid = false;
        ImGui::Text("No image loaded");
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
    float imageWidth = static_cast<float>(img.getWidth());
    float imageHeight = static_cast<float>(img.getHeight());
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

    drawViewportToolbar();
    ImGui::End();
}

void ofApp::drawDecisionSection()
{
    if (ImGui::Button("Allow"))
    {
        logConsole("Decision: Allow, stub", "INFO");
    }
    ImGui::SameLine();
    if (ImGui::Button("Block"))
    {
        logConsole("Decision: Block, stub", "WARNING");
    }
    ImGui::SameLine();
    if (ImGui::Button("Review"))
    {
        logConsole("Decision: Review, stub", "INFO");
    }
    ImGui::InputTextMultiline("notes", notesBuffer, sizeof(notesBuffer),
                              ImVec2(-1.0f, NOTES_INPUT_HEIGHT));
    if (ImGui::Button("Save Decision & Log (Enter)"))
    {
        logConsole("Decision saved, stub", "INFO");
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
            ImGui::Text("Not implemented yet.");
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
    if (!showImageViewer || !bViewportRectValid || !img.isAllocated())
    {
        return;
    }

    ofPushMatrix();
    img.draw(viewportImageRect.x, viewportImageRect.y, viewportImageRect.width,
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
    if (candidates.empty() || !img.isAllocated())
    {
        return;
    }

    float scaleX = viewportImageRect.width / static_cast<float>(img.getWidth());
    float scaleY = viewportImageRect.height / static_cast<float>(img.getHeight());
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
    (void)dragInfo;
}

void ofApp::gotMessage(ofMessage msg)
{
    (void)msg;
}
