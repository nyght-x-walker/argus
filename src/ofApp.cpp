#include "ofApp.h"

//--------------------------------------------------------------
void ofApp::setup() {
    ocrReady = plateOCR.setup();
}

//--------------------------------------------------------------
void ofApp::update() {

}

//--------------------------------------------------------------
void ofApp::draw() {

    ofBackground(40);
    ofSetColor(255);

    if (imageLoaded) {

        float frameSize = 250.0f;
        float spacing = 20.0f;

        float startX = 20.0f;
        float startY = 40.0f;

        drawImageInFrame(selectedImage, startX, startY, frameSize);

        float imageScale = std::min(frameSize / selectedImage.getWidth(), frameSize / selectedImage.getHeight());

        float displayedWidth = selectedImage.getWidth() * imageScale;
        float displayedHeight = selectedImage.getHeight() * imageScale;

        float offsetX = startX + (frameSize - displayedWidth) / 2.0f;
        float offsetY = startY + (frameSize - displayedHeight) / 2.0f;

        ofNoFill();
        ofSetColor(255, 0, 0);

        for (const auto &box : candidateBoxes) {

            float x = offsetX + box.x * imageScale;
            float y = offsetY + box.y * imageScale;
            float w = box.width * imageScale;
            float h = box.height * imageScale;

            ofDrawRectangle(x, y, w, h);
        }
        
        if (plateFound) {

            float plateX = startX + (frameSize + spacing) * 3;
            float plateY = startY + frameSize + 20.0f;
            float plateWidth = 250.0f;
            float plateSpacing = 12.0f;

            float plateHeight = plateWidth * platePreview.getHeight() / platePreview.getWidth();

            platePreview.draw(plateX - 300, plateY, plateWidth, plateHeight);
            plateGrayPreview.draw(plateX - 300, plateY + plateHeight + plateSpacing, plateWidth, plateHeight);
            plateOtsuPreview.draw(plateX - 300, plateY + (plateHeight + plateSpacing) * 2, plateWidth, plateHeight);
            plateAdaptivePreview.draw(plateX,  plateY, plateWidth, plateHeight);
            plateInvertedPreview.draw(plateX, plateY + plateHeight + plateSpacing, plateWidth, plateHeight);
            plateStraightPreview.draw(plateX, plateY + (plateHeight + plateSpacing) * 2, plateWidth, plateHeight);
        }

        ofFill();
        ofSetColor(255);

        ofDrawBitmapString("OCR from straightened plate:", 20, 370);
        ofDrawBitmapString("Otsu: " + resultOtsu, 20, 390);
        ofDrawBitmapString("Adaptive: " + resultAdaptive, 20, 410);
        ofDrawBitmapString("Inverted: " + resultInverted, 20, 430);


        drawImageInFrame(grayPreview, startX + frameSize + spacing, startY, frameSize);
        drawImageInFrame(blurredPreview, startX + (frameSize + spacing) * 2, startY, frameSize);
        drawImageInFrame(edgePreview, startX + (frameSize + spacing) * 3, startY, frameSize);

        if (plateFound) {

            float plateX = startX + (frameSize + spacing) * 3;
            float plateY = startY + frameSize + 20.0f;
            float plateWidth = 250.0f;
            float plateSpacing = 15.0f;

            float plateHeight = plateWidth * platePreview.getHeight() / platePreview.getWidth();

            platePreview.draw(plateX, plateY, plateWidth, plateHeight);
            plateGrayPreview.draw(plateX, plateY + plateHeight + plateSpacing, plateWidth, plateHeight);
            //plateCleanPreview.draw(plateX, plateY + (plateHeight + plateSpacing) * 2, plateWidth, plateHeight);
        }

        float infoY = startY + frameSize + 20.0f;

        std::string info = "File: " + selectedFilename + "\nResolution: " + ofToString(selectedImage.getWidth()) + " x " + ofToString(selectedImage.getHeight());

        ofSetColor(255);
        ofDrawBitmapString("Press O to open another image", 20, 20);
        ofDrawBitmapString(info, startX, infoY);

        ofDrawBitmapString("Detected: " + detectedText, 20, 400);

    } else {

        ofDrawBitmapString("Press O to open an image", 20, 20);
    }
}

//--------------------------------------------------------------
void ofApp::drawImageInFrame(ofImage &image, float x, float y, float frameSize) {

    if (!image.isAllocated()) {
        return;
    }

    ofSetColor(0);
    ofDrawRectangle(x, y, frameSize, frameSize);

    float scale = std::min(frameSize / image.getWidth(), frameSize / image.getHeight());

    float drawWidth = image.getWidth() * scale;
    float drawHeight = image.getHeight() * scale;

    float drawX = x + (frameSize - drawWidth) / 2.0f;
    float drawY = y + (frameSize - drawHeight) / 2.0f;

    ofSetColor(255);
    image.draw(drawX, drawY, drawWidth, drawHeight);
}

//--------------------------------------------------------------
void ofApp::exit() {

}

//--------------------------------------------------------------
void ofApp::keyPressed(int key) {

    if (key == 'o' || key == 'O') {

        ofFileDialogResult result = ofSystemLoadDialog("Upload Image");

        if (result.bSuccess) {

            if (selectedImage.load(result.getPath())) {

                selectedFilename = ofFilePath::getFileName(result.getPath());

                ofPixels &pixels = selectedImage.getPixels();

                colorImage = cv::Mat(selectedImage.getHeight(), selectedImage.getWidth(), CV_8UC3, pixels.getData());

                cv::cvtColor(colorImage, grayImage, cv::COLOR_RGB2GRAY);
                grayPreview.setFromPixels(grayImage.data, grayImage.cols, grayImage.rows, OF_IMAGE_GRAYSCALE);

                cv::GaussianBlur(grayImage, blurredImage, cv::Size(5, 5), 0);
                blurredPreview.setFromPixels(blurredImage.data, blurredImage.cols, blurredImage.rows, OF_IMAGE_GRAYSCALE);

                cv::Canny(blurredImage, edgeImage, 50, 150);
                edgePreview.setFromPixels(edgeImage.data, edgeImage.cols, edgeImage.rows, OF_IMAGE_GRAYSCALE);

                candidateBoxes.clear();
                std::vector<std::vector<cv::Point>> contours;
                cv::findContours(edgeImage, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
                for (const auto &contour : contours) {

                    cv::Rect box = cv::boundingRect(contour);

                    float aspectRatio = static_cast<float>(box.width) / box.height;
                    int area = box.width * box.height;

                    if (box.width > 60 &&
                        box.height > 15 &&
                        aspectRatio > 2.0f &&
                        aspectRatio < 6.0f &&
                        area > 1500) {

                        candidateBoxes.push_back(box);
                    }
                }

                plateFound = false;
                float bestScore = -1.0f;

                for (const auto &box : candidateBoxes) {

                    float score = plateScorer.score(box, edgeImage, colorImage, selectedImage.getWidth(), selectedImage.getHeight());

                    if (!plateFound || score > bestScore) {
                        bestScore = score;
                        bestPlateBox = box;
                        plateFound = true;
                    }
                }

                if (plateFound) {

                    // CROP
                    plateCrop = colorImage(bestPlateBox).clone();

                    // STRAIGHTEN
                    plateStraight = plateStraightener.straighten(plateCrop);

                    // PREVIEWS: ORIGINAL + STRAIGHTENED
                    platePreview.setFromPixels(plateCrop.data, plateCrop.cols, plateCrop.rows, OF_IMAGE_COLOR);
                    plateStraightPreview.setFromPixels(plateStraight.data, plateStraight.cols, plateStraight.rows, OF_IMAGE_COLOR);

                    // GRAYSCALE
                    cv::cvtColor(plateStraight, plateGray, cv::COLOR_RGB2GRAY);
                    plateGrayPreview.setFromPixels(plateGray.data, plateGray.cols, plateGray.rows, OF_IMAGE_GRAYSCALE);

                    // PREPARE FOR OCR
                    cv::resize(plateGray, plateGray, cv::Size(), 3.0, 3.0, cv::INTER_CUBIC);
                    cv::GaussianBlur(plateGray, plateGray, cv::Size(3, 3), 0);

                    // CREATE OCR VERSIONS
                    cv::threshold(plateGray, plateOtsu, 0, 255, cv::THRESH_BINARY + cv::THRESH_OTSU);
                    cv::adaptiveThreshold(plateGray, plateAdaptive, 255, cv::ADAPTIVE_THRESH_GAUSSIAN_C, cv::THRESH_BINARY, 31, 11);
                    cv::bitwise_not(plateOtsu, plateInverted);

                    // PREVIEWS: OCR VERSIONS
                    plateOtsuPreview.setFromPixels(plateOtsu.data, plateOtsu.cols, plateOtsu.rows, OF_IMAGE_GRAYSCALE);
                    plateAdaptivePreview.setFromPixels(plateAdaptive.data, plateAdaptive.cols, plateAdaptive.rows, OF_IMAGE_GRAYSCALE);
                    plateInvertedPreview.setFromPixels(plateInverted.data, plateInverted.cols, plateInverted.rows, OF_IMAGE_GRAYSCALE);

                    // OCR
                    if (ocrReady) {
                        resultOtsu = plateOCR.recognize(plateOtsu);
                        resultAdaptive = plateOCR.recognize(plateAdaptive);
                        resultInverted = plateOCR.recognize(plateInverted);
                    } else {
                        resultOtsu = "OCR not available";
                        resultAdaptive = "OCR not available";
                        resultInverted = "OCR not available";
                    }

                    // TERMINAL OUTPUT
                    std::cout << "Otsu: " << resultOtsu << std::endl;
                    std::cout << "Adaptive: " << resultAdaptive << std::endl;
                    std::cout << "Inverted: " << resultInverted << std::endl;
                }

                imageLoaded = true;
            }
        }
    }
}

//--------------------------------------------------------------
void ofApp::keyReleased(int key) {

}

//--------------------------------------------------------------
void ofApp::mouseMoved(int x, int y) {

}

//--------------------------------------------------------------
void ofApp::mouseDragged(int x, int y, int button) {

}

//--------------------------------------------------------------
void ofApp::mousePressed(int x, int y, int button) {

}

//--------------------------------------------------------------
void ofApp::mouseReleased(int x, int y, int button) {

}

//--------------------------------------------------------------
void ofApp::mouseScrolled(int x, int y, float scrollX, float scrollY) {

}

//--------------------------------------------------------------
void ofApp::mouseEntered(int x, int y) {

}

//--------------------------------------------------------------
void ofApp::mouseExited(int x, int y) {

}

//--------------------------------------------------------------
void ofApp::windowResized(int w, int h) {

}

//--------------------------------------------------------------
void ofApp::gotMessage(ofMessage msg) {

}

//--------------------------------------------------------------
void ofApp::dragEvent(ofDragInfo dragInfo) {

}