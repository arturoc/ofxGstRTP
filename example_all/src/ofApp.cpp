/**
 * This example demos how to create an application that sends audio,
 * video, depth and meta data (using osc) from computers behind a NAT
 * router.
 *
 * It uses XMPP to start a connection with another peer and then the
 * ICE protocol through libnice and ofxNice to do NAT traversal,
 * avoiding to have to setup IP addresses and opening specific ports on
 * the routers.
 *
 * Once the connection is established it uses the RTP protocol to send the
 * different streams in sync between the two peers.
 *
 * All this happens transparently in ofxGstRTP so you don't need to setup
 * every individual protocol.
 */


#include "ofApp.h"
#include "ofxGstRTPUtils.h"
#include "ofxNice.h"
#include "ofConstants.h"


// uncomment to generate env vars for bundled
// application in osx
//#define BUNDLED

// in this example we send the point cloud instead of the 8bit image
// this flag is used in several places in the example to setup, send
// and receive 16bits depth
#define USE_16BIT_DEPTH

#ifdef USE_16BIT_DEPTH
	bool depth16=true;
#else
	bool depth16=false;
#endif


//--------------------------------------------------------------
void ofApp::setup(){
	// uncommment to set logging of the different modules to verbose
	// ofSetLogLevel(ofxGstRTPClient::LOG_NAME,OF_LOG_VERBOSE);
	// ofSetLogLevel(ofxGstRTPServer::LOG_NAME,OF_LOG_VERBOSE);
	// ofSetLogLevel(ofxNice::LOG_NAME,OF_LOG_VERBOSE);
	// ofSetLogLevel(ofxXMPP::LOG_NAME,OF_LOG_VERBOSE);

	// enable additional debugging traces for libnice
	//ofxNiceEnableDebug();

#ifdef BUNDLED
	// if we want to create a bundle application, tell gstreamer
	// to look for plugins on the bundle instead of the default path
    ofxGStreamerSetBundleEnvironment();
#endif

    // try to load settings from an xml file, in a real application
    // you shouldn't be storing passwords in plain text in an xml!
	ofXml settings;
	if(settings.load("settings.xml")){
		string server = settings.getValue("server");
		string user = settings.getValue("user");
		string pwd = settings.getValue("pwd");

		// setup the rtp module with 200ms max latency on the receiving end
		rtp.setup(200);

		// the STUN server is used for NAT transversal and should be the same in
		// both computers, you can find a list of public STUN servers here:
		// http://www.voip-info.org/wiki/view/STUN
		rtp.setStunServer("132.177.123.6");

		// additionally we can use a TURN server that will relay all the streams
		// if we are behind a very restrictive network, you can install your
		// own TURN server using this software:
		// https://code.google.com/p/rfc5766-turn-server/wiki/Readme
		// if you enable a TURN server it'll also act as STUN server so set the
		// STUN server to the same IP
		//rtp.addRelay("IP_ADDRESS",3479,"","",NICE_RELAY_TYPE_TURN_UDP);

		// add a capabilities string so other clients can recognize that we
		// are running this app and not some other XMPP software like gtalk
		rtp.getXMPP().setCapabilities("telekinect");

		// start the connection to the XMPP server
		rtp.connectXMPP(server,user,pwd);

		// add the video, depth, osc and audio channels that we are going to use
		// right now this have to be the same on both ends
		rtp.addSendVideoChannel(640,480,30);
		if(depth16){
			rtp.addSendDepthChannel(160,120,30,depth16);
		}else{
			rtp.addSendDepthChannel(640,480,30,depth16);
		}
		rtp.addSendOscChannel();
		rtp.addSendAudioChannel();
	}else{
		ofLogError() << "couldn't load settings.xml file";
	}

	// Start the kinect and set the registration to true so depth
	// and rgb match
	kinect.init();
	kinect.setRegistration(true);
	bool kinectOpen = kinect.open();
	//kinect.setDepthClipping(500,1000);

	// load a shader to create a mesh out of the point clouds
	// and calculate the world coordinates on the gpu to make the
	// application faster
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

	// create a gui and add the rtp parameters to it
	gui.setup("","settings.xml",ofGetWidth()-250,10);
	gui.add(rtp.parameters);

	// allocate some textures to render the local and remote feeds
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


	// setup a mesh to draw the point cloud, if we are using 8bits
	// depth the point cloud will be really broken but we set it up anyway
	// to make the logic of the program simpler
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

	// setup an easyCam to be able to navigate in
	// the point cloud modes
	camera.setTarget(ofVec3f(0.0,0.0,-1000.0));

	// opencv image to do some basic contour finding to send a blob
	// over osc as a test
	gray.allocate(640,480);


	// add a listener to receive a callback whenever there's a chat
	// message from the XMPP client
	ofAddListener(rtp.getXMPP().newMessage,this,&ofApp::onNewMessage);


	// add listeners to receive the call live cycle callbacks
	ofAddListener(rtp.callReceived,this,&ofApp::onCallReceived);
	ofAddListener(rtp.callFinished,this,&ofApp::onCallFinished);
	ofAddListener(rtp.callAccepted,this,&ofApp::onCallAccepted);


	// some state variables and ringing sound to play when there's a new call
	ring.loadSound("ring.wav",false);
	callingState = Disconnected;
	drawState = LocalRemote;
	calling = -1;
	guiState = Friends;
	lastRing = 0;
}

// will be called whenever there's a new call from another peer
void ofApp::onCallReceived(string & from){
	callFrom = ofSplitString(from,"/")[0];
	callingState = ReceivingCall;
}

// will be called when we start a call and the other peer accepts it
void ofApp::onCallAccepted(string & from){
	if(callingState == Calling){
		callingState = InCall;
	}
}

// will be called whenever the call ends and
// receives the reason as parameter
void ofApp::onCallFinished(ofxXMPPTerminateReason & reason){
	if(callingState==Calling){
		// if we started a call most likely the other end declined it
		// or the call failed
		ofSystemAlertDialog("Call declined");
	}
	cout << "received end call" << endl;
	// reset the rtp element to be able to start a new call
	rtp.setup(200);
	rtp.setStunServer("132.177.123.6");
	rtp.addSendVideoChannel(640,480,30);
	if(depth16){
		rtp.addSendDepthChannel(160,120,30,depth16);
	}else{
		rtp.addSendDepthChannel(640,480,30,depth16);
	}
	rtp.addSendOscChannel();
	rtp.addSendAudioChannel();

	// reset the state
	callingState = Disconnected;
	calling = -1;
}

// will be called whenever there's a new chat message
void ofApp::onNewMessage(ofxXMPPMessage & msg){
	messages.push_back(msg);
	if(messages.size()>8) messages.pop_front();
}

void ofApp::exit(){
}


//--------------------------------------------------------------
void ofApp::update(){
	// update the local side and send any new data through the RTP server
	{
		// update the kinect
		kinect.update();

		// get the current time to use the same time for all the streams
		// to have a more accurate syncing
		GstClockTime now = rtp.getServer().getTimeStamp();

		// if there's a new RGB frame from the kinect, load it on a texture
		// to draw it locally and send it to the other peer through the
		// rtp element
		if(kinect.isFrameNewVideo()){
			fpsRGB.newFrame();
			textureVideoLocal.loadData(kinect.getPixelsRef());
			rtp.getServer().newFrame(kinect.getPixelsRef(),now);

		}

		// if there's a new depth frame from the kinect, load it on a texture
		// to draw it locally and send it to the other peer through the
		// rtp element, here we also do a basic contour finding over the depth
		// to test the metadata osc stream and load the depth as a point cloud
		// on an ofVboMesh
		if(kinect.isFrameNewDepth()){
			fpsDepth.newFrame();

			if(depth16){
				textureDepthLocal.loadData(kinect.getRawDepthPixelsRef(),GL_RED);
			}else{
				textureDepthLocal.loadData(kinect.getDepthPixelsRef());
			}

			if(depth16){
				kinect.getRawDepthPixelsRef().resizeTo(resizedDepth,OF_INTERPOLATE_NEAREST_NEIGHBOR);
				rtp.getServer().newFrameDepth(resizedDepth,now,zeroPPixelSize,zeroPDistance);
			}else{
				rtp.getServer().newFrameDepth(kinect.getDepthPixelsRef(),now);
			}

			// contour analisys and send the contour over osc rtp
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

			// load the local depth on the mesh
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
	}// end updating the local side



	// update the remove side and load any received data from the RTP client
	{
		// update the client
		rtp.getClient().update();

		// check if we've received a new RGB frame and load it on a texture
		if(rtp.getClient().isFrameNewVideo()){
			fpsClientVideo.newFrame();
			textureVideoRemote.loadData(rtp.getClient().getPixelsVideo());
		}

		// check if we've received a new depth frame and load it on a texture
		// and as a point cloud on the ofVboMesh
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

		// check if we've received any new contour
		// and load it on an oFPolyline to show it
		if(rtp.getClient().isFrameNewOsc()){
			ofxOscMessage msg = rtp.getClient().getOscMessage();
			remoteContour.clear();
			for(int i=0;i<msg.getNumArgs();i+=2){
				remoteContour.addVertex(msg.getArgAsFloat(i),msg.getArgAsFloat(i+1));
			}
		}
	}

	// play the ring tone if we are starting a call or receiving one
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
	switch(drawState){
	case LocalRemote:
		ofSetColor(255);
		// draw the remote RGB stream
		{
			textureVideoRemote.draw(0,0);
			ofSetColor(255,255,0);
			remoteContour.draw();
			ofSetColor(255);
			ofDrawBitmapString(ofToString(ofGetFrameRate()),20,20);
			ofDrawBitmapString(ofToString(fpsClientVideo.getFPS(),2),20,40);
		}

		// draw the remote depth stream
		{
			textureDepthRemote.draw(640,0,640,480);
			ofDrawBitmapString(ofToString(fpsClientDepth.getFPS(),2),660,20);
		}

		ofSetColor(255);
		// draw the local RGB stream
		{
			textureVideoLocal.draw(400,300,240,180);
			ofDrawBitmapString(ofToString(fpsRGB.getFPS(),2),410,315);
		}
		// draw the local depth stream
		{
			textureDepthLocal.draw(1040,300,240,180);
			ofDrawBitmapString(ofToString(fpsDepth.getFPS(),2),1050,315);
		}
		break;
	case RemotePointCloud:
		// draw the remote point cloud as a mesh
		// using the meshing shader
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
		break;
	case LocalPointCloud:
		// draw the local point cloud as a mesh
		// using the meshing shader
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
		break;
	}

	// if we have an incoming call draw a simple gui to allow
	// the user to accept it or decline it
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

	// draw the gui to show the connected friends and
	// allow to establish a new connection
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
	if(key==OF_KEY_RIGHT){
		drawState =(DrawState)(drawState+1);
		drawState = (DrawState)(drawState%NumStates);
	}else if(key==OF_KEY_LEFT){
		drawState = (DrawState)(drawState-1);
		drawState = (DrawState)(drawState%NumStates);
	}else if(key==OF_KEY_UP){
		guiState = (GuiState)(guiState+1);
		guiState = (GuiState)(guiState%NumGuiStates);
	}else if(key==' '){
		// test to check that ending a call works properly
		cout << "ending call" << endl;
		rtp.endCall();
		rtp.setup(200);
		rtp.setStunServer("132.177.123.6");
		rtp.addSendVideoChannel(640,480,30);
		if(depth16){
			rtp.addSendDepthChannel(160,120,30,depth16);
		}else{
			rtp.addSendDepthChannel(640,480,30,depth16);
		}
		rtp.addSendOscChannel();
		rtp.addSendAudioChannel();
		calling = -1;
		callingState = Disconnected;
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
	// interaction for friends list and accept/decline a call guis
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
