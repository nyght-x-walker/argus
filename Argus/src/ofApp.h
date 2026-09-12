// Argus license plate recognition and flagging system.
// Image loading, center viewport and basic docked panels.
#pragma once

#include "AlertService.h"
#include "FlagStore.h"
#include "Logger.h"
#include "PlateDetector.h"
#include "PlateOCR.h"
#include "PlateValidator.h"
#include "ofMain.h"
#include "ofxImGui.h"

#include <chrono>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

/// One pass through detect, read, validate and watchlist lookup.
/// Shared by still images and video frames alike.
struct ScanResult
{
    std::vector<argus::PlateCandidate> candidates;
    argus::PlateCandidate bestCandidate;
    bool hasBest = false;
    ofImage roiImage;
    argus::OcrResult ocr;
    std::string rawPlate;
    std::string normalizedPlate;
    bool plateValid = false;
    argus::Region region = argus::Region::Unknown;
    std::optional<argus::FlagEntry> match;
    bool wasRepaired = false;
};

/// ofApp hosts the single-window UI and the viewport state.
class ofApp : public ofBaseApp
{
public:
    void setup() override;
    void update() override;
    void draw() override;
    void exit() override;
    void keyPressed(int key) override;
    void keyReleased(int key) override;
    void mouseMoved(int x, int y) override;
    void mouseDragged(int x, int y, int button) override;
    void mousePressed(int x, int y, int button) override;
    void mouseReleased(int x, int y, int button) override;
    void mouseEntered(int x, int y) override;
    void mouseExited(int x, int y) override;
    void windowResized(int w, int h) override;
    void dragEvent(ofDragInfo dragInfo) override;
    void gotMessage(ofMessage msg) override;

    /// Image loaded from resources/images/car_01.jpg.
    ofImage img;

    /// Active media source driving the viewport and pipeline.
    enum class MediaKind
    {
        None,
        Image,
        Video
    };

    /// Current media kind, empty until the operator loads media.
    MediaKind mediaKind = MediaKind::None;

    /// Path of the loaded image or video for display and logging.
    std::string currentMediaPath;

    /// Video decoder behind the viewport in video mode.
    ofVideoPlayer videoPlayer;

    /// Latest decoded video frame feeding display and pipeline.
    ofImage frameImage;

    /// Recent normalized reads backing the temporal vote.
    std::vector<std::string> frameVotes;

    /// Cap keeping the temporal vote window bounded.
    static constexpr std::size_t MAX_FRAME_VOTES = 9;

    /// Loads an image or video path into the matching backend.
    bool loadMedia(const std::string& path);

    /// Loads a still image and parks the video decoder.
    bool loadImageMedia(const std::string& path);

    /// Loads a video file and starts playback.
    bool loadVideoMedia(const std::string& path);

    /// Opens the native file dialog and loads the chosen media.
    void browseMedia();

    /// Display pixels, video frame first when one is decoded.
    const ofImage& displayImage() const;

    /// Copies a fresh decoder frame into the frame image.
    void readVideoFrame();

    /// Runs the unified pipeline on the current video frame.
    void processVideoFrame();

    /// Records a read and logs the majority across recent frames.
    void recordFrameVote(const std::string& normalizedPlate);

    /// Visibility toggles for the docked panels.
    bool showImageViewer = true;
    bool showPipeline = true;
    bool showInspector = true;
    bool showConsole = true;

    /// Confidence threshold shared with the pipeline panel slider.
    float confidenceThreshold = 0.72f;

    /// Pipeline running flag reserved for later work.
    bool pipelineRunning = false;

    /// Upper bound for the in-memory console log.
    static constexpr std::size_t MAX_CONSOLE_LINES = 200;

    /// Capacity of the operator notes input buffer.
    static constexpr std::size_t NOTES_BUFFER_SIZE = 512;

    /// Timestamped in-memory console lines shown in the Console tab.
    std::vector<std::string> consoleLines;

    /// Appends a timestamped line and trims the log to its bound.
    void logConsole(const std::string& message, const std::string& level = "INFO");

    /// Runs the startup self checks and reports to the console.
    bool runStartupChecks();

    /// Heuristic plate detector over the loaded image.
    argus::PlateDetector detector;

    /// Tunable detector geometry applied during setup.
    float detMinArea = 0.002f;
    float detMaxArea = 0.15f;
    float detMinAR = 2.0f;
    float detMaxAR = 6.0f;

    /// Latest detection candidates in image coordinates.
    std::vector<argus::PlateCandidate> candidates;

    /// Toggles the candidate bounding box overlay.
    bool showCandidates = true;

    /// True once a detection run completed this session.
    bool bDetectorRan = false;

    /// Runs the unified pipeline on the loaded image.
    void runDetection();

    /// Runs the full chain on any frame without touching members.
    ScanResult processFrame(const ofImage& frame);

    /// Reads one candidate box into a votable scan result.
    ScanResult readVoteBox(const ofImage& frame, const argus::PlateCandidate& box);

    /// Votes across top boxes, valid reads winning over raw confidence.
    ScanResult voteReading(const ofImage& frame,
                           const std::vector<argus::PlateCandidate>& candidates);

    /// Exact watchlist hit first, one-glyph near miss for shaky reads.
    void lookupWatchlistMatch(ScanResult& result);

    /// Copies a scan result into members, logs it and checks alerts.
    void applyScanResult(const ScanResult& result, const std::string& label);

    /// Extra per-candidate lines in the pipeline log when enabled.
    bool pipelineDebug = false;

    /// Confidence below which reads and plates count as review material.
    float ocrReviewConf = 50.0f;

    /// Verifies detector behavior on sample and edge inputs.
    bool runDetectorChecks();

    /// Verifies candidate counts and bounds on fixed inputs.
    bool runDetectionQualityChecks();

    /// Tesseract reader over the cropped best candidate.
    argus::PlateOCR ocr;

    /// Reliability floor applied to the reader during setup.
    float ocrMinConf = 30.0f;

    /// Master switch for ROI preparation applied during setup.
    bool ocrPreprocess = true;

    /// Top confidence-ranked boxes read per frame for voting.
    int ocrVoteCount = 3;

    /// Latest OCR result for the best candidate.
    argus::OcrResult lastOcrResult;

    /// Largest candidate selected for reading.
    argus::PlateCandidate bestCandidate;

    /// True once a best candidate was selected this session.
    bool bHasBest = false;

    /// True once an OCR pass completed this session.
    bool bOcrRan = false;

    /// Cropped best candidate kept for reading and debugging.
    ofImage plateRoiImg;

    /// Verifies reader behavior on probe and ROI inputs.
    bool runOcrChecks();

    /// Verifies read quality on a fixed known plate crop.
    bool runOcrQualityChecks();

    /// Verifies the unified pipeline on fixed frame inputs.
    bool runPipelineChecks();

    /// Verifies media loading and vote tally on fixed inputs.
    bool runMediaChecks();

    /// Cleans OCR text and checks the EU generic shape.
    argus::PlateValidator validator;

    /// Raw OCR text feeding normalization.
    std::string rawPlateText;

    /// Normalized text shown in overlays and the inspector.
    std::string normalizedPlateText;

    /// EU shape verdict for the normalized text.
    bool plateValid = false;

    /// Region detected by the shape check.
    argus::Region detectedRegion = argus::Region::Unknown;

    /// True once a validation pass completed this session.
    bool bValidatorRan = false;

    /// Verifies normalizer behavior on fixed cases.
    bool runValidatorChecks();

    /// Plate watchlist loaded from the seed JSON file.
    argus::FlagStore flagStore;

    /// Watchlist hit for the latest normalized text, if any.
    std::optional<argus::FlagEntry> currentMatch;

    /// Watchlist path resolved through the OF data folder.
    std::string flaggedJsonPath = "resources/flagged.json";

    /// True once a watchlist lookup completed this session.
    bool bFlagRan = false;

    /// Verifies store load and lookup behavior on fixed cases.
    bool runFlagChecks();

    /// Cooldown gate for flagged plate banners.
    argus::AlertService alertService;

    /// Append-only store for operator scan decisions.
    argus::Logger logger;

    /// Last alert time used for cooldown measurement.
    std::chrono::steady_clock::time_point lastAlertTime;

    /// Current operator decision shown in the inspector.
    std::string currentDecision = "Review";

    /// Operator notes synced from the inspector input.
    std::string operatorNotes;

    /// Scan log path resolved through the OF data folder.
    std::string scanLogPath = "resources/logs.jsonl";

    /// True when the latest run raised an alert banner.
    bool bAlertActive = false;

    /// Banner text for the latest alert, empty when quiet.
    std::string alertBannerText;

    /// True once an alert evaluation completed this session.
    bool bAlertRan = false;

    /// True once a decision was saved this session.
    bool bLoggerRan = false;

    /// Toast shown after saving a decision.
    std::string saveToastText;

    /// Decision choices shared by buttons and logging.
    enum class Decision
    {
        Allow,
        Block,
        Review
    };

    /// Evaluates the watchlist hit against cooldown rules.
    void checkAlert();

    /// Builds a scan event and appends it to the log.
    void saveDecisionAndLog();

    /// Verifies alert gating on fixed cases.
    bool runAlertChecks();

    /// Verifies logger round-trip on fixed cases.
    bool runLoggerChecks();

    /// Writes and re-parses one probe event for the self-check.
    bool verifyLoggerRoundTrip(std::string& reason);

    /// Converts a decision choice to its log label.
    static std::string decisionName(Decision decision);

    /// Logs one line per candidate when pipeline debugging is on.
    void logCandidateDetails();

    /// Copies validation fields into members and logs the verdict.
    void logValidationDetails(const ScanResult& result);

    /// Copies the watchlist hit into members and logs the outcome.
    void logFlagDetails(const ScanResult& result);

    /// Logs the single-line frame summary for images and video.
    void logPipelineSummary(const ScanResult& result, const std::string& label);

    /// Runs the unified pipeline on the current image as a frame.
    void handleFrameAction();

    /// ImGui context backing all docked panels.
    ofxImGui::Gui gui;

    /// Viewport image rect captured from the ImGui layout.
    ofRectangle viewportImageRect;

    /// True once a valid viewport rect was captured this session.
    bool bViewportRectValid = false;

    /// True after the initial dock layout was built.
    bool bDockLayoutBuilt = false;

    /// Operator notes edited in the inspector panel.
    char notesBuffer[NOTES_BUFFER_SIZE];

private:
    /// Tiles the docked panels once so first run matches the mockup.
    void buildDockLayout(ImGuiID dockspaceId, const ImVec2& size);

    /// Draws the full-window dockspace host.
    void drawDockspace();

    /// Draws the top menu bar with the frame rate chip.
    void drawMenuBar();

    /// Draws the left pipeline panel with stub controls.
    void drawPipelinePanel();

    /// Draws the center viewport panel and captures its image rect.
    void drawViewportPanel();

    /// Draws the viewport toolbar row with stub buttons.
    void drawViewportToolbar();

    /// Draws the right inspector panel with collapsible sections.
    void drawInspectorPanel();

    /// Draws the decision buttons and notes input inside the inspector.
    void drawDecisionSection();

    /// Draws the bottom tabbed panel with the scrolling console.
    void drawConsolePanel();

    /// Draws the scrolling console lines with per-level colors.
    void drawConsoleTab();

    /// Draws the watchlist table inside the Flagged tab.
    void drawFlaggedTab();

    /// Draws the alert banner over the viewport when active.
    void drawAlertBanner();

    /// Draws recent scan events inside the Logs and Alerts tab.
    void drawLogsTab();

    /// Draws the column headers for the scan event table.
    void drawLogsHeader();

    /// Draws one scan event row with severity highlighting.
    void drawLogsRow(const argus::ScanEvent& event);

    /// Draws the loaded image into the captured viewport rect.
    void drawViewportImage();

    /// Draws candidate boxes mapped from image to viewport coordinates.
    void drawCandidateOverlays();

    /// Draws per-character confidence chips in the inspector.
    void drawOcrChips();

    /// Shared stub behind the Run button and the R key.
    void handleRunAction();
};
