#include "testApp.h"

//--------------------------------------------------------------
void testApp::setup(){
	ofXml settings;
	settings.load("settings.xml");
	string server = settings.getValue("server");
	string user = settings.getValue("user");
	string pwd = settings.getValue("pwd");

	ofSetLogLevel(ofxGstRTPClient::LOG_NAME,OF_LOG_VERBOSE);
	ofSetLogLevel(ofxGstRTPServer::LOG_NAME,OF_LOG_VERBOSE);

	ap = webrtc::AudioProcessing::Create(0);
	ap->Initialize();
	ap->set_num_channels(2,2);
	ap->set_num_reverse_channels(2);
	ap->set_sample_rate_hz(32000);
	//ap->echo_cancellation()->Enable(true);
	//ap->noise_suppression()->Enable(true);
	//ap->high_pass_filter()->Enable(true);
	//ap->voice_detection()->Enable(true);

	rtp.getServer().setWebRTCAudioProcessing(ap);
	rtp.getClient().setWebRTCAudioProcessing(ap);
	rtp.getServer().setRTPClient(rtp.getClient());
	rtp.setup(0);
	rtp.getXMPP().setCapabilities("telekinect");
	rtp.connectXMPP(server,user,pwd);
	rtp.addSendAudioChannel();
	calling = -1;

	ofBackground(255);

	bitrate.setup(rtp.getServer().audioBitrate);
	bitrate.setPosition(10,10);

	ofAddListener(rtp.callReceived,this,&testApp::onCallReceived);
	ofAddListener(rtp.callFinished,this,&testApp::onCallFinished);
	ofAddListener(rtp.callAccepted,this,&testApp::onCallAccepted);
}

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


}

//--------------------------------------------------------------
void testApp::draw(){
	bitrate.draw();

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

}

//--------------------------------------------------------------
void testApp::mousePressed(int x, int y, int button){
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
