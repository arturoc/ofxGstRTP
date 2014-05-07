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

#include "ofxDepthStreamCompression.h"

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


/// Server part implementing RTP. Allows to send audio, video, depth
/// and metadata through osc to a remote peer. All the channels
/// will be synchronized and the communication can be started specifying the
/// ip and port of the remote side or using ofxNice streams.
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
	/// to an specific IP and port, usually in LANs when there's no need
	/// for NAT transversal.
	void setup(string destinationAddress);

	/// add a video channel sending from a specific port, has to be the same port
	/// specified in the client. Ports for the different channels will really occupy
	/// the next 5 ports so if we specify 3000, 3000-3005 will be used and shouldn't
	/// be specified for other channel
	/// autotimestamp, specifies if the gstreamer will create timestamps automatically (true)
	/// or we want to generate them internally or externally (false)
	void addVideoChannel(int port, int w, int h, int fps, bool autotimestamp=false);

	/// add an audio channel sending from a specific port, has to be the same port
	/// specified in the client. Ports for the different channels will really occupy
	/// the next 5 ports so if we specify 3000, 3000-3005 will be used and shouldn't
	/// be specified for other channel
	/// autotimestamp, specifies if the gstreamer will create timestamps automatically (true)
	/// or we want to generate them internally or externally (false)
	void addAudioChannel(int port, bool autotimestamp=false);

	/// add a depth channel sending from a specific port, has to be the same port
	/// specified in the client. Ports for the different channels will really occupy
	/// the next 5 ports so if we specify 3000, 3000-3005 will be used and shouldn't
	/// be specified for other channel
	/// autotimestamp, specifies if the gstreamer will create timestamps automatically (true)
	/// or we want to generate them internally or externally (false)
	void addDepthChannel(int port, int w, int h, int fps, bool depth16=false, bool autotimestamp=false);

	/// add an osc channel sending from a specific port, has to be the same port
	/// specified in the client. Ports for the different channels will really occupy
	/// the next 5 ports so if we specify 3000, 3000-3005 will be used and shouldn't
	/// be specified for other channel
	/// autotimestamp, specifies if the gstreamer will create timestamps automatically (true)
	/// or we want to generate them internally or externally (false)
	void addOscChannel(int port, bool autotimestamp=false);

#if ENABLE_NAT_TRANSVERSAL
	/// use this version of setup when working with NAT transversal
	/// usually this will be done from ofxGstXMPPRTP which also controls
	/// all the workflow of the session initiation as well as creating
	/// the corresponging ICE streams and agent
	void setup();
	void addVideoChannel(shared_ptr<ofxNiceStream>, int w, int h, int fps, bool autotimestamp=false);
	void addAudioChannel(shared_ptr<ofxNiceStream>, bool autotimestamp=false);
	void addDepthChannel(shared_ptr<ofxNiceStream>, int w, int h, int fps, bool depth16=false, bool autotimestamp=false);
	void addOscChannel(shared_ptr<ofxNiceStream>, bool autotimestamp=false);
#endif

	/// close the current connection
	void close();

	/// starts the gstreamer pipeline
	void play();

	/// generate a keyframe on the video stream, used by other parts of the
	/// addon to avoid glitches when some packages are lost
	void emitVideoKeyFrame();

	/// generate a keyframe on the depth stream, used by other parts of the
	/// addon to avoid glitches when some packages are lost
	void emitDepthKeyFrame();

	/// ofxGstRTPServer will generate timestamps for every channel if the
	/// corresponding newFrame* method is called with timestamp = GST_CLOCK_TIME_NONE
	/// In some cases is better to get a timestamp each update and use that for
	/// every channel to improve sync
	GstClockTime getTimeStamp();

	/// Should be called when there's a new video frame, if timestamp is not
	/// specified, will generate one internally
	void newFrame(ofPixels & pixels, GstClockTime timestamp=GST_CLOCK_TIME_NONE);

	/// Should be called when there's a new depth frame, if timestamp is not
	/// specified, will generate one internally
	void newFrameDepth(ofPixels & pixels, GstClockTime timestamp=GST_CLOCK_TIME_NONE);

	/// Should be called when there's a new 16bits depth frame, if timestamp is not
	/// specified, will generate one internally
	/// pixel_size and distance are the calibration parameter from the kinect
	void newFrameDepth(ofShortPixels & pixels, GstClockTime timestamp=GST_CLOCK_TIME_NONE, float pixel_size=1, float distance=1);

	/// Should be called when there's a new osc message, if timestamp is not
	/// specified, will generate one internally
	void newOscMsg(ofxOscMessage & msg, GstClockTime timestamp=GST_CLOCK_TIME_NONE);

	/// groups all the parameters of this class
	ofParameterGroup parameters;

	/// parameter to adjust the bitrate of the video stream, can be adjunsted
	/// on runtime
	ofParameter<int> videoBitrate;

	/// parameter to adjust the bitrate of the depth stream, can be adjusted
	/// on runtime
	ofParameter<int> depthBitrate;

	/// parameter to adjust the bitrate of the audio stream, can be adjusted
	/// on runtime
	ofParameter<int> audioBitrate;

	/// parameter to change the type of calculation for the drift when doing
	/// echo cancellation
	ofParameter<bool> reverseDriftCalculation;

	static string LOG_NAME;

private:

	bool on_message(GstMessage * msg);
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
	shared_ptr<ofxNiceStream> videoStream;
	shared_ptr<ofxNiceStream> depthStream;
	shared_ptr<ofxNiceStream> oscStream;
	shared_ptr<ofxNiceStream> audioStream;
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

	ofxDepthStreamCompression depthCompressor;

	// rctp stats stream adjustment
	int videoPacketsLost, depthPacketsLost;

	bool videoAutoTimestamp, depthAutoTimestamp, audioAutoTimestamp, oscAutoTimestamp;
};

#endif /* OFXGSTRTPSERVER_H_ */
