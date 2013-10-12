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

#include "ofxGstRTPConstants.h"

#include "ofxOsc.h"
#include "ofxOscPacketPool.h"

#if ENABLE_NAT_TRANSVERSAL
	#include "ofxNice.h"
	#include "ofxXMPP.h"
#endif


#if ENABLE_ECHO_CANCEL
	#include "ofxWebRTCAudioPool.h"
	#include "ofxEchoCancel.h"
#endif

class ofxGstRTPClient;

template<typename PixelType>
class ofxGstBufferPool;


class ofxGstRTPServer: public ofGstAppSink {
public:
	ofxGstRTPServer();
	virtual ~ofxGstRTPServer();


#if ENABLE_ECHO_CANCEL
	/// this needs to be called before adding an audio channel if we want echo cancellation
	void setEchoCancel(ofxEchoCancel & echoCancel);
	void setRTPClient(ofxGstRTPClient & client);
#endif

	/// use this version of setup when working with direct connection
	/// to an specific IP and port
	void setup(string destinationAddress);
	void addVideoChannel(int port, int w, int h, int fps);
	void addAudioChannel(int port);
	void addDepthChannel(int port, int w, int h, int fps, bool depth16=false);
	void addOscChannel(int port);

#if ENABLE_NAT_TRANSVERSAL
	/// use this version of setup when working with NAT transversal
	/// usually this will be done from ofxGstXMPPRTP
	void setup();
	void addVideoChannel(ofxNiceStream * niceStream, int w, int h, int fps);
	void addAudioChannel(ofxNiceStream * niceStream);
	void addDepthChannel(ofxNiceStream * niceStream, int w, int h, int fps, bool depth16=false);
	void addOscChannel(ofxNiceStream * niceStream);
#endif

	void close();

	void play();

	void emitVideoKeyFrame();
	void emitDepthKeyFrame();

	void newFrame(ofPixels & pixels);
	void newFrameDepth(ofPixels & pixels);
	void newFrameDepth(ofShortPixels & pixels);

	void newOscMsg(ofxOscMessage & msg);

	bool on_message(GstMessage * msg);

	ofParameterGroup parameters;
	ofParameter<int> videoBitrate;
	ofParameter<int> depthBitrate;
	ofParameter<int> audioBitrate;
	ofParameter<bool> reverseDriftCalculation;

	static string LOG_NAME;

private:
	void vBitRateChanged(int & bitrate);
	void dBitRateChanged(int & bitrate);
	void aBitRateChanged(int & bitrate);
	void appendMessage( ofxOscMessage& message, osc::OutboundPacketStream& p );
	static void on_new_ssrc_handler(GstBin *rtpbin, guint session, guint ssrc, ofxGstRTPServer * rtpClient);
	void update(ofEventArgs& args);


	ofGstUtils gst;
	ofGstUtils gstAudioIn;
	GstElement * rtpbin;
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
	guint lastSessionNumber;
	guint audioSessionNumber, videoSessionNumber, depthSessionNumber, oscSessionNumber;
	guint audioSSRC, videoSSRC, depthSSRC, oscSSRC;
	bool sendVideoKeyFrame, sendDepthKeyFrame;

#if ENABLE_NAT_TRANSVERSAL
	ofxNiceStream * videoStream;
	ofxNiceStream * depthStream;
	ofxNiceStream * oscStream;
	ofxNiceStream * audioStream;
#endif

	bool firstVideoFrame;
	bool firstOscFrame;
	bool firstDepthFrame;
	bool firstAudioFrame;

#if ENABLE_ECHO_CANCEL
	bool audioChannelReady;
	GstElement * appSinkAudio;
	GstElement * appSrcAudio;
	GstElement * audiocapture;
	GstElement * volume;

	static void on_eos_from_audio(GstAppSink * elt, void * rtpClient);
	static GstFlowReturn on_new_preroll_from_audio(GstAppSink * elt, void * rtpClient);
	static GstFlowReturn on_new_buffer_from_audio(GstAppSink * elt, void * data);

	ofxEchoCancel * echoCancel;
	GstMapInfo mapinfo;
	ofxGstRTPClient * client;
	GstBuffer * prevAudioBuffer;
	unsigned long long audioFramesProcessed;
	u_int64_t analogAudio;
	ofxWebRTCAudioPool audioPool;
	void sendAudioOut(PooledAudioFrame * pooledFrame);
#endif


	// rctp stats stream adjustment
	int videoPacketsLost, depthPacketsLost;
};

#endif /* OFXGSTRTPSERVER_H_ */
