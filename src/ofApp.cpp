#include "ofApp.h"

//--------------------------------------------------------------
void ofApp::setup(){

}

//--------------------------------------------------------------
void ofApp::update(){

}

//--------------------------------------------------------------
void ofApp::draw() {

    ofBackground(40);
    ofSetColor(255);

    if (imageLoaded) {

        float maxSize = 400.0f;

        float scale = std::min(
            maxSize / selectedImage.getWidth(),
            maxSize / selectedImage.getHeight()
        );

        float drawWidth = selectedImage.getWidth() * scale;
        float drawHeight = selectedImage.getHeight() * scale;

        // Information text
        std::string info =
            "Press O to open another image\n"
            "File: " + selectedFilename + "\n"
            "Resolution: " +
            ofToString(selectedImage.getWidth()) + " x " +
            ofToString(selectedImage.getHeight());

        ofDrawBitmapString(info, 20, 25);

        // Image underneath the text
        selectedImage.draw(20, 80, drawWidth, drawHeight);
        grayPreview.draw(maxSize+20, 80, drawWidth, drawHeight);

    } else {

        ofDrawBitmapString("Press O to open an image", 20, 25);
    }
}

//--------------------------------------------------------------
void ofApp::exit(){

}

//--------------------------------------------------------------
void ofApp::keyPressed(int key){
    if (key == 'o' || key == 'O') {
        ofFileDialogResult result = ofSystemLoadDialog("Upload Image");

        if (result.bSuccess) {
            selectedFilename = ofFilePath::getFileName(result.getPath());
            selectedImage.load(result.getPath());

            ofPixels &pixels = selectedImage.getPixels();
            cv::Mat colorImage(selectedImage.getHeight(),selectedImage.getWidth(),CV_8UC3,pixels.getData());
            cv::cvtColor(colorImage, grayImage, cv::COLOR_RGB2GRAY);
            grayPreview.setFromPixels(grayImage.data,grayImage.cols,grayImage.rows,OF_IMAGE_GRAYSCALE);

            if (selectedImage.isAllocated()) {
                imageLoaded = true;
            }
        }
    }

}

//--------------------------------------------------------------
void ofApp::keyReleased(int key){

}

//--------------------------------------------------------------
void ofApp::mouseMoved(int x, int y ){

}

//--------------------------------------------------------------
void ofApp::mouseDragged(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mousePressed(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mouseReleased(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mouseScrolled(int x, int y, float scrollX, float scrollY){

}

//--------------------------------------------------------------
void ofApp::mouseEntered(int x, int y){

}

//--------------------------------------------------------------
void ofApp::mouseExited(int x, int y){

}

//--------------------------------------------------------------
void ofApp::windowResized(int w, int h){

}

//--------------------------------------------------------------
void ofApp::gotMessage(ofMessage msg){

}

//--------------------------------------------------------------
void ofApp::dragEvent(ofDragInfo dragInfo){ 

}
