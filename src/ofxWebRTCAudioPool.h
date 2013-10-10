/*
 * ofxWebRTCAudioPool.h
 *
 *  Created on: Oct 8, 2013
 *      Author: arturo
 */

#ifndef OFXWEBRTCAUDIOPOOL_H_
#define OFXWEBRTCAUDIOPOOL_H_

#include "ofxEchoCancel.h"
#include "ofConstants.h"
#include <list>
#include "ofTypes.h"

class ofxWebRTCAudioPool;

class PooledAudioFrame{
public:
	PooledAudioFrame(ofxWebRTCAudioPool * pool)
	:pool(pool){}

	webrtc::AudioFrame audioFrame;
	ofxWebRTCAudioPool * pool;
};

class ofxWebRTCAudioPool {
public:
	ofxWebRTCAudioPool();
	virtual ~ofxWebRTCAudioPool();

	PooledAudioFrame * newFrame();
	static void relaseFrame(PooledAudioFrame * buffer);

private:
	void returnFrameToPool(PooledAudioFrame * buffer);
	list<PooledAudioFrame *> pool;
	ofMutex mutex;
};

#endif /* OFXWEBRTCAUDIOPOOL_H_ */
