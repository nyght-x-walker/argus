// Argus license plate recognition and flagging system.
// Phase 2: image loading, center viewport and basic docked panels.
#include "ofApp.h"

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

bool ofApp::runPhase2Tests()
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
        logConsole("Phase 2 tests: FAILED, " + reason, "ERROR");
        ofLogNotice("Phase2") << "Phase 2 tests: FAILED, " << reason;
        return false;
    }
    logConsole("Phase 2 tests: OK", "INFO");
    ofLogNotice("Phase2") << "Phase 2 tests: OK";
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
    logConsole("Status: Phase 2, image load and draw", "INFO");

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

    runPhase2Tests();
}

void ofApp::update()
{
    // Pipeline detection and OCR land in later phases.
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
    logConsole("[update] ImageSource load img_01.jpg", "INFO");
    logConsole("[draw] pipeline stub complete", "INFO");
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
    ImGui::BulletText("PlateDetector");
    ImGui::BulletText("PlateOCR");
    ImGui::BulletText("PlateValidator");
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

void ofApp::drawInspectorPanel()
{
    if (!showInspector)
    {
        return;
    }

    ImGui::Begin(INSPECTOR_WINDOW_TITLE, &showInspector);
    if (ImGui::CollapsingHeader("Match Info", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Text("Watchlist: NO");
        ImGui::Text("Flag type: -");
        ImGui::Text("Reason: -");
    }
    if (ImGui::CollapsingHeader("OCR, per-char", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::TextWrapped("OCR not implemented yet, Phase 4.");
        ImGui::Text("Mean: -");
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
            ImGui::Text("Not implemented yet, future phase.");
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Logs and Alerts"))
        {
            ImGui::Text("Not implemented yet, future phase.");
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Memory"))
        {
            ImGui::Text("Not implemented yet, future phase.");
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
