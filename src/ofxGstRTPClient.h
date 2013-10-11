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

class ofxGstRTPClient: public ofGstAppSink {
public:
	ofxGstRTPClient();
	virtual ~ofxGstRTPClient();

#if ENABLE_ECHO_CANCEL
	/// this has to be called before adding an audio channel
	void setEchoCancel(ofxEchoCancel & echoCancel);
#endif


	/// use this version of setup when working with direct connection
	/// to an specific IP and port
	void setup(string srcIP, int latency);
	void addAudioChannel(int port);
	void addVideoChannel(int port);
	void addDepthChannel(int port, bool depth16=false);
	void addOscChannel(int port);

#if ENABLE_NAT_TRANSVERSAL
	/// use this version of setup when working with NAT transversal
	/// usually this will be done from ofxGstXMPPRTP
	void setup(int latency);
	void addAudioChannel(ofxNiceStream * niceStream);
	void addVideoChannel(ofxNiceStream * niceStream);
	void addDepthChannel(ofxNiceStream * niceStream, bool depth16=false);
	void addOscChannel(ofxNiceStream * niceStream);
#endif

	void close();

	void play();
	void update();
	bool isFrameNewVideo();
	bool isFrameNewDepth();
	bool isFrameNewOsc();

	ofPixels & getPixelsVideo();
	ofPixels & getPixelsDepth();
	ofShortPixels & getPixelsDepth16();
	ofxOscMessage getOscMessage();


	ofParameter<int> latency;
	ofParameter<bool> drop;
	ofParameterGroup parameters;

	static string LOG_NAME;

#if ENABLE_ECHO_CANCEL
	u_int64_t getAudioOutLatencyMs();
	u_int64_t getAudioFramesProcessed();
#endif

private:
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
	void createNetworkElements(NetworkElementsProperties properties, ofxNiceStream * niceStream);
#else
	void createNetworkElements(NetworkElementsProperties properties, void *);
#endif

	void createAudioChannel(string rtpCaps);
	void createVideoChannel(string rtpCaps);
	void createDepthChannel(string rtpCaps, bool depth16=false);
	void createOscChannel(string rtpCaps);

	// calbacks from gstUtils
	bool on_message(GstMessage * msg);
	GstFlowReturn on_preroll(GstSample * buffer);
	GstFlowReturn on_buffer(GstSample * buffer);
	void on_stream_prepared();

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
	GstElement * dh264depay;
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


	ofxGstVideoDoubleBuffer<unsigned char> doubleBufferVideo;
	ofxGstVideoDoubleBuffer<unsigned char> doubleBufferDepth;
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
	ofxNiceStream * videoStream;
	ofxNiceStream * depthStream;
	ofxNiceStream * oscStream;
	ofxNiceStream * audioStream;
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
