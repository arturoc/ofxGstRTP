/*
 * ofApp.h
 *
 *  Created on: Jul 19, 2013
 *      Author: arturo castro
 */

#pragma once

#include "ofMain.h"
#include "ofxGstXMPPRTP.h"
#include "ofxFPS.h"
#include "ofxGui.h"
#include "ofxKinect.h"
#include "ofxOpenCv.h"

class ofApp : public ofBaseApp{

	public:
		void setup();
		void update();
		void draw();
		void exit();

		bool onFriendSelected(const void *sender);
		void onConnectionStateChanged(ofxXMPPConnectionState & connectionState);
		void onNewMessage(ofxXMPPMessage & msg);
		void onCallReceived(string & from);
		void onCallAccepted(string & from);
		void onCallFinished(ofxXMPPTerminateReason & reason);

		void keyPressed(int key);
		void keyReleased(int key);
		void mouseMoved(int x, int y );
		void mouseDragged(int x, int y, int button);
		void mousePressed(int x, int y, int button);
		void mouseReleased(int x, int y, int button);
		void windowResized(int w, int h);
		void dragEvent(ofDragInfo dragInfo);
		void gotMessage(ofMessage msg);

		ofxGstXMPPRTP rtp;

		ofxKinect kinect;
		ofxFPS fpsRGB;
		ofxFPS fpsDepth;
		ofxFPS fpsClientVideo;
		ofxFPS fpsClientDepth;
		ofTexture textureVideoRemote, textureDepthRemote;
		ofTexture textureVideoLocal, textureDepthLocal;

		ofxPanel gui;

		ofVboMesh pointCloud;
		ofEasyCam camera;

		enum DrawState{
			LocalRemote,
			LocalPointCloud,
			RemotePointCloud,
			NumStates
		}drawState;

		ofShader shader;

		ofxCvGrayscaleImage gray;
		ofxCvContourFinder contourFinder;
		ofPolyline remoteContour;

		enum CallingState{
			MissingSettings,
			Calling,
			ReceivingCall,
			InCall,
			Disconnected
		}callingState;
		string callFrom;
		int calling;

		string currentMessage;
		deque<ofxXMPPMessage> messages;
		enum GuiState{
			Network,
			Friends,
			NumGuiStates
		}guiState;

		ofSoundPlayer ring;
		unsigned long long lastRing;
		ofRectangle ok,cancel;

		ofShortPixels resizedDepth;

		float zeroPPixelSize,zeroPDistance;
};
