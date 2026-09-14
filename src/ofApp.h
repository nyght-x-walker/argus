#pragma once

#include "ofMain.h"
#include "PlateCandidateScorer.h"
#include <opencv2/opencv.hpp>
#include <opencv2/geometry.hpp>
#include "PlateOCR.h"
#include "PlateStraightener.h"

class ofApp : public ofBaseApp {

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
    void mouseScrolled(int x, int y, float scrollX, float scrollY) override;
    void mouseEntered(int x, int y) override;
    void mouseExited(int x, int y) override;
    void windowResized(int w, int h) override;
    void dragEvent(ofDragInfo dragInfo) override;
    void gotMessage(ofMessage msg) override;

    void drawImageInFrame(ofImage &image, float x, float y, float frameSize);

	PlateCandidateScorer plateScorer;
    PlateStraightener plateStraightener;
    PlateOCR plateOCR;
    std::string detectedText;

    ofImage selectedImage;
    ofImage grayPreview;
    ofImage blurredPreview;
    ofImage edgePreview;

	cv::Mat colorImage;
    cv::Mat grayImage;
    cv::Mat blurredImage;
    cv::Mat edgeImage;
	std::vector<cv::Rect> candidateBoxes;

    cv::Mat plateCrop;
    cv::Mat plateGray;

    cv::Mat plateOtsu;
    cv::Mat plateAdaptive;
    cv::Mat plateInverted;

    cv::Mat plateStraight;
    ofImage plateStraightPreview;

    ofImage platePreview;
    ofImage plateGrayPreview;
    ofImage plateOtsuPreview;
    ofImage plateAdaptivePreview;
    ofImage plateInvertedPreview;

    std::string resultOtsu;
    std::string resultAdaptive;
    std::string resultInverted;

	cv::Rect bestPlateBox;
	bool plateFound = false;

    bool imageLoaded = false;
    std::string selectedFilename;

    bool ocrReady = false;

};