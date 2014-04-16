/*
 * ofxGstPixelsPool.h
 *
 *  Created on: Jul 29, 2013
 *      Author: arturo castro
 */

#ifndef OFXGSTPIXELSPOOL_H_
#define OFXGSTPIXELSPOOL_H_

#include "ofConstants.h"
#include "ofTypes.h"
#include "ofPixels.h"
#include <list>

template<typename PixelType>
class ofxGstBufferPool;

/// ofPixels stored in a pool so we don't need to allocate memory for every buffer
template<typename PixelType>
class PooledPixels: public ofPixels_<PixelType>{
public:
	ofxGstBufferPool<PixelType> * pool;
};


/// pixels pool: ofPixels copies are allocated if the pool is empty
/// if not we just get one from the pool. When a buffer is not needed anymore
/// gstreamer calls the passed function which instead of deleting them
/// will return them to the pool they belong
/// Used internally by the addon to avoid allocation every frame
template<typename PixelType>
class ofxGstBufferPool{
public:
	ofxGstBufferPool(int width, int height, int channels);

	PooledPixels<PixelType> * newBuffer();
	static void relaseBuffer(PooledPixels<PixelType> * buffer);

private:
	void returnBufferToPool(PooledPixels<PixelType> * pixels);
	list<PooledPixels<PixelType>*> pool;
	int width, height, channels;
	ofMutex mutex;
};




//------------------------------------------------
// implementation

template<typename PixelType>
ofxGstBufferPool<PixelType>::ofxGstBufferPool(int width, int height, int channels)
:width(width), height(height), channels(channels){};

template<typename PixelType>
PooledPixels<PixelType> * ofxGstBufferPool<PixelType>::newBuffer(){
	PooledPixels<PixelType> * pixels;
	mutex.lock();
	if(pool.empty()){
		mutex.unlock();
		pixels = new PooledPixels<PixelType>;
		pixels->allocate(width,height,channels);
		pixels->pool = this;
	}else{
		pixels = pool.back();
		pool.pop_back();
		mutex.unlock();
	}
	return pixels;
}

template<typename PixelType>
void ofxGstBufferPool<PixelType>::returnBufferToPool(PooledPixels<PixelType> * pixels){
	mutex.lock();
	pool.push_back(pixels);
	mutex.unlock();
}


template<typename PixelType>
void ofxGstBufferPool<PixelType>::relaseBuffer(PooledPixels<PixelType> * buffer){
	PooledPixels<PixelType>* pixels = (PooledPixels<PixelType>*) buffer;
	pixels->pool->returnBufferToPool(pixels);
}


#endif /* OFXGSTPIXELSPOOL_H_ */
