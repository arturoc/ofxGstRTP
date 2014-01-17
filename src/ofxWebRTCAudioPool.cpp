/*
 * ofxWebRTCAudioPool.cpp
 *
 *  Created on: Oct 8, 2013
 *      Author: arturo
 */

#include "ofxWebRTCAudioPool.h"
#if ENABLE_ECHO_CANCEL

ofxWebRTCAudioPool::ofxWebRTCAudioPool() {
	// TODO Auto-generated constructor stub

}

ofxWebRTCAudioPool::~ofxWebRTCAudioPool() {
	// TODO Auto-generated destructor stub
}


PooledAudioFrame * ofxWebRTCAudioPool::newFrame(){
	mutex.lock();
	if(pool.empty()){
		mutex.unlock();
		PooledAudioFrame * frame = new PooledAudioFrame(this);
		return frame;
	}else{
		PooledAudioFrame * frame = pool.front();
		pool.erase(pool.begin());
		mutex.unlock();
		return frame;
	}
}

void ofxWebRTCAudioPool::relaseFrame(PooledAudioFrame * frame){
	frame->pool->returnFrameToPool(frame);
}

void ofxWebRTCAudioPool::returnFrameToPool(PooledAudioFrame * frame){
	mutex.lock();
	pool.push_back(frame);
	mutex.unlock();
}

#endif