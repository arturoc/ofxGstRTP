#include "ofApp.h"
#include "ofxGstRTPUtils.h"
#define USE_16BIT_DEPTH

#ifdef USE_16BIT_DEPTH
	bool depth16=true;
#else
	bool depth16=false;
#endif

#define STRINGIFY(x) #x

string vertexShader = STRINGIFY(
	uniform float removeFar;
	void main(){
		gl_TexCoord[0] = gl_MultiTexCoord0;
		if(gl_Vertex.z<0.1){
			gl_Position = vec4(0.,0.,0.,0.);
			gl_FrontColor =  vec4(0.,0.,0.,0.);
		}else if(removeFar>.5 && gl_Vertex.z>490.){
			gl_Position = vec4(0.,0.,0.,0.);
			gl_FrontColor =  vec4(0.,0.,0.,0.);
		}else{
			vec4 pos = gl_Vertex;
			pos.z *= -1.;
			gl_Position = gl_ProjectionMatrix * gl_ModelViewMatrix * pos;
			gl_FrontColor =  gl_Color;
		}
	}
);


//--------------------------------------------------------------
void ofApp::setup(){
	ofSetLogLevel(ofxGstRTPClient::LOG_NAME,OF_LOG_VERBOSE);

	ofXml settings;
	settings.load("settings.xml");
	string server = settings.getValue("server");
	string user = settings.getValue("user");
	string pwd = settings.getValue("pwd");

	rtp.setup(200);
	rtp.getXMPP().setCapabilities("telekinect");
	rtp.connectXMPP(server,user,pwd);
	rtp.addSendVideoChannel(640,480,30);
	if(depth16){
		rtp.addSendDepthChannel(160,120,30,depth16);
	}else{
		rtp.addSendDepthChannel(640,480,30,depth16);
	}
	rtp.addSendOscChannel();
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
	shader.setUniform1f("max_distance",500);
	shader.end();

	gui.setup("","settings.xml",ofGetWidth()-250,10);
	gui.add(rtp.parameters);

	textureVideoRemote.allocate(640,480,GL_RGB8);
	textureVideoLocal.allocate(640,480,GL_RGB8);

	if(depth16){
		textureDepthRemote.allocate(160,120,GL_R16);
		textureDepthLocal.allocate(160,120,GL_R16);
	}else{
		textureDepthRemote.allocate(640,480,GL_R8);
		textureDepthLocal.allocate(640,480,GL_R8);
	}
	textureDepthRemote.setRGToRGBASwizzles(true);
	textureDepthLocal.setRGToRGBASwizzles(true);
	resizedDepth.allocate(160,120,1);

	drawState = LocalRemote;

	if(depth16){
		pointCloud.getVertices().resize(160*120);
		pointCloud.getTexCoords().resize(160*120);
		pointCloud.setUsage(GL_STREAM_DRAW);
		int i=0;
		for(int y=0;y<120;y++){
			for(int x=0;x<160;x++){
				pointCloud.getTexCoords()[i].set(x*4,y*4);
				pointCloud.getVertices()[i++].set(x,y);
			}
		}
		for (int y=0;y<120;y++){
			for (int x=0;x<160;x++){
				pointCloud.addIndex(y*160+x+1);
				pointCloud.addIndex(y*160+x);
				pointCloud.addIndex((y+1)*160+x);

				pointCloud.addIndex((y+1)*160+x);
				pointCloud.addIndex((y+1)*160+x+1);
				pointCloud.addIndex(y*160+x+1);
			}
		}
	}else{
		pointCloud.setMode(OF_PRIMITIVE_POINTS);
		pointCloud.getVertices().resize(640*480);
		pointCloud.getTexCoords().resize(640*480);
		pointCloud.setUsage(GL_STREAM_DRAW);
		int i=0;
		for(int y=0;y<480;y++){
			for(int x=0;x<640;x++){
				pointCloud.getTexCoords()[i].set(x,y);
				pointCloud.getVertices()[i++].set(x,y);
			}
		}
	}

	//ofSetBackgroundAuto(false);

	if(depth16){
		//ofxGstRTPUtils::CreateColorGradientLUT(pow(2.f,14.f));
	}

	//camera.setVFlip(true);

	camera.setTarget(ofVec3f(0.0,0.0,-1000.0));

	gray.allocate(640,480);

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
	{
		kinect.update();
		GstClockTime now = rtp.getServer().getTimeStamp();

		if(kinect.isFrameNewVideo()){
			fpsRGB.newFrame();
			textureVideoLocal.loadData(kinect.getPixelsRef());

			{
				//kinectUpdater.signalNewKinectFrame();
				rtp.getServer().newFrame(kinect.getPixelsRef(),now);
			}

		}

		if(kinect.isFrameNewDepth()){
			fpsDepth.newFrame();
			if(depth16){
				kinect.getRawDepthPixelsRef().resizeTo(resizedDepth,OF_INTERPOLATE_NEAREST_NEIGHBOR);
			}

			if(depth16){
				textureDepthLocal.loadData(kinect.getRawDepthPixelsRef(),GL_RED);
			}else{
				textureDepthLocal.loadData(kinect.getDepthPixelsRef());
			}

			{
				//kinectUpdater.signalNewKinectFrame();
				if(depth16){
					rtp.getServer().newFrameDepth(resizedDepth,now,zeroPPixelSize,zeroPDistance);
				}else{
					rtp.getServer().newFrameDepth(kinect.getDepthPixelsRef(),now);
				}
			}

			{
				gray.setFromPixels(kinect.getDepthPixelsRef());
				gray.adaptiveThreshold(5,0,false,true);
				contourFinder.findContours(gray,10,640*480/3,1,false,true);
				ofxOscMessage msg;
				msg.setAddress("//telekinect/contour");

				if(contourFinder.blobs.size()){
					ofPolyline blob;
					blob.addVertices(contourFinder.blobs[0].pts);
					blob.simplify(2);
					for(int i=0;i<(int)blob.size();i++){
						msg.addFloatArg(blob[i].x);
						msg.addFloatArg(blob[i].y);
					}
				}
				rtp.getServer().newOscMsg(msg,now);
			}

			if(drawState==LocalPointCloud){
				if(depth16){
					int i=0;
					for(int y=0;y<120;y++){
						for(int x=0;x<160;x++){
							pointCloud.getVertices()[i].z = resizedDepth[i];
							i++;
						}
					}
				}else{
					int i=0;
					for(int y=0;y<480;y++){
						for(int x=0;x<640;x++){
							pointCloud.getVertices()[i].set(kinect.getWorldCoordinateAt(x,y,kinect.getRawDepthPixelsRef()[i]));
							i++;
						}
					}
				}
			}
		}
	}

	// update the client ànd load the pixels into a texture if there's a new frame
	{
		rtp.getClient().update();
		if(rtp.getClient().isFrameNewVideo()){
			fpsClientVideo.newFrame();
			textureVideoRemote.loadData(rtp.getClient().getPixelsVideo());
		}
		if(rtp.getClient().isFrameNewDepth()){
			fpsClientDepth.newFrame();
			if(depth16){
				textureDepthRemote.loadData(rtp.getClient().getPixelsDepth16());
			}else{
				textureDepthRemote.loadData(rtp.getClient().getPixelsDepth());
			}

			if(depth16){
				if(drawState==RemotePointCloud){
					int i=0;
					for(int y=0;y<120;y++){
						for(int x=0;x<160;x++){
							pointCloud.getVertices()[i].z = rtp.getClient().getPixelsDepth16()[i];
							i++;
						}
					}
				}
			}else{
				if(drawState==RemotePointCloud){
					int i=0;
					for(int y=0;y<480;y++){
						for(int x=0;x<640;x++){
							pointCloud.getVertices()[i].set(x-320,y-240,1000-ofMap(rtp.getClient().getPixelsDepth()[i],0,255,500,1000));
							i++;
						}
					}
				}

			}
		}

		if(rtp.getClient().isFrameNewOsc()){
			ofxOscMessage msg = rtp.getClient().getOscMessage();
			remoteContour.clear();
			for(int i=0;i<msg.getNumArgs();i+=2){
				remoteContour.addVertex(msg.getArgAsFloat(i),msg.getArgAsFloat(i+1));
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
		if(now - lastRing>2000){
			lastRing = now;
			ring.play();
		}
	}

}

//--------------------------------------------------------------
void ofApp::draw(){
	switch(drawState){
	case LocalRemote:
		ofSetColor(255);
		{
			textureVideoRemote.draw(0,0);
			ofSetColor(255,255,0);
			remoteContour.draw();
			ofSetColor(255);
			ofDrawBitmapString(ofToString(ofGetFrameRate()),20,20);
			ofDrawBitmapString(ofToString(fpsClientVideo.getFPS(),2),20,40);
		}

		{
			textureDepthRemote.draw(640,0);
			ofDrawBitmapString(ofToString(fpsClientDepth.getFPS(),2),660,20);
		}

		ofSetColor(255);
		{
			textureVideoLocal.draw(400,300,240,180);
			ofDrawBitmapString(ofToString(fpsRGB.getFPS(),2),410,315);
		}
		{
			textureDepthLocal.draw(1040,300,240,180);
			ofDrawBitmapString(ofToString(fpsDepth.getFPS(),2),1050,315);
		}
		break;
	case RemotePointCloud:
		//if(rtp.getClient().isFrameNewDepth()){
			ofBackground(0);
			ofSetColor(255);
			ofEnableDepthTest();
			shader.begin();
			shader.setUniform1f("ref_pix_size",rtp.getClient().getZeroPlanePixelSize());
			shader.setUniform1f("ref_distance",rtp.getClient().getZeroPlaneDistance());
			camera.begin();
			textureVideoRemote.bind();
			pointCloud.drawWireframe();
			textureVideoRemote.unbind();
			camera.end();
			shader.end();
			ofDisableDepthTest();
		//}
		break;
	case LocalPointCloud:
		//if(kinect.isFrameNewDepth()){
			ofBackground(0);
			ofSetColor(255);
			ofEnableDepthTest();
			shader.begin();
			shader.setUniform1f("ref_pix_size",zeroPPixelSize);
			shader.setUniform1f("ref_distance",zeroPDistance);
			camera.begin();
			textureVideoLocal.bind();
			pointCloud.drawWireframe();
			textureVideoLocal.unbind();
			camera.end();
			shader.end();
			ofDisableDepthTest();
		//}
		break;
	}

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
		const vector<ofxXMPPUser> & friends = rtp.getXMPP().getFriends();
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
	if(key==OF_KEY_RIGHT){
		drawState =(DrawState)(drawState+1);
		drawState = (DrawState)(drawState%NumStates);
	}else if(key==OF_KEY_LEFT){
		drawState = (DrawState)(drawState-1);
		drawState = (DrawState)(drawState%NumStates);
	}else if(key==OF_KEY_UP){
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
			rtp.call(rtp.getXMPP().getFriends()[calling]);
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
