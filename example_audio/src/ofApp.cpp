#include "ofApp.h"
#include "ofxNice.h"

//--------------------------------------------------------------
void ofApp::setup(){
	ofXml settings;
	settings.load("settings.xml");
	string server = settings.getValue("server");
	string user = settings.getValue("user");
	string pwd = settings.getValue("pwd");

	ofSetLogLevel(ofxGstRTPClient::LOG_NAME,OF_LOG_VERBOSE);
	ofSetLogLevel(ofxGstRTPServer::LOG_NAME,OF_LOG_VERBOSE);

	ofxNiceEnableDebug();
	rtp.setup("132.177.123.6",500);
	//rtp.getClient().drop = false;
	rtp.getXMPP().setCapabilities("telekinect");
	rtp.connectXMPP(server,user,pwd);
	rtp.addSendAudioChannel();
	calling = -1;

	ofBackground(255);

	gui.setup();
	gui.add(rtp.parameters);

	ofAddListener(rtp.callReceived,this,&ofApp::onCallReceived);
	ofAddListener(rtp.callFinished,this,&ofApp::onCallFinished);
	ofAddListener(rtp.callAccepted,this,&ofApp::onCallAccepted);
}

void ofApp::onCallReceived(string & from){
	rtp.acceptCall();
}

void ofApp::onCallAccepted(string & from){

}

void ofApp::onCallFinished(ofxXMPPTerminateReason & reason){
}


void ofApp::exit(){
}


//--------------------------------------------------------------
void ofApp::update(){


}

//--------------------------------------------------------------
void ofApp::draw(){
	gui.draw();

	ofTranslate(0,40);
	const vector<ofxXMPPUser> & friends = rtp.getXMPP().getFriends();

	for(size_t i=0;i<friends.size();i++){
		ofSetColor(0);
		if(calling==i){
			if(rtp.getXMPP().getJingleState()==ofxXMPP::SessionAccepted){
				ofSetColor(127);
			}else{
				ofSetColor(ofMap(sin(ofGetElapsedTimef()*2),-1,1,50,127));
			}
			ofRect(ofGetWidth()-300,calling*20+5,300,20);
			ofSetColor(255);
		}
		ofDrawBitmapString(friends[i].userName,ofGetWidth()-250,20+20*i);
		if(friends[i].show==ofxXMPPShowAvailable){
			ofSetColor(ofColor::green);
		}else{
			ofSetColor(ofColor::orange);
		}
		ofCircle(ofGetWidth()-270,20+20*i-5,3);
		//cout << friends[i].userName << endl;
		for(size_t j=0;j<friends[i].capabilities.size();j++){
			if(friends[i].capabilities[j]=="telekinect"){
				ofNoFill();
				ofCircle(ofGetWidth()-270,20+20*i-5,5);
				ofFill();
				break;
			}
		}
	}

}

//--------------------------------------------------------------
void ofApp::keyPressed(int key){
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
	if(calling==-1){
		ofVec2f mouse(x,y-40);
		ofRectangle friendsRect(ofGetWidth()-300,0,300,rtp.getXMPP().getFriends().size()*20);
		if(friendsRect.inside(mouse)){
			calling = mouse.y/20;
			rtp.call(rtp.getXMPP().getFriends()[calling]);
		}
	}
}

//--------------------------------------------------------------
void ofApp::mouseReleased(int x, int y, int button){

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
