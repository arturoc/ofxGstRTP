/*
 * testApp.h
 *
 *  Created on: Jul 19, 2013
 *      Author: arturo castro
 */

#pragma once

#include "ofMain.h"
#include "ofxGstRTPClient.h"
#include "ofxGstRTPServer.h"
#include "ofxGui.h"

#define DO_ECHO_CANCEL 1

#if DO_ECHO_CANCEL
#include "ofxEchoCancel.h"
#endif

class testApp : public ofBaseApp{

	public:
		void setup();
		void update();
		void draw();
		void exit();

		void keyPressed(int key);
		void keyReleased(int key);
		void mouseMoved(int x, int y );
		void mouseDragged(int x, int y, int button);
		void mousePressed(int x, int y, int button);
		void mouseReleased(int x, int y, int button);
		void windowResized(int w, int h);
		void dragEvent(ofDragInfo dragInfo);
		void gotMessage(ofMessage msg);

		ofxGstRTPClient client;
		ofxGstRTPServer server;

		ofVideoGrabber grabber;
		ofTexture remoteVideo;

		ofxPanel gui;
#if DO_ECHO_CANCEL
		ofxEchoCancel echoCancel;
#endif
};
