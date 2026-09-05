// Argus license plate recognition and flagging system.
// Phase 2: image loading, center viewport and basic docked panels.
#pragma once

#include "ofMain.h"
#include "ofxImGui.h"

#include <cstddef>
#include <string>
#include <vector>

/// ofApp hosts the single-window UI and the Phase 2 viewport state.
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

    /// Visibility toggles for the docked panels.
    bool showImageViewer = true;
    bool showPipeline = true;
    bool showInspector = true;
    bool showConsole = true;

    /// Confidence threshold shared with the pipeline panel slider.
    float confidenceThreshold = 0.72f;

    /// Pipeline running flag reserved for later phases.
    bool pipelineRunning = false;

    /// Upper bound for the in-memory console log.
    static constexpr std::size_t MAX_CONSOLE_LINES = 200;

    /// Capacity of the operator notes input buffer.
    static constexpr std::size_t NOTES_BUFFER_SIZE = 512;

    /// Timestamped in-memory console lines shown in the Console tab.
    std::vector<std::string> consoleLines;

    /// Appends a timestamped line and trims the log to its bound.
    void logConsole(const std::string& message, const std::string& level = "INFO");

    /// Runs the Phase 2 startup self checks and reports to the console.
    bool runPhase2Tests();

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

    /// Draws the loaded image into the captured viewport rect.
    void drawViewportImage();

    /// Shared stub behind the Run button and the R key.
    void handleRunAction();
};
