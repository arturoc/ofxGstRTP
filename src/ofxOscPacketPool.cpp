/*
 * ofxOscPacketPool.cpp
 *
 *  Created on: Aug 22, 2013
 *      Author: arturo
 */

#include "ofxOscPacketPool.h"

ofxOscPacketPool::ofxOscPacketPool() {
	// TODO Auto-generated constructor stub

}

ofxOscPacketPool::~ofxOscPacketPool() {
	// TODO Auto-generated destructor stub
}


PooledOscPacket * ofxOscPacketPool::newBuffer(){
	mutex.lock();
	if(pool.empty()){
		mutex.unlock();
		static const int capacity = 65535;
		char * buffer = new char[capacity];
		PooledOscPacket * pkg = new PooledOscPacket(buffer,capacity,this);
		return pkg;
	}else{
		PooledOscPacket * pkg = pool.front();
		pool.erase(pool.begin());
		mutex.unlock();
		return pkg;
	}
}

void ofxOscPacketPool::relaseBuffer(PooledOscPacket * buffer){
	buffer->pool->returnBufferToPool(buffer);
}

void ofxOscPacketPool::returnBufferToPool(PooledOscPacket * buffer){
	buffer->clear();
	mutex.lock();
	pool.push_back(buffer);
	mutex.unlock();
}
