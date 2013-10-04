/*
 * ofxGstRTPServer.h
 *
 *  Created on: Jul 19, 2013
 *      Author: arturo castro
 */

#ifndef OFXGSTRTPSERVER_H_
#define OFXGSTRTPSERVER_H_

#include "ofGstUtils.h"
#include <gst/app/gstappsink.h>
#include "ofParameter.h"
#include "ofParameterGroup.h"

#include "ofxOsc.h"

#include "ofxOscPacketPool.h"
#include <agent.h>
#include "ofxNice.h"
#include "ofxXMPP.h"

#include "audio_processing.h"
#include "module_common_types.h"

class ofxGstRTPClient;

template<typename PixelType>
class ofxGstBufferPool;


class ofxGstRTPServer: public ofGstAppSink {
public:
	ofxGstRTPServer();
	virtual ~ofxGstRTPServer();


	void setRTPClient(ofxGstRTPClient & client);
	void setWebRTCAudioProcessing(webrtc::AudioProcessing * audioProcessing);
	void setup(string destinationAddress);
	void setup();
	void close();

	void addVideoChannel(int port, int w, int h, int fps, int bitrate);
	void addAudioChannel(int port);
	void addDepthChannel(int port, int w, int h, int fps, int bitrate, bool depth16=false);
	void addOscChannel(int port);

	void addVideoChannel(ofxNiceStream * niceStream, int w, int h, int fps, int bitrate);
	void addAudioChannel(ofxNiceStream * niceStream);
	void addDepthChannel(ofxNiceStream * niceStream, int w, int h, int fps, int bitrate, bool depth16=false);
	void addOscChannel(ofxNiceStream * niceStream);

	void play();

	void newFrame(ofPixels & pixels);
	void newFrameDepth(ofPixels & pixels);
	void newFrameDepth(ofShortPixels & pixels);

	void newOscMsg(ofxOscMessage & msg);

	bool on_message(GstMessage * msg);

	ofParameterGroup parameters;
	ofParameter<int> videoBitrate;
	ofParameter<int> audioBitrate;

	static string LOG_NAME;

private:
	void vBitRateChanged(int & bitrate);
	void aBitRateChanged(int & bitrate);
	void appendMessage( ofxOscMessage& message, osc::OutboundPacketStream& p );

	static void on_eos_from_audio(GstAppSink * elt, void * rtpClient);
	static GstFlowReturn on_new_preroll_from_audio(GstAppSink * elt, void * rtpClient);
	static GstFlowReturn on_new_buffer_from_audio(GstAppSink * elt, void * data);


	ofGstUtils gst;
	ofGstUtils gstAudioIn;
	GstElement * vRTPsink;
	GstElement * vRTPCsink;
	GstElement * vRTPCsrc;

	GstElement * aRTPsink;
	GstElement * aRTPCsink;
	GstElement * aRTPCsrc;

	GstElement * dRTPsink;
	GstElement * dRTPCsink;
	GstElement * dRTPCsrc;

	GstElement * oRTPsink;
	GstElement * oRTPCsink;
	GstElement * oRTPCsrc;

	GstElement * vEncoder;
	GstElement * dEncoder;
	GstElement * aEncoder;
	GstElement * appSrcVideoRGB;
	GstElement * appSrcDepth;
	GstElement * appSrcOsc;
	GstElement * appSinkAudio;
	GstElement * appSrcAudio;
	ofxGstBufferPool<unsigned char> * bufferPool;
	ofxGstBufferPool<unsigned char> * bufferPoolDepth;
	ofxOscPacketPool oscPacketPool;
	int fps;
	GstClockTime prevTimestamp;
	unsigned long long numFrame;
	GstClockTime prevTimestampDepth;
	unsigned long long numFrameDepth;
	GstClockTime prevTimestampOsc;
	unsigned long long numFrameOsc;
	GstClockTime prevTimestampAudio;
	unsigned long long numFrameAudio;
	int width, height;

	string pipelineStr;
	string dest;
	int lastSessionNumber;

	ofxNiceStream * videoStream;
	ofxNiceStream * depthStream;
	ofxNiceStream * oscStream;
	ofxNiceStream * audioStream;

	bool firstVideoFrame;
	bool firstOscFrame;
	bool firstDepthFrame;
	bool firstAudioFrame;

	webrtc::AudioProcessing * audioProcessing;
	GstMapInfo mapinfo;
	webrtc::AudioFrame audioFrame;
	ofxGstRTPClient * client;
	GstBuffer * prevAudioBuffer;
	void sendAudioOut(webrtc::AudioFrame & frame);
};

#endif /* OFXGSTRTPSERVER_H_ */
