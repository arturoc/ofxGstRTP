/*
 * ofxOscPacketPool.h
 *
 *  Created on: Aug 22, 2013
 *      Author: arturo
 */

#ifndef OFXOSCPACKETPOOL_H_
#define OFXOSCPACKETPOOL_H_

#include <list>
#include "OscOutboundPacketStream.h"
#include "ofTypes.h"
#include "snappy.h"

class ofxOscPacketPool;

/// osc packet that can be returned to a pool to avoid
/// allocations when sending new osc messages. It also
/// can compress the data of each message
class PooledOscPacket{
public:
	PooledOscPacket(char *buffer, unsigned long capacity, ofxOscPacketPool * pool)
	:packet(buffer,capacity)
	,pool(pool)
	,compressed(new char[capacity])
	,compressedDirty(true)
	,compressedBytes(0){}

	void clear(){
		packet.Clear();
		compressedDirty = true;
	}

	char * compressedData(){
		if(compressedDirty){
			snappy::RawCompress(packet.Data(),packet.Size(),compressed,&compressedBytes);
			compressedDirty = false;
		}
		return compressed;
	}

	size_t compressedSize(){
		if(compressedDirty){
			compressedData();
		}
		return compressedBytes;
	}

	osc::OutboundPacketStream packet;
	ofxOscPacketPool * pool;
private:
	char * compressed;
	bool compressedDirty;
	size_t compressedBytes;
};

/// OSC messages pool, used internally by the addon to avoid
/// doing allocations every frame
class ofxOscPacketPool {
public:
	ofxOscPacketPool();
	virtual ~ofxOscPacketPool();
	PooledOscPacket * newBuffer();
	static void relaseBuffer(PooledOscPacket * buffer);

private:
	void returnBufferToPool(PooledOscPacket * buffer);
	list<PooledOscPacket *> pool;
	ofMutex mutex;
};

#endif /* OFXOSCPACKETPOOL_H_ */
