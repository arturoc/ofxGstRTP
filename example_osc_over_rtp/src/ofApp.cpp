#include "ofApp.h"
#include "ofxNice.h"

//--------------------------------------------------------------
void ofApp::setup(){
	ofXml settings;
	settings.load("settings.xml");
	string server = settings.getValue("server");
	string user = settings.getValue("user");
	string pwd = settings.getValue("pwd");

	ofxNiceEnableDebug();
	rtp.setup(500);
	rtp.setStunServer("132.177.123.6");
	rtp.getXMPP().setCapabilities("telekinect");
	rtp.connectXMPP(server,user,pwd);
	rtp.addSendOscChannel();
	calling = -1;

	ofBackground(255);
	ofSetFrameRate(30);

	ofAddListener(rtp.callReceived,this,&ofApp::onCallReceived);
	ofAddListener(rtp.callFinished,this,&ofApp::onCallFinished);
	ofAddListener(rtp.callAccepted,this,&ofApp::onCallAccepted);
}

// this accepts any incomming call for more complex setups you probably want to implement
// some logic to let the user decide if she wants to accept or decline the call
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
	ofxOscMessage message;
	message.setAddress("//telekinect/poly");
	for(int i=0;i<localPoly.size();i++){
		message.addFloatArg(localPoly[i].x);
		message.addFloatArg(localPoly[i].y);
	}
	rtp.getServer().newOscMsg(message);

	rtp.getClient().update();
	if(rtp.getClient().isFrameNewOsc()){
		const ofxOscMessage & msg = rtp.getClient().getOscMessage();
		if(msg.getAddress()=="//telekinect/poly"){
			remotePoly.clear();
			for(int i=0;i<msg.getNumArgs();i+=2){
				remotePoly.addVertex(msg.getArgAsFloat(i),msg.getArgAsFloat(i+1));
			}
		}
	}

}

//--------------------------------------------------------------
void ofApp::draw(){
	ofSetColor(ofColor::magenta);
	localPoly.draw();
	ofSetColor(ofColor::cyan);
	remotePoly.draw();


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
	ofVec2f mouse(x,y);
	ofRectangle friendsRect(ofGetWidth()-300,0,300,rtp.getXMPP().getFriends().size()*20);
	if(!friendsRect.inside(mouse)){
		localPoly.addVertex(mouse);
	}
}

//--------------------------------------------------------------
void ofApp::mousePressed(int x, int y, int button){
	ofVec2f mouse(x,y);
	ofRectangle friendsRect(ofGetWidth()-300,0,300,rtp.getXMPP().getFriends().size()*20);
	if(friendsRect.inside(mouse) && calling==-1){
		calling = mouse.y/20;
		rtp.call(rtp.getXMPP().getFriends()[calling]);
	}else if(!friendsRect.inside(mouse)){
		localPoly.addVertex(mouse);
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
