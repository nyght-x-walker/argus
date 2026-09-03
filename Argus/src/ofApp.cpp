//========================================================================
// Argus – License Plate Recognition & Flagging System
// OF + minimal UI scaffold
//========================================================================

#include "ofApp.h"

//========================================================================
void ofApp::setup(){
	ofSetWindowTitle("Argus – License Plate Recognition");
	ofSetWindowShape(1280, 720);
	ofSetVerticalSync(true);
	ofSetFrameRate(60);
	
	bShowStatusWindow = true;
}

//========================================================================
void ofApp::update(){
	// Pipeline placeholder – will be filled in later phases
}

//========================================================================
void ofApp::draw(){
	ofBackground(30, 30, 40);
	
	// "Scaffold" UI using OF primitives (ofxImGui integration will be added in later phases)
	// Dockspace covering whole window
	ofPushStyle();
	ofNoFill();
	ofSetColor(255);
	ofDrawRectangle(0, 0, ofGetWidth(), ofGetHeight());
	ofPopStyle();
	
	// "Argus Status" box
	ofSetColor(255);
	ofDrawRectangle(20, 20, 280, 80);
	ofSetColor(0, 0, 0);
	ofDrawRectangle(22, 22, 276, 76);
	ofSetColor(255, 255, 255);
	ofDrawBitmapString("Argus v0.1.0", 24, 36);
	ofDrawBitmapString("Status: Scaffold OK", 24, 52);
	ofDrawBitmapString("FPS: " + ofToString(ofGetFrameRate(), 2), 24, 68);
}

//========================================================================
void ofApp::exit(){
	// Called when app exits
	ofLog() << "ofApp::exit() called";
}

//========================================================================
void ofApp::keyPressed(int key){
	(void)key;
}

//========================================================================
void ofApp::keyReleased(int key){
	(void)key;
}

//========================================================================
void ofApp::mouseMoved(int x, int y){
	(void)x;
	(void)y;
}

//========================================================================
void ofApp::mouseDragged(int x, int y, int button){
	(void)x;
	(void)y;
	(void)button;
}

//========================================================================
void ofApp::mousePressed(int x, int y, int button){
	(void)x;
	(void)y;
	(void)button;
}

//========================================================================
void ofApp::mouseReleased(int x, int y, int button){
	(void)x;
	(void)y;
	(void)button;
}

//========================================================================
void ofApp::mouseEntered(int x, int y){
	(void)x;
	(void)y;
}

//========================================================================
void ofApp::mouseExited(int x, int y){
	(void)x;
	(void)y;
}

//========================================================================
void ofApp::windowResized(int w, int h){
	(void)w;
	(void)h;
}

//========================================================================
void ofApp::dragEvent(ofDragInfo dragInfo){
	(void)dragInfo;
}

//========================================================================
void ofApp::gotMessage(ofMessage msg){
	(void)msg;
}
//========================================================================
