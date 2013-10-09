#include "testApp.h"

//--------------------------------------------------------------
void testApp::setup(){
	ofXml settings;
	settings.load("settings.xml");
	string server = settings.getValue("server");
	string user = settings.getValue("user");
	string pwd = settings.getValue("pwd");

	rtp.setup(0);
	rtp.getXMPP().setCapabilities("telekinect");
	rtp.connectXMPP(server,user,pwd);
	rtp.addSendOscChannel();
	calling = -1;

	ofBackground(255);
	ofSetFrameRate(30);

	ofAddListener(rtp.callReceived,this,&testApp::onCallReceived);
	ofAddListener(rtp.callFinished,this,&testApp::onCallFinished);
	ofAddListener(rtp.callAccepted,this,&testApp::onCallAccepted);
}

// this accepts any incomming call for more complex setups you probably want to implement
// some logic to let the user decide if she wants to accept or decline the call
void testApp::onCallReceived(string & from){
	rtp.acceptCall();
}

void testApp::onCallAccepted(string & from){

}

void testApp::onCallFinished(ofxXMPPTerminateReason & reason){
}


void testApp::exit(){
}


//--------------------------------------------------------------
void testApp::update(){
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
void testApp::draw(){
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
void testApp::keyPressed(int key){
}

//--------------------------------------------------------------
void testApp::keyReleased(int key){

}

//--------------------------------------------------------------
void testApp::mouseMoved(int x, int y ){

}

//--------------------------------------------------------------
void testApp::mouseDragged(int x, int y, int button){
	ofVec2f mouse(x,y);
	ofRectangle friendsRect(ofGetWidth()-300,0,300,rtp.getXMPP().getFriends().size()*20);
	if(!friendsRect.inside(mouse)){
		localPoly.addVertex(mouse);
	}
}

//--------------------------------------------------------------
void testApp::mousePressed(int x, int y, int button){
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
void testApp::mouseReleased(int x, int y, int button){

}

//--------------------------------------------------------------
void testApp::windowResized(int w, int h){

}

//--------------------------------------------------------------
void testApp::gotMessage(ofMessage msg){

}

//--------------------------------------------------------------
void testApp::dragEvent(ofDragInfo dragInfo){ 

}
