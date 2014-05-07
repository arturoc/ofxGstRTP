/*
 * ofxGstRTPClient.h
 *
 *  Created on: Jul 20, 2013
 *      Author: arturo castro
 */

#ifndef OFXGSTRTPCLIENT_H_
#define OFXGSTRTPCLIENT_H_

#include "ofGstUtils.h"
#include <gst/app/gstappsink.h>
#include "ofxGstVideoDoubleBuffer.h"
#include "ofxOsc.h"
#include "ofxGstOscDoubleBuffer.h"
#include "ofxGstRTPConstants.h"

#include "ofParameter.h"
#include "ofParameterGroup.h"

#if ENABLE_NAT_TRANSVERSAL
#include <agent.h>
#include "ofxNice.h"
#include "ofxXMPP.h"
#endif

#if ENABLE_ECHO_CANCEL
#include "ofxEchoCancel.h"
#include "ofxWebRTCAudioPool.h"
#endif


/// Client part implementing the RTP protocol. Allows to receive audio,
/// video, depth and metadata through osc from a remote peer. All the channels
/// will be synchronized and the communication can be started specifying the
/// ip and port of the remote side or using ofxNice streams.
class ofxGstRTPClient: public ofGstAppSink {
public:
	ofxGstRTPClient();
	virtual ~ofxGstRTPClient();

#if ENABLE_ECHO_CANCEL
	/// this has to be called before adding an audio channel
	void setEchoCancel(ofxEchoCancel & echoCancel);
#endif


	/// use this version of setup when working with direct connection
	/// to an specific IP and port, usually in LANs when there's no need
	/// for NAT transversal.
	/// the latency parameter specifies a latency in milliseconds so the client
	/// buffers the received data to make the conection more reliable. The latency
	/// can be adjusted afterwards, but this will be the maximum latency the client can
	/// set later
	void setup(string srcIP, int latency);

	/// add an audio channel receiving in a specific port, has to be the same port
	/// specified in the server. Ports for the different channels will really occupy
	/// the next 5 ports so if we specify 3000, 3000-3005 will be used and shouldn't
	/// be specified for other channel
	void addAudioChannel(int port);
	/// add an video channel receiving in a specific port. Ports for the different channels will really occupy
	/// the next 5 ports so if we specify 3000, 3000-3005 will be used and shouldn't
	/// be specified for other channel
	void addVideoChannel(int port);
	/// add an depth channel receiving in a specific port. Ports for the different channels will really occupy
	/// the next 5 ports so if we specify 3000, 3000-3005 will be used and shouldn't
	/// be specified for other channel
	void addDepthChannel(int port, bool depth16=false);
	/// add an osc channel receiving in a specific port. Ports for the different channels will really occupy
	/// the next 5 ports so if we specify 3000, 3000-3005 will be used and shouldn't
	/// be specified for other channel
	void addOscChannel(int port);

#if ENABLE_NAT_TRANSVERSAL
	/// use this version of setup when working with NAT transversal
	/// usually this will be done from ofxGstXMPPRTP which also controls
	/// all the workflow of the session initiation as well as creating
	/// the corresponging ICE streams and agent
	void setup(int latency);
	void addAudioChannel(shared_ptr<ofxNiceStream> niceStream);
	void addVideoChannel(shared_ptr<ofxNiceStream> niceStream);
	void addDepthChannel(shared_ptr<ofxNiceStream> niceStream, bool depth16=false);
	void addOscChannel(shared_ptr<ofxNiceStream> niceStream);
#endif

	/// close the current connection
	void close();

	/// starts the gstreamer pipeline
	void play();

	/// update the pipeline and receive any available buffers
	void update();

	/// returns true if there's a new video frame after calling update
	bool isFrameNewVideo();

	/// returns true if there's a new depth frame after calling update
	bool isFrameNewDepth();

	/// returns true if there's a new osc frame after calling update
	bool isFrameNewOsc();

	/// get the pixels for the last frame received for the video channel
	ofPixels & getPixelsVideo();
	/// get the pixels for the last frame received for the depth channel
	ofPixels & getPixelsDepth();
	/// get the pixels for the last frame received for the depth channel 16bits
	ofShortPixels & getPixelsDepth16();
	/// get the pixels for the last frame received for the osc channel
	ofxOscMessage getOscMessage();
	/// get the zero plane pixel size, of the remote peer, used to undistort the
	/// received point cloud
	float getZeroPlanePixelSize();
	/// get the zero plane distance of the remote peer, used to undistort the
	/// received point cloud
	float getZeroPlaneDistance();



	/// this paramter adjusts the latency on the client side to a maximum of the
	/// value set in setup
	ofParameter<int> latency;

	/// sets if the client should drop frames or accumulate them in a buffer,
	/// if the application doens't read fast enough this can cause a grow in memory
	/// but dropping frames can have the effect of dropping a key frame leading to
	/// glitches in the video and depth streams
	ofParameter<bool> drop;

	/// groups all the parameters of this class
	ofParameterGroup parameters;

	ofEvent<void> disconnectedEvent;

	static string LOG_NAME;

#if ENABLE_ECHO_CANCEL
	u_int64_t getAudioOutLatencyMs();
	u_int64_t getAudioFramesProcessed();
#endif

private:
	void requestKeyFrame();
	void latencyChanged(int & latency);
	void dropChanged(bool & drop);

	struct NetworkElementsProperties{
		GstElement ** source;
		GstElement ** rtpcsource;
		GstElement ** rtpcsink;
		string srcIP;
		string capsstr;
		string capsfiltername;
		int port;
		int rtpcsrcport;
		int rtpcsinkport;
		int sessionNumber;
		string sourceName, rtpcSourceName, rtpcSinkName;
	};

#if ENABLE_NAT_TRANSVERSAL
	void createNetworkElements(NetworkElementsProperties properties, shared_ptr<ofxNiceStream> niceStream);
#else
	void createNetworkElements(NetworkElementsProperties properties, void *);
#endif

	void createAudioChannel(string rtpCaps);
	void createVideoChannel(string rtpCaps);
	void createDepthChannel(string rtpCaps, bool depth16=false);
	void createOscChannel(string rtpCaps);

	// calbacks from gstUtils
	bool on_message(GstMessage * msg);
	void on_stream_prepared();
	void on_eos();

	// signal handlers for rtpc
	static void on_ssrc_active_handler(GstBin * rtpbin, guint session, guint ssrc, ofxGstRTPClient * rtpClient);
	static void on_new_ssrc_handler(GstBin *rtpbin, guint session, guint ssrc, ofxGstRTPClient * rtpClient);
	static void on_bye_ssrc_handler(GstBin *rtpbin, guint session, guint ssrc, ofxGstRTPClient * rtpClient);
	static void on_pad_added(GstBin *rtpbin, GstPad *pad, ofxGstRTPClient * rtpClient);

	// video callbacks
	static void on_eos_from_video(GstAppSink * elt, void * rtpClient);
	static GstFlowReturn on_new_preroll_from_video(GstAppSink * elt, void * rtpClient);
	static GstFlowReturn on_new_buffer_from_video(GstAppSink * elt, void * rtpClient);

	// depth callbacks
	static void on_eos_from_depth(GstAppSink * elt, void * rtpClient);
	static GstFlowReturn on_new_preroll_from_depth(GstAppSink * elt, void * rtpClient);
	static GstFlowReturn on_new_buffer_from_depth(GstAppSink * elt, void * rtpClient);

	// osc callbacks
	static void on_eos_from_osc(GstAppSink * elt, void * rtpClient);
	static GstFlowReturn on_new_preroll_from_osc(GstAppSink * elt, void * rtpClient);
	static GstFlowReturn on_new_buffer_from_osc(GstAppSink * elt, void * rtpClient);

#if ENABLE_ECHO_CANCEL
	// audio echo cancel callbacks
	static void on_eos_from_audio(GstAppSink * elt, void * rtpClient);
	static GstFlowReturn on_new_preroll_from_audio(GstAppSink * elt, void * rtpClient);
	static GstFlowReturn on_new_buffer_from_audio(GstAppSink * elt, void * rtpClient);
#endif

	// video instance callbacks
	void on_eos_from_video(GstAppSink * elt){};
	GstFlowReturn on_new_preroll_from_video(GstAppSink * elt){return GST_FLOW_OK;}
	GstFlowReturn on_new_buffer_from_video(GstAppSink * elt);
	void linkVideoPad(GstPad * pad);

	// depth instance callbacks
	void on_eos_from_depth(GstAppSink * elt){};
	GstFlowReturn on_new_preroll_from_depth(GstAppSink * elt){return GST_FLOW_OK;};
	GstFlowReturn on_new_buffer_from_depth(GstAppSink * elt);
	void linkDepthPad(GstPad * pad);

	// osc instance callbacks
	void on_eos_from_osc(GstAppSink * elt){};
	GstFlowReturn on_new_preroll_from_osc(GstAppSink * elt){return GST_FLOW_OK;};
	GstFlowReturn on_new_buffer_from_osc(GstAppSink * elt);
	void linkOscPad(GstPad * pad);

	void linkAudioPad(GstPad * pad);

	ofGstUtils gst;
	ofGstUtils gstAudioOut;
	int width, height;
	GstMapInfo mapinfo;

	GstElement * pipeline;
	GstElement * pipelineAudioOut;
	GstElement * rtpbin;

	GstElement * vh264depay;
	GstElement * opusdepay;
	GstElement * depthdepay;
	GstElement * gstdepay;

	GstAppSink * videoSink;
	GstAppSink * depthSink;
	GstAppSink * oscSink;

	GstElement * vqueue;
	GstElement * dqueue;

	GstElement * vudpsrc;
	GstElement * audpsrc;
	GstElement * dudpsrc;
	GstElement * oudpsrc;
	GstElement * vudpsrcrtcp;
	GstElement * audpsrcrtcp;
	GstElement * dudpsrcrtcp;
	GstElement * oudpsrcrtcp;

	GstElement * audioechosrc;
	GstElement * audioechosink;


	ofxGstVideoDoubleBuffer<unsigned char> doubleBufferVideo;
	ofxGstVideoDoubleBuffer<unsigned char> doubleBufferDepth;
	ofxGstVideoDoubleBuffer<unsigned short> doubleBufferDepth16;
	ofxGstOscDoubleBuffer doubleBufferOsc;

	bool depth16;
	ofShortPixels depth16Pixels;

	string src;

	int videoSessionNumber;
	int audioSessionNumber;
	int depthSessionNumber;
	int oscSessionNumber;
	int lastSessionNumber;

	guint videoSSRC;
	guint depthSSRC;
	guint audioSSRC;
	guint oscSSRC;

	bool videoReady;
	bool depthReady;
	bool audioReady;
	bool oscReady;

#if ENABLE_NAT_TRANSVERSAL
	// ICE/XMPP related
	shared_ptr<ofxNiceStream> videoStream;
	shared_ptr<ofxNiceStream> depthStream;
	shared_ptr<ofxNiceStream> oscStream;
	shared_ptr<ofxNiceStream> audioStream;
	ofxXMPPJingleInitiation remoteJingle;
#endif


#if ENABLE_ECHO_CANCEL
	bool audioChannelReady;
	ofxEchoCancel * echoCancel;
	GstClockTime prevTimestampAudio;
	unsigned long long numFrameAudio;
	bool firstAudioFrame;
	GstBuffer * prevAudioBuffer;
	unsigned long long audioFramesProcessed;
	ofxWebRTCAudioPool audioPool;
	void sendAudioOut(PooledAudioFrame * pooledFrame);
#endif
};

#endif /* OFXGSTRTPCLIENT_H_ */
