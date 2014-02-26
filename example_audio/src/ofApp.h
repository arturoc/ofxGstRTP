/*
 * ofApp.h
 *
 *  Created on: Jul 19, 2013
 *      Author: arturo castro
 */

#pragma once

#include "ofMain.h"
#include "ofxGui.h"
#include "ofxGstXMPPRTP.h"

class ofApp : public ofBaseApp{

	public:
		void setup();
		void update();
		void draw();
		void exit();

		bool onFriendSelected(const void *sender);
		void onConnectionStateChanged(ofxXMPPConnectionState & connectionState);

		void keyPressed(int key);
		void keyReleased(int key);
		void mouseMoved(int x, int y );
		void mouseDragged(int x, int y, int button);
		void mousePressed(int x, int y, int button);
		void mouseReleased(int x, int y, int button);
		void windowResized(int w, int h);
		void dragEvent(ofDragInfo dragInfo);
		void gotMessage(ofMessage msg);

		void onCallReceived(string & from);
		void onCallAccepted(string & from);
		void onCallFinished(ofxXMPPTerminateReason & reason);

		ofxGstXMPPRTP rtp;
		int calling;
		ofxPanel gui;
};
