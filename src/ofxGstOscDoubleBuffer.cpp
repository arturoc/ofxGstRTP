/*
 * ofxGstOscDoubleBuffer.cpp
 *
 *  Created on: Aug 22, 2013
 *      Author: arturo
 */

#include "ofxGstOscDoubleBuffer.h"
#include "snappy.h"

ofxGstOscDoubleBuffer::ofxGstOscDoubleBuffer()
:frontSample(NULL)
,backSample(NULL)
,packet(NULL)
,mapinfo()
,bIsNewFrame(false)
,uncompressed(new char[65535])
{
	GstMapInfo mapinfo = {0,};
	this->mapinfo=mapinfo;

}

ofxGstOscDoubleBuffer::~ofxGstOscDoubleBuffer() {
	// TODO Auto-generated destructor stub
}


bool ofxGstOscDoubleBuffer::isFrameNew(){
	return bIsNewFrame;
}

void ofxGstOscDoubleBuffer::newSample(GstSample * sample){
	mutex.lock();
	if(backSample) gst_sample_unref(backSample);
	backSample = sample;
	mutex.unlock();

}

void ofxGstOscDoubleBuffer::update(){
	mutex.lock();
	if(frontSample!=backSample){
		if(frontSample) gst_sample_unref(frontSample);
		frontSample = backSample;
		gst_sample_ref(frontSample);
		bIsNewFrame = true;
		mutex.unlock();

		GstBuffer * _buffer = gst_sample_get_buffer(frontSample);
		gst_buffer_map (_buffer, &mapinfo, GST_MAP_READ);

		size_t uncompressedSize;
		snappy::RawUncompress((const char*)mapinfo.data,mapinfo.size,uncompressed);
		snappy::GetUncompressedLength((const char*)mapinfo.data,mapinfo.size,&uncompressedSize);

		if(packet) delete packet;
		packet = new osc::ReceivedPacket(uncompressed, uncompressedSize);

		gst_buffer_unmap(_buffer,&mapinfo);
	}else{
		bIsNewFrame = false;
		mutex.unlock();
	}
}

osc::ReceivedPacket * ofxGstOscDoubleBuffer::getOscReceivedPacket(){
	return packet;
}
