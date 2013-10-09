#include "testApp.h"

//--------------------------------------------------------------
void testApp::setup(){
	grabber.initGrabber(640,480);
	remoteVideo.allocate(640,480,GL_RGB);

	// to run this example sending data from different applications or computers
	// set the ports to be different in the client and server, but matching the client
	// and server ports from one app to the other, for example one computer will have:
	//
	// client video: 5000
	// client audio: 6000
	// server video: 7000
	// server audio: 8000
	//
	// the other:
	//
	// client video: 7000
	// client audio: 8000
	// server video: 5000
	// server audio: 6000
	//
	// port numbers have to be even according to the standard and have to be separated by at least
	// 4 numbers since internally rtp uses the next 2 odd port numbers for communicating stats
	// of the network state through rctp, so if we set 5000 for video internally it'll use
	// also 5001 and 5003


#if DO_ECHO_CANCEL
	// if echo cancel is enabled we need to setup the echo cancel module and pass it to
	// the server and client, also the server needs a reference to the client in order to now
	// the audio latency to be able to remove the echo
	echoCancel.setup();
	client.setEchoCancel(echoCancel);
	server.setEchoCancel(echoCancel);
	server.setRTPClient(client);
#endif

	// this sets the remote ip and the latency, in a LAN you can usually use latency 0
	// over internet you'll probably need to make it higher, around 200 is usually a good
	// number but depends on the network conditions
	client.setup("127.0.0.1",0);
	client.addVideoChannel(5000,640,480,30);
	client.addAudioChannel(6000);

	server.setup("127.0.0.1");
	server.addVideoChannel(5000,640,480,30);
	server.addAudioChannel(6000);

	gui.setup("","settings.xml",660,10);
	gui.add(client.parameters);
	gui.add(server.parameters);
#if DO_ECHO_CANCEL
	gui.add(echoCancel.parameters);
#endif

	client.play();
	server.play();

	ofBackground(255);
}


void testApp::exit(){
}


//--------------------------------------------------------------
void testApp::update(){
	grabber.update();
	if(grabber.isFrameNew()){
		server.newFrame(grabber.getPixelsRef());
	}

	client.update();
	if(client.isFrameNewVideo()){
		remoteVideo.loadData(client.getPixelsVideo());
	}
}

//--------------------------------------------------------------
void testApp::draw(){
	ofSetColor(255);
	remoteVideo.draw(0,0);
	grabber.draw(400,300,240,180);
	gui.draw();
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
