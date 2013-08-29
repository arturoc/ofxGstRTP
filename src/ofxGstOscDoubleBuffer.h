/*
 * ofxGstOscDoubleBuffer.h
 *
 *  Created on: Aug 22, 2013
 *      Author: arturo
 */

#ifndef OFXGSTOSCDOUBLEBUFFER_H_
#define OFXGSTOSCDOUBLEBUFFER_H_


#include <gst/gstsample.h>
#include "OscReceivedElements.h"
#include "ofTypes.h"

class ofxGstOscDoubleBuffer {
public:
	ofxGstOscDoubleBuffer();
	virtual ~ofxGstOscDoubleBuffer();

	bool isFrameNew();
	void newSample(GstSample * sample);
	void update();
	osc::ReceivedPacket * getOscReceivedPacket();

private:
	GstSample * frontSample, * backSample;
	osc::ReceivedPacket * packet;
	ofMutex mutex;
	GstMapInfo mapinfo;
	bool bIsNewFrame;
	char * uncompressed;
};




#endif /* OFXGSTOSCDOUBLEBUFFER_H_ */
