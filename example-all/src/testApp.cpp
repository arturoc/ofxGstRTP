#include "testApp.h"
#include "Utils.h"

//#define USE_16BIT_DEPTH

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
void testApp::setup(){
	ofSetLogLevel(ofxGstRTPClient::LOG_NAME,OF_LOG_VERBOSE);

	ofXml settings;
	settings.load("settings.xml");
	string server = settings.getValue("server");
	string user = settings.getValue("user");
	string pwd = settings.getValue("pwd");

	rtp.setup(0);
	rtp.getXMPP().setCapabilities("telekinect");
	rtp.connectXMPP(server,user,pwd);
	rtp.addSendVideoChannel(640,480,30,300);
	rtp.addSendDepthChannel(640,480,30,300);
	rtp.addSendOscChannel();
	rtp.addSendAudioChannel();

	shaderRemoveZero.setupShaderFromSource(GL_VERTEX_SHADER,vertexShader);
	shaderRemoveZero.linkProgram();



	kinect.init();
	kinect.setRegistration(true);
	kinect.open();

	kinect.setDepthClipping(500,1000);

	gui.setup("","settings.xml",ofGetWidth()-250,10);

	textureVideoRemote.allocate(640,480,GL_RGB8);
	textureVideoLocal.allocate(640,480,GL_RGB8);

	if(depth16){
		textureDepthRemote.allocate(640,480,GL_RGB8);
		textureDepthLocal.allocate(640,480,GL_LUMINANCE16);
	}else{
		textureDepthRemote.allocate(640,480,GL_LUMINANCE8);
		textureDepthLocal.allocate(640,480,GL_LUMINANCE8);
	}

	drawState = LocalRemote;

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

	//ofSetBackgroundAuto(false);

	if(depth16){
		Utils::CreateColorGradientLUT(pow(2.f,14.f));
	}

	camera.setVFlip(true);

	gray.allocate(640,480);

	calling = -1;

	ofAddListener(rtp.getXMPP().newMessage,this,&testApp::onNewMessage);
}


void testApp::onNewMessage(ofxXMPPMessage & msg){
	messages.push_back(msg);
	if(messages.size()>8) messages.pop_front();
}

void testApp::exit(){
}


//--------------------------------------------------------------
void testApp::update(){
	{
		kinect.update();

		if(kinect.isFrameNewVideo()){
			fpsRGB.newFrame();
			textureVideoLocal.loadData(kinect.getPixelsRef());

			{
				//kinectUpdater.signalNewKinectFrame();
				rtp.getServer().newFrame(kinect.getPixelsRef());
			}

		}

		if(kinect.isFrameNewDepth()){
			fpsDepth.newFrame();

			if(depth16){
				textureDepthLocal.loadData(kinect.getRawDepthPixelsRef());
			}else{
				textureDepthLocal.loadData(kinect.getDepthPixelsRef());
			}

			{
				//kinectUpdater.signalNewKinectFrame();
				if(depth16){
					rtp.getServer().newFrameDepth(kinect.getRawDepthPixelsRef());
				}else{
					rtp.getServer().newFrameDepth(kinect.getDepthPixelsRef());
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
				rtp.getServer().newOscMsg(msg);
			}

			if(drawState==LocalPointCloud){
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

	// update the client ànd load the pixels into a texture if there's a new frame
	{
		rtp.getClient().update();
		if(rtp.getClient().isFrameNewVideo()){
			fpsClientVideo.newFrame();
			textureVideoRemote.loadData(rtp.getClient().getPixelsVideo());
		}
		if(rtp.getClient().isFrameNewDepth()){
			fpsClientDepth.newFrame();
			textureDepthRemote.loadData(rtp.getClient().getPixelsDepth());

			if(depth16){
				if(drawState==RemotePointCloud){
					int i=0;
					for(int y=0;y<480;y++){
						for(int x=0;x<640;x++){
							pointCloud.getVertices()[i].set(kinect.getWorldCoordinateAt(x,y,rtp.getClient().getPixelsDepth16()[i]));
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

}

//--------------------------------------------------------------
void testApp::draw(){
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
		if(rtp.getClient().isFrameNewDepth()){
			ofBackground(255);
			ofSetColor(255);
			ofEnableDepthTest();
			shaderRemoveZero.begin();
			shaderRemoveZero.setUniform1f("removeFar",1);
			camera.begin();
			textureVideoRemote.bind();
			pointCloud.draw();
			textureVideoRemote.unbind();
			camera.end();
			shaderRemoveZero.end();
			ofDisableDepthTest();
		}
		break;
	case LocalPointCloud:
		if(kinect.isFrameNewDepth()){
			ofBackground(255);
			ofSetColor(255);
			ofEnableDepthTest();
			shaderRemoveZero.begin();
			shaderRemoveZero.setUniform1f("removeFar",0);
			camera.begin();
			textureVideoLocal.bind();
			pointCloud.draw();
			textureVideoLocal.unbind();
			camera.end();
			shaderRemoveZero.end();
			ofDisableDepthTest();
		}
		break;
	}

	//gui.draw();

	ofSetColor(255);
	ofRect(ofGetWidth()-300,0,300,ofGetHeight());
	const vector<ofxXMPPUser> & friends = rtp.getXMPP().getFriends();
	size_t i=0;

	for(;i<friends.size();i++){
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
}

//--------------------------------------------------------------
void testApp::keyPressed(int key){
	if(key==OF_KEY_RIGHT){
		drawState =(DrawState)(drawState+1);
		drawState = (DrawState)(drawState%NumStates);
	}else if(key==OF_KEY_LEFT){
		drawState = (DrawState)(drawState-1);
		drawState = (DrawState)(drawState%NumStates);
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
		ofVec2f mouse(x,y);
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
