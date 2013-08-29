/*
 * ofxGstVideoDoubleBuffer.h
 *
 *  Created on: Aug 1, 2013
 *      Author: arturo
 */

#ifndef OFXGSTVIDEODOUBLEBUFFER_H_
#define OFXGSTVIDEODOUBLEBUFFER_H_

#include "ofPixels.h"
#include "ofTypes.h"

#include <gst/gstsample.h>

template<typename PixelType>
class ofxGstVideoDoubleBuffer {
public:
	ofxGstVideoDoubleBuffer();
	virtual ~ofxGstVideoDoubleBuffer();

	void setup(int width, int height, int numChannels);

	bool isFrameNew();
	void newSample(GstSample * sample);
	void update();
	ofPixels_<PixelType> & getPixels();

private:
	GstSample * frontSample, * backSample;
	ofPixels_<PixelType> pixels;
	ofMutex mutex;
	GstMapInfo mapinfo;
	bool bIsNewFrame;
};




template<typename PixelType>
ofxGstVideoDoubleBuffer<PixelType>::ofxGstVideoDoubleBuffer()
:frontSample(NULL)
,backSample(NULL)
,mapinfo()
,bIsNewFrame(false)
{
	GstMapInfo mapinfo = {0,};
	this->mapinfo=mapinfo;

}

template<typename PixelType>
ofxGstVideoDoubleBuffer<PixelType>::~ofxGstVideoDoubleBuffer() {
	// TODO Auto-generated destructor stub
}


template<typename PixelType>
void ofxGstVideoDoubleBuffer<PixelType>::setup(int width, int height, int numChannels){
	pixels.allocate(width,height,numChannels);
}


template<typename PixelType>
bool ofxGstVideoDoubleBuffer<PixelType>::isFrameNew(){
	return bIsNewFrame;
}

template<typename PixelType>
void ofxGstVideoDoubleBuffer<PixelType>::newSample(GstSample * sample){
	mutex.lock();
	if(backSample) gst_sample_unref(backSample);
	backSample = sample;
	mutex.unlock();

}

template<typename PixelType>
void ofxGstVideoDoubleBuffer<PixelType>::update(){
	mutex.lock();
	if(frontSample!=backSample){
		if(frontSample) gst_sample_unref(frontSample);
		frontSample = backSample;
		gst_sample_ref(frontSample);
		bIsNewFrame = true;
		mutex.unlock();

		GstBuffer * _buffer = gst_sample_get_buffer(frontSample);
		gst_buffer_map (_buffer, &mapinfo, GST_MAP_READ);
		pixels.setFromExternalPixels((PixelType*)mapinfo.data,pixels.getWidth(),pixels.getHeight(),pixels.getNumChannels());
		gst_buffer_unmap(_buffer,&mapinfo);
	}else{
		bIsNewFrame = false;
		mutex.unlock();
	}
}

template<typename PixelType>
ofPixels_<PixelType> & ofxGstVideoDoubleBuffer<PixelType>::getPixels(){
	return pixels;
}

#endif /* OFXGSTVIDEODOUBLEBUFFER_H_ */
