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
#include "ofxDepthCompressedFrame.h"

#include <gst/gstsample.h>

template<typename PixelType>
class ofxGstVideoDoubleBuffer {
public:
	ofxGstVideoDoubleBuffer();
	virtual ~ofxGstVideoDoubleBuffer();

	void setup(int width, int height, int numChannels);
	void setupFor16();
	bool isAllocated();

	bool isFrameNew();
	void newSample(GstSample * sample);
	void update();
	ofPixels_<PixelType> & getPixels();

	float getZeroPlanePixelSize();
	float getZeroPlaneDistance();

private:
	GstSample * frontSample, * backSample;
	ofPixels_<PixelType> pixels;
	ofMutex mutex;
	GstMapInfo mapinfo;
	bool bIsNewFrame;
	bool allocated;
	bool depth16;
	ofxDepthCompressedFrame lastKeyFrame;
	ofxDepthCompressedFrame lastFrame;
	float pixelSize;
	float distance;
};




template<typename PixelType>
ofxGstVideoDoubleBuffer<PixelType>::ofxGstVideoDoubleBuffer()
:frontSample(NULL)
,backSample(NULL)
,mapinfo()
,bIsNewFrame(false)
,allocated(false)
,depth16(false)
,pixelSize(1)
,distance(1)
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
	allocated = true;
}

template<typename PixelType>
void ofxGstVideoDoubleBuffer<PixelType>::setupFor16(){
	allocated = true;
	depth16 = true;
}

template<typename PixelType>
bool ofxGstVideoDoubleBuffer<PixelType>::isAllocated(){
	return allocated;
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
		if(!depth16){
			pixels.setFromExternalPixels((PixelType*)mapinfo.data,pixels.getWidth(),pixels.getHeight(),pixels.getNumChannels());
		}else{
			lastFrame.fromCompressedData((char*)mapinfo.data,mapinfo.size);
			if(lastFrame.isKeyFrame()){
				lastKeyFrame = lastFrame;
				if(!pixels.isAllocated()){
					pixels.allocate(lastKeyFrame.getPixels().getWidth(),lastKeyFrame.getPixels().getHeight(),1);
				}
				pixelSize = lastKeyFrame.getPixelSize();
				distance = lastKeyFrame.getDistance();
				memcpy(pixels.getPixels(),((unsigned short*)lastKeyFrame.getPixels().getPixels()), pixels.size()*sizeof(short));
			}else{
				if(pixels.isAllocated()){
					pixelSize = lastFrame.getPixelSize();
					distance = lastFrame.getDistance();
					for(int i=0;i<pixels.size();i++){
						pixels[i] = lastKeyFrame.getPixels()[i] + lastFrame.getPixels()[i];
					}
				}
			}
		}
		gst_buffer_unmap(_buffer,&mapinfo);
	}else{
		bIsNewFrame = false;
		mutex.unlock();
	}
}


template<typename PixelType>
float ofxGstVideoDoubleBuffer<PixelType>::getZeroPlanePixelSize(){
	return pixelSize;
}

template<typename PixelType>
float ofxGstVideoDoubleBuffer<PixelType>::getZeroPlaneDistance(){
	return distance;
}

template<typename PixelType>
ofPixels_<PixelType> & ofxGstVideoDoubleBuffer<PixelType>::getPixels(){
	return pixels;
}

#endif /* OFXGSTVIDEODOUBLEBUFFER_H_ */
