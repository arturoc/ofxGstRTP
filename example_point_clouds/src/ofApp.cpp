#include "ofApp.h"
#include "ofxGstRTPUtils.h"
#include "ofxNice.h"
#include "ofConstants.h"


//--------------------------------------------------------------
void ofApp::setup(){
	ofSetLogLevel(ofxGstRTPClient::LOG_NAME,OF_LOG_VERBOSE);

	ofXml settings;
	settings.load("settings.xml");
	string server = settings.getValue("server");
	string user = settings.getValue("user");
	string pwd = settings.getValue("pwd");

	ofxNiceEnableDebug();
	rtp.setup("132.177.123.6",200);
	rtp.getXMPP().setCapabilities("telekinect");
	rtp.connectXMPP(server,user,pwd);
	rtp.addSendVideoChannel(640,480,30);
	rtp.addSendDepthChannel(160,120,30,true);
	rtp.addSendAudioChannel();


	kinect.init();
	kinect.setRegistration(true);
	bool kinectOpen = kinect.open();

	//kinect.setDepthClipping(500,1000);

	shader.load("shader.vert","shader.frag","shader.geom");
	shader.begin();
	if(kinectOpen){
		zeroPPixelSize = kinect.getZeroPlanePixelSize();
		zeroPDistance = kinect.getZeroPlaneDistance();
	}else{
		zeroPPixelSize = 0.1042;
		zeroPDistance = 120;
	}
	shader.setUniform1f("ref_pix_size",zeroPPixelSize);
	shader.setUniform1f("ref_distance",zeroPDistance);
	shader.setUniform1f("SCALE_UP",4);
	shader.setUniform1f("max_distance",100);
	shader.end();

	gui.setup("","settings.xml",ofGetWidth()-250,10);
	gui.add(rtp.parameters);

	textureVideoRemote.allocate(640,480,GL_RGB8);
	textureVideoLocal.allocate(640,480,GL_RGB8);

	textureDepthRemote.allocate(160,120,GL_R16);
	textureDepthLocal.allocate(160,120,GL_R16);

	textureDepthRemote.setRGToRGBASwizzles(true);
	textureDepthLocal.setRGToRGBASwizzles(true);
	resizedDepth.allocate(160,120,1);

	pointCloudLocal.getVertices().resize(160*120);
	pointCloudLocal.getTexCoords().resize(160*120);
	pointCloudLocal.setUsage(GL_STREAM_DRAW);
	int i=0;
	for(int y=0;y<120;y++){
		for(int x=0;x<160;x++){
			pointCloudLocal.getTexCoords()[i].set(x*4,y*4);
			pointCloudLocal.getVertices()[i++].set(x,120-y);
		}
	}
	for (int y=0;y<120;y++){
		for (int x=0;x<160;x++){
			pointCloudLocal.addIndex(y*160+x+1);
			pointCloudLocal.addIndex(y*160+x);
			pointCloudLocal.addIndex((y+1)*160+x);

			pointCloudLocal.addIndex((y+1)*160+x);
			pointCloudLocal.addIndex((y+1)*160+x+1);
			pointCloudLocal.addIndex(y*160+x+1);
		}
	}

	pointCloudRemote.getVertices().resize(160*120);
	pointCloudRemote.getTexCoords().resize(160*120);
	pointCloudRemote.setUsage(GL_STREAM_DRAW);
	i=0;
	for(int y=0;y<120;y++){
		for(int x=0;x<160;x++){
			pointCloudRemote.getTexCoords()[i].set(x*4,y*4);
			pointCloudRemote.getVertices()[i++].set(x,y);
		}
	}
	for (int y=0;y<120;y++){
		for (int x=0;x<160;x++){
			pointCloudRemote.addIndex(y*160+x+1);
			pointCloudRemote.addIndex(y*160+x);
			pointCloudRemote.addIndex((y+1)*160+x);

			pointCloudRemote.addIndex((y+1)*160+x);
			pointCloudRemote.addIndex((y+1)*160+x+1);
			pointCloudRemote.addIndex(y*160+x+1);
		}
	}

	//camera.setVFlip(true);
	camera.setTarget(ofVec3f(0.0,0.0,0.0));
	camera.setPosition(ofVec3f(1000.0,0.0,0.0));

	calling = -1;

	ofAddListener(rtp.getXMPP().newMessage,this,&ofApp::onNewMessage);

	guiState = Friends;

	ofAddListener(rtp.callReceived,this,&ofApp::onCallReceived);
	ofAddListener(rtp.callFinished,this,&ofApp::onCallFinished);
	ofAddListener(rtp.callAccepted,this,&ofApp::onCallAccepted);

	callingState = Disconnected;

	ring.loadSound("ring.wav",false);
	lastRing = 0;
}

void ofApp::onCallReceived(string & from){
	callFrom = ofSplitString(from,"/")[0];
	callingState = ReceivingCall;
}

void ofApp::onCallAccepted(string & from){
	if(callingState == Calling){
		callingState = InCall;
	}
}

void ofApp::onCallFinished(ofxXMPPTerminateReason & reason){
	if(callingState==Calling){
		ofSystemAlertDialog("Call declined");
	}
	callingState = Disconnected;

	/*rtp.setup(200);
	rtp.addSendVideoChannel(640,480,30,300);
	rtp.addSendDepthChannel(640,480,30,300);
	rtp.addSendOscChannel();
	rtp.addSendAudioChannel();*/
}

void ofApp::onNewMessage(ofxXMPPMessage & msg){
	messages.push_back(msg);
	if(messages.size()>8) messages.pop_front();
}

void ofApp::exit(){
}


//--------------------------------------------------------------
void ofApp::update(){
	kinect.update();
	GstClockTime now = rtp.getServer().getTimeStamp();

	if(kinect.isFrameNewVideo()){
		fpsRGB.newFrame();
		textureVideoLocal.loadData(kinect.getPixelsRef());
		rtp.getServer().newFrame(kinect.getPixelsRef(),now);
	}

	if(kinect.isFrameNewDepth()){
		fpsDepth.newFrame();
		kinect.getRawDepthPixelsRef().resizeTo(resizedDepth,OF_INTERPOLATE_NEAREST_NEIGHBOR);
		textureDepthLocal.loadData(kinect.getRawDepthPixelsRef(),GL_RED);

		rtp.getServer().newFrameDepth(resizedDepth,now,zeroPPixelSize,zeroPDistance);

		int i=0;
		for(int y=0;y<120;y++){
			for(int x=0;x<160;x++){
				pointCloudLocal.getVertices()[i].z = -resizedDepth[i];
				i++;
			}
		}
	}

	// update the client ànd load the pixels into a texture if there's a new frame
	rtp.getClient().update();
	if(rtp.getClient().isFrameNewVideo()){
		fpsClientVideo.newFrame();
		textureVideoRemote.loadData(rtp.getClient().getPixelsVideo());
	}
	if(rtp.getClient().isFrameNewDepth()){

		if(rtp.getClient().getPixelsDepth16().getWidth()==120 && rtp.getClient().getPixelsDepth16().getWidth()==160){
			fpsClientDepth.newFrame();
			textureDepthRemote.loadData(rtp.getClient().getPixelsDepth16());
			int i=0;
			for(int y=0;y<120;y++){
				for(int x=0;x<160;x++){
					pointCloudRemote.getVertices()[i].z = rtp.getClient().getPixelsDepth16()[i];
					i++;
				}
			}
		}
	}

	/*if(calling!=-1){
		if(rtp.getXMPP().getJingleState()==ofxXMPP::Disconnected){
			calling = -1;
		}
	}*/

	if(callingState==ReceivingCall || callingState==Calling){
		unsigned long long now = ofGetElapsedTimeMillis();
		if(now - lastRing>2500){
			lastRing = now;
			ring.play();
		}
	}

}

//--------------------------------------------------------------
void ofApp::draw(){
	ofBackground(0);
	ofSetColor(255);
	ofEnableDepthTest();
	shader.begin();

	shader.setUniform1f("ref_pix_size",rtp.getClient().getZeroPlanePixelSize());
	shader.setUniform1f("ref_distance",rtp.getClient().getZeroPlaneDistance());
	camera.begin();
	textureVideoRemote.bind();
	pointCloudRemote.drawWireframe();
	textureVideoRemote.unbind();

	shader.setUniform1f("ref_pix_size",zeroPPixelSize);
	shader.setUniform1f("ref_distance",zeroPDistance);
	textureVideoLocal.bind();
	pointCloudLocal.drawWireframe();
	textureVideoLocal.unbind();
	camera.end();
	shader.end();
	ofDisableDepthTest();

	if(callingState==ReceivingCall){
		ofSetColor(30,30,30,170);
		ofRect(0,0,ofGetWidth(),ofGetHeight());
		ofSetColor(255,255,255);
		ofRectangle popup(ofGetWidth()*.5-200,ofGetHeight()*.5-100,400,200);
		ofRect(popup);
		ofSetColor(0);
		ofDrawBitmapString("Receiving call from " + callFrom,popup.x+30,popup.y+30);

		ok.set(popup.x+popup.getWidth()*.25-50,popup.getCenter().y+20,100,30);
		cancel.set(popup.x+popup.getWidth()*.75-50,popup.getCenter().y+20,100,30);
		ofRect(ok);
		ofRect(cancel);

		ofSetColor(255);
		ofDrawBitmapString("Ok",ok.x+30,ok.y+20);
		ofDrawBitmapString("Decline",cancel.x+30,cancel.y+20);

	}

	ofSetColor(255);
	ofRect(ofGetWidth()-300,0,300,ofGetHeight());
	if(guiState==Friends){
		const vector<ofxXMPPUser> & friends = rtp.getXMPP().getFriendsWithCapability("telekinect");
		size_t i=0;

		for(;i<friends.size();i++){
			ofSetColor(0);
			if(calling==i){
				if(callingState == Calling){
					ofSetColor(ofMap(sin(ofGetElapsedTimef()*2),-1,1,50,127));
				}else if(callingState==InCall){
					ofSetColor(127);
				}
				ofRect(ofGetWidth()-300,calling*20+5,300,20);
				ofSetColor(255);
			}else{
				if(callingState==InCall && friends[i].userName==callFrom){
					ofSetColor(127);
					ofRect(ofGetWidth()-300,i*20+5,300,20);
					ofSetColor(255);
				}
			}
			ofDrawBitmapString(friends[i].userName,ofGetWidth()-250,20+20*i);
			if(friends[i].show==ofxXMPPShowAvailable){
				ofSetColor(ofColor::green);
			}else{
				ofSetColor(ofColor::orange);
			}
			ofCircle(ofGetWidth()-270,20+20*i-5,3);
		}

		i++;
		ofSetColor(0);
		size_t j=0;

		for (;j<messages.size();j++){
			ofDrawBitmapString(ofSplitString(messages[j].from,"/")[0] +":\n" + messages[j].body,ofGetWidth()-280,20+i*20+j*30);
		}

		if(currentMessage!=""){
			ofDrawBitmapString("me: " + currentMessage, ofGetWidth()-280, 20 + i++ *20 + j*30);
		}

		if(calling>=0 && calling<(int)friends.size()){
			if(friends[calling].chatState==ofxXMPPChatStateComposing){
				ofDrawBitmapString(friends[calling].userName + ": ...", ofGetWidth()-280, 20 + i*20 + j*30);
			}
		}
	}else{
		gui.draw();
	}
}

//--------------------------------------------------------------
void ofApp::keyPressed(int key){
	if(key==OF_KEY_UP){
		guiState = (GuiState)(guiState+1);
		guiState = (GuiState)(guiState%NumGuiStates);
	}else if(key!=OF_KEY_RETURN){
		if(calling!=-1){
			currentMessage += (char)key;
		}
	}else{
		if(calling!=-1){
			rtp.getXMPP().sendMessage(rtp.getFriends()[calling].userName,currentMessage);
			ofxXMPPMessage msg;
			msg.from = "me";
			msg.body = currentMessage;
			messages.push_back(msg);
			if(messages.size()>8) messages.pop_front();

			currentMessage = "";
		}
	}
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
	ofVec2f mouse(x,y);
	if(callingState==Disconnected && guiState==Friends){
		ofRectangle friendsRect(ofGetWidth()-300,0,300,rtp.getXMPP().getFriends().size()*20);
		if(friendsRect.inside(mouse)){
			calling = mouse.y/20;
			rtp.call(rtp.getXMPP().getFriendsWithCapability("telekinect")[calling]);
			callingState = Calling;
		}
	}else if(callingState == ReceivingCall){
		if(ok.inside(mouse)){
			rtp.acceptCall();
			callingState = InCall;
		}else if(cancel.inside(mouse)){
			rtp.refuseCall();
			callingState = Disconnected;
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
