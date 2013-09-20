/*
 * ofxGstRTP.h
 *
 *  Created on: Aug 27, 2013
 *      Author: arturo
 */

#ifndef OFXGSTRTP_H_
#define OFXGSTRTP_H_

#include "ofxNice.h"
#include "ofxXMPP.h"
#include "ofxGstRTPServer.h"
#include "ofxGstRTPClient.h"

class ofxGstXMPPRTP {
public:
	ofxGstXMPPRTP();
	virtual ~ofxGstXMPPRTP();

	void setup(int clientLatency=0);

	void connectXMPP(const string & host, const string & username, const string & pwd);
	vector<ofxXMPPUser> getFriends();
	void setShow(ofxXMPPShowState showState);
	void setStatus(const string & status);

	void sendXMPPMessage(const string & to, const string & message);

	void addSendVideoChannel(int w, int h, int fps, int bitrate);
	void addSendDepthChannel(int w, int h, int fps, int bitrate, bool depth16=false);
	void addSendAudioChannel();
	void addSendOscChannel();

	void call(const ofxXMPPUser & user);

	ofxGstRTPServer & getServer();
	ofxGstRTPClient & getClient();
	ofxXMPP & getXMPP();

	ofParameterGroup parameters;

	ofEvent<string> callReceived;
	ofEvent<ofxXMPPTerminateReason> callFinished;
	ofEvent<string> callAccepted;
	void acceptCall();
	void refuseCall();

private:
	void onNiceLocalCandidatesGathered(const void * sender, vector<ofxICECandidate> & candidates);
	void onJingleInitiationReceived(ofxXMPPJingleInitiation & jingle);
	void onJingleInitiationAccepted(ofxXMPPJingleInitiation & jingle);
	void onJingleTerminateReceived(ofxXMPPTerminateReason & reason);

	ofxXMPP xmpp;
	ofxNiceAgent nice;
	ofxGstRTPClient client;
	ofxGstRTPServer server;
	ofxXMPPJingleInitiation remoteJingle;
	ofxXMPPJingleInitiation localJingle;

	ofxXMPPUser callingTo;
	bool isControlling;

	ofxNiceStream * videoStream;
	ofxNiceStream * depthStream;
	ofxNiceStream * audioStream;
	ofxNiceStream * oscStream;

	bool videoGathered, depthGathered, audioGathered, oscGathered;

};

#endif /* OFXGSTRTP_H_ */
