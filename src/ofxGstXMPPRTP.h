/*
 * ofxGstRTP.h
 *
 *  Created on: Aug 27, 2013
 *      Author: arturo
 */

#ifndef OFXGSTRTP_H_
#define OFXGSTRTP_H_

#include "ofxGstRTPConstants.h"

#if ENABLE_NAT_TRANSVERSAL

#include "ofxNice.h"
#include "ofxXMPP.h"
#include "ofxGstRTPServer.h"
#include "ofxGstRTPClient.h"
#include "ofTypes.h"
#include "ofConstants.h"

#if ENABLE_ECHO_CANCEL
#include "ofxEchoCancel.h"
#endif

/// This class should be used when session initiation through XMPP
/// and NAT transversal is required. It controls the work flow of the
/// ofxXMPP addon using the jingle implementation in it to start a call
/// accept it...
/// It also contains a server and a client, and creates all the necessary
/// NICE streams
/// To use it call setup, then connectXMPP with the required credentials.
/// before starting a call add the desired channels with the addSend*Channel
/// methods, (only in the side starting the call) and call the call method
/// The other peer will receive a notification on the callReceived event
/// and should call acceptCall or refuseCall
/// Once the call is established both sides, can start to send an receive frames
/// through the server and client respectively.
class ofxGstXMPPRTP {
public:
	ofxGstXMPPRTP();
	virtual ~ofxGstXMPPRTP();

	/// starts a client and server with the specified maximum latency
	/// for the client
	void setup(int clientLatency=200, bool enableEchoCancel=true);

	/// sets an external xmpp client, in case non is set the addon will
	/// create one when calling setup, so this needs to be called before
	/// setup
	void setXMPP(shared_ptr<ofxXMPP> & xmpp);

	/// setup a STUN server to be used for NAT transversal
	void setStunServer(const string & ip, uint port=3478);

	/// add a TURN server to use for NAT transversal
	void addRelay(const string & ip, uint port, const string & user, const string & pwd, NiceRelayType type);

	/// connect to the XMPP server, to be able to establish a session or
	/// send chat messages
	void connectXMPP(const string & host, const string & username, const string & pwd);

	/// get a vector of contacts currently connected and their states
	vector<ofxXMPPUser> getFriends();

	/// set our show status, away, dnd...
	void setShow(ofxXMPPShowState showState);

	/// set our status, as a string message
	void setStatus(const string & status);

	/// send a chat message to a user in our contacts
	void sendXMPPMessage(const string & to, const string & message);

	/// before starting a call the initiating side shoulc add the desired
	/// channels, this method adds a video channel
	void addSendVideoChannel(int w, int h, int fps);

	/// before starting a call the initiating side shoulc add the desired
	/// channels, this method adds a depth channel
	void addSendDepthChannel(int w, int h, int fps, bool depth16=false);

	/// before starting a call the initiating side shoulc add the desired
	/// channels, this method adds an audio channel
	void addSendAudioChannel();

	/// before starting a call the initiating side shoulc add the desired
	/// channels, this method adds an osc channel
	void addSendOscChannel();

	/// after connecting to xmpp and adding the desired channels, use this method
	/// to start a call with a user
	void call(const ofxXMPPUser & user);

	/// accessor for the server, needed to send new frames once the connection
	/// is started
	ofxGstRTPServer & getServer();

	/// accessor for the server, needed to receive new frames once the connection
	/// is started
	ofxGstRTPClient & getClient();

	/// accessor for the XMPP utility class, can be used for more advanced uses
	/// but usually not needed
	ofxXMPP & getXMPP();

	ofParameterGroup parameters;

	/// this event will be notified when a contact is trying to start a call with
	/// us, we should notify the user that there's an incomming call, usually with
	/// a sound or some kind of visual feedback
	ofEvent<string> callReceived;

	/// this event will be notified when the current call has been finished by the
	/// other side, it can also mean that a call was refused by the other side
	ofEvent<ofxXMPPTerminateReason> callFinished;

	/// this event will be notified when after starting a call, the other side accepts
	/// it
	ofEvent<string> callAccepted;


	/// after receiving a call, call this method to accept it
	void acceptCall();

	/// after receiving a call, call this method to refuse it
	void refuseCall();

	void endCall();

private:
	void onNiceLocalCandidatesGathered(const void * sender, vector<ofxICECandidate> & candidates);
	void onJingleInitiationReceived(ofxXMPPJingleInitiation & jingle);
	void onJingleInitiationAccepted(ofxXMPPJingleInitiation & jingle);
	void onJingleTerminateReceived(ofxXMPPTerminateReason & reason);
	void onClientDisconnected();

	void close();

	shared_ptr<ofxXMPP> xmpp;
	shared_ptr<ofxNiceAgent> nice;
	shared_ptr<ofxGstRTPClient> client;
	shared_ptr<ofxGstRTPServer> server;
	ofxXMPPJingleInitiation remoteJingle;
	ofxXMPPJingleInitiation localJingle;

	ofxXMPPUser callingTo;
	bool isControlling;

	shared_ptr<ofxNiceStream> videoStream;
	shared_ptr<ofxNiceStream> depthStream;
	shared_ptr<ofxNiceStream> audioStream;
	shared_ptr<ofxNiceStream> oscStream;

	bool videoGathered, depthGathered, audioGathered, oscGathered;
	bool depth16;
	bool initialized;
	string stunServer;
	uint stunPort;

#if ENABLE_ECHO_CANCEL
	ofxEchoCancel echoCancel;
#endif

};

#endif

#endif /* OFXGSTRTP_H_ */
