/*
 * ofxGstRTPServer.cpp
 *
 *  Created on: Jul 19, 2013
 *      Author: arturo
 */

#include "ofxGstRTPServer.h"
#include <gst/app/gstappsink.h>
#include <gst/app/gstappsrc.h>
#include <gst/video/video.h>
#include <gst/video/gstvideometa.h>

#include <glib-object.h>
#include <glib.h>

#include <agent.h>

#include "ofxGstPixelsPool.h"
#include "ofxGstRTPUtils.h"
#include "module_common_types.h"

#include "ofxGstRTPClient.h"

//  sends the output of v4l2src as h264 encoded RTP on port 5000, RTCP is sent on
//  port 5001. The destination is 127.0.0.1.
//  the video receiver RTCP reports are received on port 5005
//  sends the output of autoaudiosrc as OPUS encoded RTP on port 5002, RTCP is sent on
//  port 5003. The destination is 127.0.0.1.
//  the receiver RTCP reports are received on port 5007
//
//  .-------.    .-------.    .-------.      .----------.     .-------.
//  |appsrc |    |h264enc|    |h264pay|      | rtpbin   |     |udpsink|  RTP
//  |video src->sink    src->sink    src->send_rtp send_rtp->sink     | port=5000
//  '-------'    '-------'    '-------'      |          |     '-------'
//                                           |          |
//                                           |          |     .-------.
//                                           |          |     |udpsink|  RTCP
//                                           |    send_rtcp->sink     | port=5001
//                            .-------.      |          |     '-------' sync=false
//                 RTCP       |udpsrc |      |          |               async=false
//               port=5005    |     src->recv_rtcp      |
//                            '-------'      |          |
//                                           |          |
//                                           |          |
//                                           |          |
// .--------.    .-------.    .-------.      |          |     .-------.
// |audiosrc|    |opusenc|    |opuspay|      | rtpbin   |     |udpsink|  RTP
// |       src->sink    src->sink    src->send_rtp send_rtp->sink     | port=5002
// '--------'    '-------'    '-------'      |          |     '-------'
//                                           |          |
//                                           |          |     .-------.
//                                           |          |     |udpsink|  RTCP
//                                           |    send_rtcp->sink     | port=5003
//                            .-------.      |          |     '-------' sync=false
//                 RTCP       |udpsrc |      |          |               async=false
//               port=5007    |     src->recv_rtcp      |
//                            '-------'      |          |
//                                           |          |
//                                           |          |
//                                           |          |
//  .-------.    .-------.    .-------.      |          |     .-------.
//  |appsrc |    |h264enc|    |h264pay|      |          |     |udpsink|  RTP
//  |depth src->sink    src->sink    src->send_rtp send_rtp->sink     | port=5008
//  '-------'    '-------'    '-------'      |          |     '-------'
//                                           |          |
//                                           |          |     .-------.
//                                           |          |     |udpsink|  RTCP
//                                           |    send_rtcp->sink     | port=5009
//                            .-------.      |          |     '-------' sync=false
//                 RTCP       |udpsrc |      |          |               async=false
//               port=5011    |     src->recv_rtcp      |
//                            '-------'      |          |
//                                           |          |
//                                           '----------'

string ofxGstRTPServer::LOG_NAME="ofxGstRTPServer";

ofxGstRTPServer::ofxGstRTPServer()
:vRTPsink(NULL)
,vRTPCsink(NULL)
,vRTPCsrc(NULL)
,vEncoder(NULL)
,dEncoder(NULL)
,aEncoder(NULL)
,appSrcVideoRGB(NULL)
,appSrcDepth(NULL)
,appSrcOsc(NULL)
,bufferPool(NULL)
,bufferPoolDepth(NULL)
,fps(0)
,prevTimestamp(0)
,numFrame(0)
,prevTimestampDepth(0)
,numFrameDepth(0)
,prevTimestampOsc(0)
,numFrameOsc(0)
,prevTimestampAudio(0)
,numFrameAudio(0)
,width(0)
,height(0)
,lastSessionNumber(0)
,videoStream(NULL)
,depthStream(NULL)
,oscStream(NULL)
,audioStream(NULL)
,firstVideoFrame(true)
,firstOscFrame(true)
,firstDepthFrame(true)
,firstAudioFrame(true)
,echoCancel(0)
,prevAudioBuffer(NULL)
,audioFramesProcessed(0)
,analogAudio(0x10000U)
{
	videoBitrate.set("video bitrate",1024,0,6000);
	videoBitrate.addListener(this,&ofxGstRTPServer::vBitRateChanged);
	audioBitrate.set("audio bitrate",4000,0,650000);
	audioBitrate.addListener(this,&ofxGstRTPServer::aBitRateChanged);
	parameters.setName("gst rtp server");
	parameters.add(videoBitrate);
	parameters.add(audioBitrate);

	GstMapInfo mapinfo = {0,};
	this->mapinfo=mapinfo;
}

ofxGstRTPServer::~ofxGstRTPServer() {
}


void ofxGstRTPServer::addVideoChannel(int port, int w, int h, int fps, int bitrate){
	int sessionNumber = lastSessionNumber;
	lastSessionNumber++;

	videoBitrate.disableEvents();
	videoBitrate = bitrate;
	videoBitrate.enableEvents();
	// video elements
	// ------------------
		// appsrc, allows to pass new frames from the app using the newFrame method
		string velem="appsrc is-live=1 format=time name=appsrcvideo";

		// video format that we are pushing to the pipeline
		string vcaps="video/x-raw,format=RGB,width="+ofToString(w)+ ",height="+ofToString(h)+",framerate="+ofToString(fps)+"/1";

		// queue so the conversion and encoding happen in a different thread to appsrc
		string vsource= velem + " ! queue leaky=2 max-size-buffers=100 ! " + vcaps + " ! videoconvert name=vconvert1";

		// h264 encoder + rtp pay
		string venc="x264enc tune=zerolatency byte-stream=true bitrate=" + ofToString(bitrate) +" speed-preset=1 name=vencoder ! video/x-h264,width="+ofToString(w)+ ",height="+ofToString(h)+",framerate="+ofToString(fps)+"/1 ! rtph264pay pt=96";

	// video rtpc
	// ------------------
		string vrtpsink;
		string vrtpcsink;
		string vrtpcsrc;

		if(videoStream){
			vrtpsink="nicesink ts-offset=0 name=vrtpsink";
			vrtpcsink="nicesink  sync=false async=false name=vrtcpsink";
			vrtpcsrc="nicesrc name=vrtcpsrc";
		}else{
			vrtpsink="udpsink port=" + ofToString(port) + " host="+dest+" ts-offset=0 force-ipv4=1 name=vrtpsink";
			vrtpcsink="udpsink port=" + ofToString(port+1) + " host="+dest+" sync=false async=false force-ipv4=1 name=vrtcpsink";
			vrtpcsrc="udpsrc port=" + ofToString(port+3) + " name=vrtcpsrc";
		}

	pipelineStr += " " + vsource + " ! " + venc + " ! rtpbin.send_rtp_sink_" + ofToString(sessionNumber) +
				" rtpbin.send_rtp_src_" + ofToString(sessionNumber) + " ! " + vrtpsink +
				" rtpbin.send_rtcp_src_" + ofToString(sessionNumber) + " ! " + vrtpcsink +
				" " + vrtpcsrc + " ! rtpbin.recv_rtcp_sink_" + ofToString(sessionNumber) + " ";

	// create a pixels pool of the correct w,h and bpp to use on newFrame
	bufferPool = new ofxGstBufferPool<unsigned char>(w,h,3);
}


void ofxGstRTPServer::addAudioChannel(int port){
	int sessionNumber = lastSessionNumber;
	lastSessionNumber++;

	// audio elements
	//-------------------
		// audio source
		string aelem = "appsrc is-live=1 format=time name=audioechosrc ! audio/x-raw,format=S16LE,rate=32000,channels=1 ";

		// audio source + queue for threading + audio resample and convert
		// to change sampling rate and format to something supported by the encoder
		string asource=aelem + " ! audioresample ! audioconvert";

		// opus encoder + opus pay
		// FIXME: audio=0 is voice??
		string aenc="opusenc name=aencoder audio=0 ! rtpopuspay pt=97";

	// audio rtpc
		string artpsink;
		string artpcsink;
		string artpcsrc;

		if(audioStream){
			artpsink="nicesink ts-offset=0 name=artpsink";
			artpcsink="nicesink sync=false async=false name=artcpsink";
			artpcsrc="nicesrc name=artcpsrc";
		}else{
			artpsink="udpsink port=" + ofToString(port) + " host="+dest+" ts-offset=0 force-ipv4=1 name=artpsink";
			artpcsink="udpsink port=" + ofToString(port+1) + " host="+dest+" sync=false async=false force-ipv4=1 name=artcpsink";
			artpcsrc="udpsrc port=" + ofToString(port+3) + " name=artcpsrc";
		}


		// audio
	pipelineStr += " " +  asource + " ! " + aenc + " ! rtpbin.send_rtp_sink_" + ofToString(sessionNumber) +
			" rtpbin.send_rtp_src_" + ofToString(sessionNumber) + " ! " + artpsink +
			" rtpbin.send_rtcp_src_" + ofToString(sessionNumber) + " ! " + artpcsink +
			" " + artpcsrc + " ! rtpbin.recv_rtcp_sink_" + ofToString(sessionNumber) + " ";
}

void ofxGstRTPServer::addDepthChannel(int port, int w, int h, int fps, int bitrate, bool depth16){
	int sessionNumber = lastSessionNumber;
	lastSessionNumber++;

	videoBitrate.disableEvents();
	videoBitrate = bitrate;
	videoBitrate.enableEvents();

	// depth elements
	// ------------------
		// appsrc, allows to pass new frames from the app using the newFrame method
		string delem="appsrc is-live=1 format=time name=appsrcdepth";

		// video format that we are pushing to the pipeline
		string dcaps;
		if(depth16){
			dcaps="video/x-raw,format=RGB,width="+ofToString(w)+ ",height="+ofToString(h)+",framerate="+ofToString(fps)+"/1";
		}else{
			dcaps="video/x-raw,format=GRAY8,width="+ofToString(w)+ ",height="+ofToString(h)+",framerate="+ofToString(fps)+"/1";
		}

		// queue so the conversion and encoding happen in a different thread to appsrc
		string dsource= delem + " ! queue leaky=2 max-size-buffers=100 ! " + dcaps + " ! videoconvert name=dconvert1";

		// h264 encoder + rtp pay
		string denc="x264enc tune=zerolatency byte-stream=true bitrate="+ofToString(bitrate)+" speed-preset=1 name=dencoder ! video/x-h264,width="+ofToString(w)+ ",height="+ofToString(h)+",framerate="+ofToString(fps)+"/1 ! rtph264pay pt=98";

	// depth rtpc
	// ------------------
		string drtpsink;
		string drtpcsink;
		string drtpcsrc;

		if(depthStream){
			drtpsink="nicesink ts-offset=0 name=drtpsink";
			drtpcsink="nicesink sync=false async=false name=drtcpsink";
			drtpcsrc="nicesrc name=drtcpsrc";
		}else{
			drtpsink="udpsink port=" + ofToString(port) + " host="+dest+" ts-offset=0 force-ipv4=1 name=drtpsink";
			drtpcsink="udpsink port=" + ofToString(port+1) + " host="+dest+" sync=false async=false force-ipv4=1 name=drtcpsink";
			drtpcsrc="udpsrc port=" + ofToString(port+3) + " name=drtcpsrc";
		}

	// depth
	pipelineStr += " " +  dsource + " ! " + denc + " ! rtpbin.send_rtp_sink_" + ofToString(sessionNumber) +
		" rtpbin.send_rtp_src_" + ofToString(sessionNumber) + " ! " + drtpsink +
		" rtpbin.send_rtcp_src_" + ofToString(sessionNumber) + " ! " + drtpcsink +
		" " + drtpcsrc + " ! rtpbin.recv_rtcp_sink_" + ofToString(sessionNumber) + " ";

	if(depth16){
		bufferPoolDepth = new ofxGstBufferPool<unsigned char>(w,h,3);
	}else{
		bufferPoolDepth = new ofxGstBufferPool<unsigned char>(w,h,1);
	}
}

void ofxGstRTPServer::addOscChannel(int port){
	int sessionNumber = lastSessionNumber;
	lastSessionNumber++;

	// osc elements
	// ------------------
		// appsrc, allows to pass new frames from the app using the newFrame method
		string oelem="appsrc is-live=1 format=time name=appsrcosc ! application/x-osc ";

		// queue so the conversion and encoding happen in a different thread to appsrc
		string osource= oelem;

		// rtp pay
		string oenc=" rtpgstpay pt=99";

	// osc rtpc
	// ------------------
		string ortpsink;
		string ortpcsink;
		string ortpcsrc;

		if(oscStream){
			ortpsink="nicesink ts-offset=0 name=ortpsink";
			ortpcsink="nicesink sync=false async=false name=ortcpsink";
			ortpcsrc="nicesrc name=ortcpsrc";
		}else{
			ortpsink="udpsink port=" + ofToString(port) + " host="+dest+" ts-offset=0 force-ipv4=1 name=ortpsink";
			ortpcsink="udpsink port=" + ofToString(port+1) + " host="+dest+" sync=false async=false force-ipv4=1 name=ortcpsink";
			ortpcsrc="udpsrc port=" + ofToString(port+3) + " name=ortcpsrc";

		}

	// osc
	pipelineStr += " " + osource + " ! " + oenc + " ! rtpbin.send_rtp_sink_" + ofToString(sessionNumber) +
		" rtpbin.send_rtp_src_" + ofToString(sessionNumber) + " ! " + ortpsink +
		" rtpbin.send_rtcp_src_" + ofToString(sessionNumber) + " ! " + ortpcsink +
		" " + ortpcsrc + " ! rtpbin.recv_rtcp_sink_" + ofToString(sessionNumber) + " ";
}


void ofxGstRTPServer::addVideoChannel(ofxNiceStream * niceStream, int w, int h, int fps, int bitrate){
	videoStream = niceStream;
	addVideoChannel(0,w,h,fps,bitrate);
}

void ofxGstRTPServer::addAudioChannel(ofxNiceStream * niceStream){
	audioStream = niceStream;
	addAudioChannel(0);
}

void ofxGstRTPServer::addDepthChannel(ofxNiceStream * niceStream, int w, int h, int fps, int bitrate, bool depth16){
	depthStream = niceStream;
	addDepthChannel(0,w,h,fps,bitrate,depth16);
}

void ofxGstRTPServer::addOscChannel(ofxNiceStream * niceStream){
	oscStream = niceStream;
	addOscChannel(0);
}

void ofxGstRTPServer::setup(string dest){
	this->dest = dest;
	// full pipeline
	// FIXME: we should set this more modularly to allow to negociate the formats
	// through rtpc with the server.
	// force-ipv4 is needed on all osx udpsinks or it'll fail
	pipelineStr = "rtpbin name=rtpbin ";


	// set this class as listener so we can get messages from the pipeline
	gst.setSinkListener(this);
	gstAudioIn.setSinkListener(this);



	// properties introspection, this allows to create ofParameters from reading the gstElement proeprties
	/*guint n_properties;
	GParamSpec** properties;
	properties = g_object_class_list_properties(G_OBJECT_GET_CLASS(vEncoder),&n_properties);
	cout << "writable on run time" << endl;
	for(guint i=0; i<n_properties; i++){
		if((properties[i]->flags & G_PARAM_WRITABLE) && !(properties[i]->flags & G_PARAM_CONSTRUCT_ONLY)){
			cout << properties[i]->name;
			if(properties[i]->value_type==G_TYPE_INT
					|| properties[i]->value_type==G_TYPE_UINT
					|| properties[i]->value_type==G_TYPE_LONG
					|| properties[i]->value_type==G_TYPE_ULONG
					|| properties[i]->value_type==G_TYPE_INT64
					|| properties[i]->value_type==G_TYPE_UINT64){

				ofParameter<int> p(properties[i]->name,0,0,1000);
				parameters.add(p);
				cout << " int "<< endl;
			}
			else if(properties[i]->value_type==G_TYPE_BOOLEAN) {
				ofParameter<bool> p(properties[i]->name,false);
				parameters.add(p);
				cout << " boolean " << endl;
			}
			else if(properties[i]->value_type==G_TYPE_STRING){
				ofParameter<string> p(properties[i]->name,"");
				parameters.add(p);
				cout << " string " << endl;
			}
			else if(properties[i]->value_type==G_TYPE_ENUM)   cout << " enum " << endl;
			else cout << endl;
		}
	}
	cout << "readonly" << endl;
	for(guint i=0; i<n_properties; i++){
		if((properties[i]->flags & G_PARAM_READABLE) && !(properties[i]->flags & G_PARAM_WRITABLE))
			cout << properties[i]->name << endl;
	}*/


}

void ofxGstRTPServer::setRTPClient(ofxGstRTPClient & client){
	this->client = &client;
}

void ofxGstRTPServer::setEchoCancel(ofxEchoCancel & echoCancel){
	this->echoCancel = &echoCancel;
}

void ofxGstRTPServer::setup(){
	setup("");
}

void ofxGstRTPServer::close(){
	gst.close();
	vRTPsink = NULL;
	vRTPCsink = NULL;
	vRTPCsrc = NULL;
	vEncoder = NULL;
	dEncoder = NULL;
	aEncoder = NULL;
	appSrcVideoRGB = NULL;
	appSrcDepth = NULL;
	appSrcOsc = NULL;
	bufferPool = NULL;
	bufferPoolDepth = NULL;
	fps = 0;
	prevTimestamp = 0;
	numFrame = 0;
	prevTimestampDepth = 0;
	numFrameDepth = 0;
	prevTimestampOsc = 0;
	numFrameOsc = 0;
	width = 0;
	height = 0;
	lastSessionNumber = 0;
	videoStream = NULL;
	depthStream = NULL;
	oscStream = NULL;
	audioStream = NULL;
	firstVideoFrame = true;
	firstOscFrame = true;
	firstDepthFrame = true;
}

void ofxGstRTPServer::vBitRateChanged(int & bitrate){
	g_object_set(G_OBJECT(vEncoder),"bitrate",bitrate,NULL);
	g_object_set(G_OBJECT(dEncoder),"bitrate",bitrate,NULL);
}

void ofxGstRTPServer::aBitRateChanged(int & bitrate){
	g_object_set(G_OBJECT(aEncoder),"bitrate",bitrate,NULL);
}

void ofxGstRTPServer::play(){
	// pass the pipeline to the gstUtils so it starts everything
	cout << pipelineStr << endl;
	gst.setPipelineWithSink(pipelineStr,"",true);

	// get the rtp and rtpc elements from the pipeline so we can read their properties
	// during execution
	vRTPsink = gst.getGstElementByName("vrtpsink");
	vRTPCsink = gst.getGstElementByName("vrtcpsink");
	vRTPCsrc = gst.getGstElementByName("vrtcpsrc");

	aRTPsink = gst.getGstElementByName("artpsink");
	aRTPCsink = gst.getGstElementByName("artcpsink");
	aRTPCsrc = gst.getGstElementByName("artcpsrc");

	dRTPsink = gst.getGstElementByName("drtpsink");
	dRTPCsink = gst.getGstElementByName("drtcpsink");
	dRTPCsrc = gst.getGstElementByName("drtcpsrc");

	oRTPsink = gst.getGstElementByName("ortpsink");
	oRTPCsink = gst.getGstElementByName("ortcpsink");
	oRTPCsrc = gst.getGstElementByName("ortcpsrc");

	vEncoder = gst.getGstElementByName("vencoder");
	dEncoder = gst.getGstElementByName("dencoder");
	aEncoder = gst.getGstElementByName("aencoder");
	appSrcVideoRGB = gst.getGstElementByName("appsrcvideo");
	appSrcDepth = gst.getGstElementByName("appsrcdepth");
	appSrcOsc = gst.getGstElementByName("appsrcosc");

	appSrcAudio = gst.getGstElementByName("audioechosrc");
	if(appSrcAudio){
		gst_app_src_set_stream_type((GstAppSrc*)appSrcAudio,GST_APP_STREAM_TYPE_STREAM);
	}

#ifdef TARGET_LINUX
	gstAudioIn.setPipelineWithSink("pulsesrc stream-properties=\"props,media.role=phone\" name=audiocapture ! audio/x-raw,format=S16LE,rate=44100,channels=1 ! audioresample ! audioconvert ! audio/x-raw,format=S16LE,rate=32000,channels=1 ! appsink name=audioechosink");

#elif defined(TARGET_OSX)
	// for osx we specify the output format since osxaudiosrc doesn't report the formats supported by the hw
	// FIXME: we should detect the format somehow and set it automatically
	gstAudioIn.setPipelineWithSink("osxaudiosrc ! audio/x-raw,rate=44100,channels=1 name=audiocapture ! audioresample ! audioconvert ! audio/x-raw,format=S16LE,rate=32000,channels=1 ! appsink name=audioechosink");
#endif

	appSinkAudio = gstAudioIn.getGstElementByName("audioechosink");
	audiocapture = gstAudioIn.getGstElementByName("audiocapture");


	// set callbacks to receive rgb data
	GstAppSinkCallbacks gstCallbacks;
	gstCallbacks.eos = &on_eos_from_audio;
	gstCallbacks.new_preroll = &on_new_preroll_from_audio;
	gstCallbacks.new_sample = &on_new_buffer_from_audio;
	gst_app_sink_set_callbacks(GST_APP_SINK(appSinkAudio), &gstCallbacks, this, NULL);
	gst_app_sink_set_emit_signals(GST_APP_SINK(appSinkAudio),0);


	if(videoStream){
		g_object_set(G_OBJECT(vRTPsink),"agent",videoStream->getAgent(),"stream",videoStream->getStreamID(),"component",1,NULL);
		g_object_set(G_OBJECT(vRTPCsink),"agent",videoStream->getAgent(),"stream",videoStream->getStreamID(),"component",2,NULL);
		g_object_set(G_OBJECT(vRTPCsrc),"agent",videoStream->getAgent(),"stream",videoStream->getStreamID(),"component",3,NULL);
	}
	if(depthStream){
		g_object_set(G_OBJECT(dRTPsink),"agent",depthStream->getAgent(),"stream",depthStream->getStreamID(),"component",1,NULL);
		g_object_set(G_OBJECT(dRTPCsink),"agent",depthStream->getAgent(),"stream",depthStream->getStreamID(),"component",2,NULL);
		g_object_set(G_OBJECT(dRTPCsrc),"agent",depthStream->getAgent(),"stream",depthStream->getStreamID(),"component",3,NULL);
	}
	if(audioStream){
		g_object_set(G_OBJECT(aRTPsink),"agent",audioStream->getAgent(),"stream",audioStream->getStreamID(),"component",1,NULL);
		g_object_set(G_OBJECT(aRTPCsink),"agent",audioStream->getAgent(),"stream",audioStream->getStreamID(),"component",2,NULL);
		g_object_set(G_OBJECT(aRTPCsrc),"agent",audioStream->getAgent(),"stream",audioStream->getStreamID(),"component",3,NULL);
	}
	if(oscStream){
		g_object_set(G_OBJECT(oRTPsink),"agent",oscStream->getAgent(),"stream",oscStream->getStreamID(),"component",1,NULL);
		g_object_set(G_OBJECT(oRTPCsink),"agent",oscStream->getAgent(),"stream",oscStream->getStreamID(),"component",2,NULL);
		g_object_set(G_OBJECT(oRTPCsrc),"agent",oscStream->getAgent(),"stream",oscStream->getStreamID(),"component",3,NULL);
	}


	if(appSrcVideoRGB) gst_app_src_set_stream_type((GstAppSrc*)appSrcVideoRGB,GST_APP_STREAM_TYPE_STREAM);
	if(appSrcDepth) gst_app_src_set_stream_type((GstAppSrc*)appSrcDepth,GST_APP_STREAM_TYPE_STREAM);
	if(appSrcOsc) gst_app_src_set_stream_type((GstAppSrc*)appSrcOsc,GST_APP_STREAM_TYPE_STREAM);


	gst.startPipeline();
	gstAudioIn.startPipeline();

	gst.play();
	gstAudioIn.play();

	GstClock * clock = gst_pipeline_get_clock(GST_PIPELINE(gst.getPipeline()));
	gst_object_ref(clock);
	GstClockTime now = gst_clock_get_time (clock) - gst_element_get_base_time(gst.getPipeline());
	gst_object_unref (clock);
	prevTimestamp = now;
	prevTimestampDepth = now;
	prevTimestampOsc = now;

}

bool ofxGstRTPServer::on_message(GstMessage * msg){
	// read messages from the pipeline like dropped packages
	switch (GST_MESSAGE_TYPE (msg)) {
	case GST_MESSAGE_ELEMENT:{
		GstObject * messageSrc = GST_MESSAGE_SRC(msg);
		ofLogVerbose(LOG_NAME) << "Got " << GST_MESSAGE_TYPE_NAME(msg) << " message from " << GST_MESSAGE_SRC_NAME(msg);
		ofLogVerbose(LOG_NAME) << "Message source type: " << G_OBJECT_CLASS_NAME(G_OBJECT_GET_CLASS(messageSrc));
		return true;
	}
	case GST_MESSAGE_QOS:{
		GstObject * messageSrc = GST_MESSAGE_SRC(msg);
		ofLogVerbose(LOG_NAME) << "Got " << GST_MESSAGE_TYPE_NAME(msg) << " message from " << GST_MESSAGE_SRC_NAME(msg);
		ofLogVerbose(LOG_NAME) << "Message source type: " << G_OBJECT_CLASS_NAME(G_OBJECT_GET_CLASS(messageSrc));

		GstFormat format;
		guint64 processed;
		guint64 dropped;
		gst_message_parse_qos_stats(msg,&format,&processed,&dropped);
		ofLogVerbose(LOG_NAME) << "format " << gst_format_get_name(format) << " processed " << processed << " dropped " << dropped;

		gint64 jitter;
		gdouble proportion;
		gint quality;
		gst_message_parse_qos_values(msg,&jitter,&proportion,&quality);
		ofLogVerbose(LOG_NAME) << "jitter " << jitter << " proportion " << proportion << " quality " << quality;

		gboolean live;
		guint64 running_time;
		guint64 stream_time;
		guint64 timestamp;
		guint64 duration;
		gst_message_parse_qos(msg,&live,&running_time,&stream_time,&timestamp,&duration);
		ofLogVerbose(LOG_NAME) << "live stream " << live << " runninng_time " << running_time << " stream_time " << stream_time << " timestamp " << timestamp << " duration " << duration;

		return true;
	}
	default:
		ofLogVerbose(LOG_NAME) << "Got " << GST_MESSAGE_TYPE_NAME(msg) << " message from " << GST_MESSAGE_SRC_NAME(msg);
		return false;
	}
}


void ofxGstRTPServer::newFrame(ofPixels & pixels){
	// here we push new video frames in the pipeline, it's important
	// to timestamp them properly so gstreamer can sync them with the
	// audio.

	if(!bufferPool || !appSrcVideoRGB) return;

	// get current time from the pipeline
	GstClock * clock = gst_pipeline_get_clock(GST_PIPELINE(gst.getPipeline()));
	gst_object_ref(clock);
	GstClockTime now = gst_clock_get_time (clock) - gst_element_get_base_time(gst.getPipeline());
	gst_object_unref (clock);

	if(firstVideoFrame){
		prevTimestamp = now;
		firstVideoFrame = false;
		return;
	}

	// get a pixels buffer from the pool and copy the passed frame into it
	PooledPixels<unsigned char> * pooledPixels = bufferPool->newBuffer();
	*(ofPixels*)pooledPixels=pixels;

	// wrap the pooled pixels into a gstreamer buffer and pass the release
	// callback so when it's not needed anymore by gst we can return it to the pool
	GstBuffer * buffer;
	buffer = gst_buffer_new_wrapped_full (GST_MEMORY_FLAG_READONLY,pooledPixels->getPixels(), pooledPixels->size(), 0, pooledPixels->size(), pooledPixels, (GDestroyNotify)&ofxGstBufferPool<unsigned char>::relaseBuffer);

	// timestamp the buffer, right now we are using:
	// timestamp = current pipeline time - base time
	// duration = timestamp - previousTimeStamp
	// the duration is actually the duration of the previous frame
	// but should be accurate enough
	GST_BUFFER_OFFSET(buffer) = numFrame++;
	GST_BUFFER_OFFSET_END(buffer) = numFrame;
	GST_BUFFER_DTS (buffer) = now;
	GST_BUFFER_PTS (buffer) = now;
	GST_BUFFER_DURATION(buffer) = now-prevTimestamp;
	prevTimestamp = now;

	// add video format metadata to the buffer
	/*const GstVideoFormatInfo *finfo = gst_video_format_get_info(GST_VIDEO_FORMAT_RGB);
	gsize offset[GST_VIDEO_MAX_PLANES];
	gint stride[GST_VIDEO_MAX_PLANES];

	int n_planes = 3;
	int offs = 0;
	for (int i = 0; i < n_planes; i++) {
	  offset[i] = offs;
	  stride[i] = width*3;

	  offs += stride[i] * GST_VIDEO_FORMAT_INFO_SCALE_HEIGHT (finfo, i, height);
	}
	gst_buffer_add_video_meta_full (buffer, GST_VIDEO_FRAME_FLAG_NONE,
			GST_VIDEO_FORMAT_RGB, width, height, 3,
			offset, stride);*/

	// finally push the buffer into the pipeline through the appsrc element
	GstFlowReturn flow_return = gst_app_src_push_buffer((GstAppSrc*)appSrcVideoRGB, buffer);
	if (flow_return != GST_FLOW_OK) {
		ofLogError() << "error pushing video buffer: flow_return was " << flow_return;
	}
}


void ofxGstRTPServer::newFrameDepth(ofPixels & pixels){
	//unsigned long long time = ofGetElapsedTimeMicros();

	// here we push new depth frames in the pipeline, it's important
	// to timestamp them properly so gstreamer can sync them with the
	// audio.

	if(!bufferPoolDepth || !appSrcDepth) return;

	// get current time from the pipeline
	GstClock * clock = gst_pipeline_get_clock(GST_PIPELINE(gst.getPipeline()));
	gst_object_ref(clock);
	GstClockTime now = gst_clock_get_time (clock) - gst_element_get_base_time(gst.getPipeline());
	gst_object_unref (clock);

	if(firstDepthFrame){
		prevTimestampDepth = now;
		firstDepthFrame = false;
		return;
	}

	// get a pixels buffer from the pool and copy the passed frame into it
	PooledPixels<unsigned char> * pooledPixels = bufferPoolDepth->newBuffer();
	//pooledPixels->swap(pixels);
	*(ofPixels*)pooledPixels=pixels;

	// wrap the pooled pixels into a gstreamer buffer and pass the release
	// callback so when it's not needed anymore by gst we can return it to the pool
	GstBuffer * buffer;
	buffer = gst_buffer_new_wrapped_full (GST_MEMORY_FLAG_READONLY,pooledPixels->getPixels(), pooledPixels->size(), 0, pooledPixels->size(), pooledPixels, (GDestroyNotify)&ofxGstBufferPool<unsigned char>::relaseBuffer);

	// timestamp the buffer, right now we are using:
	// timestamp = current pipeline time - base time
	// duration = timestamp - previousTimeStamp
	// the duration is actually the duration of the previous frame
	// but should be accurate enough
	GST_BUFFER_OFFSET(buffer) = numFrameDepth++;
	GST_BUFFER_OFFSET_END(buffer) = numFrameDepth;
	GST_BUFFER_DTS (buffer) = now;
	GST_BUFFER_PTS (buffer) = now;
	GST_BUFFER_DURATION(buffer) = now-prevTimestampDepth;
	prevTimestampDepth = now;

	// add video format metadata to the buffer
	/*const GstVideoFormatInfo *finfo = gst_video_format_get_info(GST_VIDEO_FORMAT_GRAY8);
	gsize offset[GST_VIDEO_MAX_PLANES];
	gint stride[GST_VIDEO_MAX_PLANES];

	int n_planes = 1;
	int offs = 0;
	for (int i = 0; i < n_planes; i++) {
	  offset[i] = offs;
	  stride[i] = width;

	  offs += stride[i] * GST_VIDEO_FORMAT_INFO_SCALE_HEIGHT (finfo, i, height);
	}
	gst_buffer_add_video_meta_full (buffer, GST_VIDEO_FRAME_FLAG_NONE,
			GST_VIDEO_FORMAT_GRAY8, width, height, 1,
			offset, stride);*/

	// finally push the buffer into the pipeline through the appsrc element
	GstFlowReturn flow_return = gst_app_src_push_buffer((GstAppSrc*)appSrcDepth, buffer);
	if (flow_return != GST_FLOW_OK) {
		ofLogError() << "error pushing depth buffer: flow_return was " << flow_return;
	}

	//cout << ofGetElapsedTimeMicros() - time << endl;
}


void ofxGstRTPServer::newFrameDepth(ofShortPixels & pixels){
	//unsigned long long time = ofGetElapsedTimeMicros();

	// here we push new depth frames in the pipeline, it's important
	// to timestamp them properly so gstreamer can sync them with the
	// audio.

	if(!bufferPoolDepth || !appSrcDepth) return;

	// get current time from the pipeline
	GstClock * clock = gst_pipeline_get_clock(GST_PIPELINE(gst.getPipeline()));
	gst_object_ref(clock);
	GstClockTime now = gst_clock_get_time (clock) - gst_element_get_base_time(gst.getPipeline());
	gst_object_unref (clock);

	if(firstDepthFrame){
		prevTimestampDepth = now;
		firstDepthFrame = false;
		return;
	}

	// get a pixels buffer from the pool and copy the passed frame into it
	PooledPixels<unsigned char> * pooledPixels = bufferPoolDepth->newBuffer();
	ofxGstRTPUtils::convertShortToColoredDepth(pixels,*pooledPixels,pow(2.f,14.f));

	// wrap the pooled pixels into a gstreamer buffer and pass the release
	// callback so when it's not needed anymore by gst we can return it to the pool
	GstBuffer * buffer;
	buffer = gst_buffer_new_wrapped_full (GST_MEMORY_FLAG_READONLY,pooledPixels->getPixels(), pooledPixels->size(), 0, pooledPixels->size(), pooledPixels, (GDestroyNotify)&ofxGstBufferPool<unsigned char>::relaseBuffer);

	// timestamp the buffer, right now we are using:
	// timestamp = current pipeline time - base time
	// duration = timestamp - previousTimeStamp
	// the duration is actually the duration of the previous frame
	// but should be accurate enough
	GST_BUFFER_OFFSET(buffer) = numFrameDepth++;
	GST_BUFFER_OFFSET_END(buffer) = numFrameDepth;
	GST_BUFFER_DTS (buffer) = now;
	GST_BUFFER_PTS (buffer) = now;
	GST_BUFFER_DURATION(buffer) = now-prevTimestampDepth;
	prevTimestampDepth = now;

	// add video format metadata to the buffer
	/*const GstVideoFormatInfo *finfo = gst_video_format_get_info(GST_VIDEO_FORMAT_RGB);
	gsize offset[GST_VIDEO_MAX_PLANES];
	gint stride[GST_VIDEO_MAX_PLANES];

	int n_planes = 3;
	int offs = 0;
	for (int i = 0; i < n_planes; i++) {
	  offset[i] = offs;
	  stride[i] = width*3;

	  offs += stride[i] * GST_VIDEO_FORMAT_INFO_SCALE_HEIGHT (finfo, i, height);
	}
	gst_buffer_add_video_meta_full (buffer, GST_VIDEO_FRAME_FLAG_NONE,
			GST_VIDEO_FORMAT_RGB, width, height, 1,
			offset, stride);*/

	// finally push the buffer into the pipeline through the appsrc element
	GstFlowReturn flow_return = gst_app_src_push_buffer((GstAppSrc*)appSrcDepth, buffer);
	if (flow_return != GST_FLOW_OK) {
		ofLogError() << "error pushing depth buffer: flow_return was " << flow_return;
	}

	//cout << ofGetElapsedTimeMicros() - time << endl;
}


void ofxGstRTPServer::newOscMsg(ofxOscMessage & msg){
	if(!appSrcOsc) return;

	// get current time from the pipeline
	GstClock * clock = gst_pipeline_get_clock(GST_PIPELINE(gst.getPipeline()));
	gst_object_ref(clock);
	GstClockTime now = gst_clock_get_time (clock) - gst_element_get_base_time(gst.getPipeline());
	gst_object_unref (clock);

	if(firstOscFrame){
		prevTimestampOsc = now;
		firstOscFrame = false;
		return;
	}

	PooledOscPacket * pooledOscPkg = oscPacketPool.newBuffer();
	appendMessage(msg,pooledOscPkg->packet);

	GstBuffer * buffer = gst_buffer_new_wrapped_full(GST_MEMORY_FLAG_READONLY,(void*)pooledOscPkg->compressedData(),pooledOscPkg->compressedSize(),0,pooledOscPkg->compressedSize(),pooledOscPkg,(GDestroyNotify)&ofxOscPacketPool::relaseBuffer);

	GST_BUFFER_OFFSET(buffer) = numFrameOsc++;
	GST_BUFFER_OFFSET_END(buffer) = numFrameOsc;
	GST_BUFFER_DTS (buffer) = now;
	GST_BUFFER_PTS (buffer) = now;
	GST_BUFFER_DURATION(buffer) = now-prevTimestampOsc;
	prevTimestampOsc = now;

	GstFlowReturn flow_return = gst_app_src_push_buffer((GstAppSrc*)appSrcOsc, buffer);
	if (flow_return != GST_FLOW_OK) {
		ofLogError() << "error pushing osc buffer: flow_return was " << flow_return;
	}
}


void ofxGstRTPServer::appendMessage( ofxOscMessage& message, osc::OutboundPacketStream& p )
{
    p << osc::BeginMessage( message.getAddress().c_str() );
	for ( int i=0; i< message.getNumArgs(); ++i )
	{
		if ( message.getArgType(i) == OFXOSC_TYPE_INT32 )
			p << message.getArgAsInt32( i );
		else if ( message.getArgType(i) == OFXOSC_TYPE_INT64 )
			p << (osc::int64)message.getArgAsInt64( i );
		else if ( message.getArgType( i ) == OFXOSC_TYPE_FLOAT )
			p << message.getArgAsFloat( i );
		else if ( message.getArgType( i ) == OFXOSC_TYPE_STRING )
			p << message.getArgAsString( i ).c_str();
		else
		{
			ofLogError("ofxOscSender") << "appendMessage(): bad argument type " << message.getArgType( i );
		}
		//cout << i << ": " << p.Size() << endl;
	}
	p << osc::EndMessage;
}

void ofxGstRTPServer::on_eos_from_audio(GstAppSink * elt, void * rtpClient){

}


GstFlowReturn ofxGstRTPServer::on_new_preroll_from_audio(GstAppSink * elt, void * rtpClient){
	return GST_FLOW_OK;
}

void ofxGstRTPServer::sendAudioOut(PooledAudioFrame * pooledFrame){
	GstClock * clock = gst_pipeline_get_clock(GST_PIPELINE(gst.getPipeline()));
	gst_object_ref(clock);
	GstClockTime now = gst_clock_get_time (clock) - gst_element_get_base_time(gst.getPipeline());
	gst_object_unref (clock);

	if(firstAudioFrame){
		prevTimestampAudio = now;
		firstAudioFrame = false;
		return;
	}

	//cout << "server sending " << frame._payloadDataLengthInSamples << " samples at " << (int)(now*0.000001) << "ms" << endl;
	int size = pooledFrame->audioFrame._payloadDataLengthInSamples*2*pooledFrame->audioFrame._audioChannel;
	/*GstBuffer * echoCancelledBuffer = gst_buffer_new_allocate(NULL,size,NULL);
	gst_buffer_fill(echoCancelledBuffer,0,frame._payloadData,size);*/

	GstBuffer * echoCancelledBuffer = gst_buffer_new_wrapped_full(GST_MEMORY_FLAG_READONLY,(void*)pooledFrame->audioFrame._payloadData,size,0,size,pooledFrame,(GDestroyNotify)&ofxWebRTCAudioPool::relaseFrame);

	GST_BUFFER_OFFSET(echoCancelledBuffer) = numFrameAudio++;
	GST_BUFFER_OFFSET_END(echoCancelledBuffer) = numFrameAudio;
	GST_BUFFER_DTS (echoCancelledBuffer) = now;
	GST_BUFFER_PTS (echoCancelledBuffer) = now;
	GST_BUFFER_DURATION(echoCancelledBuffer) = now-prevTimestampAudio;
	prevTimestampAudio = now;


	GstFlowReturn flow_return = gst_app_src_push_buffer((GstAppSrc*)appSrcAudio, echoCancelledBuffer);
	if (flow_return != GST_FLOW_OK) {
		ofLogError(LOG_NAME) << "error pushing audio buffer: flow_return was " << flow_return;
	}
}

GstFlowReturn ofxGstRTPServer::on_new_buffer_from_audio(GstAppSink * elt, void * data){
	static int posInBuffer=0;
	ofxGstRTPServer * server = (ofxGstRTPServer *)data;
	if(server->echoCancel){
		GstSample * sample = gst_app_sink_pull_sample(elt);
		GstBuffer * buffer = gst_sample_get_buffer(sample);


		/*GstClock * clock = gst_pipeline_get_clock(GST_PIPELINE(server->gstAudioIn.getPipeline()));
		gst_object_ref(clock);
		GstClockTime now = gst_clock_get_time (clock) - gst_element_get_base_time(server->gstAudioIn.getPipeline());
		gst_object_unref (clock);
		u_int64_t t_capture = GST_BUFFER_TIMESTAMP(buffer)*0.000001;
		u_int64_t t_process = now*0.000001;
		int delay = (t_process - t_capture) + server->client->getAudioOutLatencyMs();*/
		int delay = server->gstAudioIn.getMinLatencyNanos()*0.000001 + server->client->getAudioOutLatencyMs();
		//delay = 500;

		const int numChannels = 1;
		const int samplerate = 32000;
		int buffersize = gst_buffer_get_size(buffer)/2/numChannels;
		const int samplesIn10Ms = samplerate/100;

		if(server->prevAudioBuffer){
			PooledAudioFrame * audioFrame = server->audioPool.newFrame();
			gst_buffer_map (server->prevAudioBuffer, &server->mapinfo, GST_MAP_READ);
			int prevBuffersize = gst_buffer_get_size(server->prevAudioBuffer)/2/numChannels;
			memcpy(audioFrame->audioFrame._payloadData,((short*)server->mapinfo.data)+(posInBuffer*numChannels),(prevBuffersize-posInBuffer)*numChannels*sizeof(short));
			//cout << "copying " << prevBuffersize-posInBuffer;
			gst_buffer_unmap(server->prevAudioBuffer,&server->mapinfo);
			gst_buffer_unref(server->prevAudioBuffer);

			gst_buffer_map (buffer, &server->mapinfo, GST_MAP_READ);
			memcpy(audioFrame->audioFrame._payloadData+((prevBuffersize-posInBuffer)*numChannels),((short*)server->mapinfo.data),(samplesIn10Ms-(prevBuffersize-posInBuffer))*numChannels*sizeof(short));

			audioFrame->audioFrame._payloadDataLengthInSamples = samplesIn10Ms;
			audioFrame->audioFrame._audioChannel = numChannels;
			audioFrame->audioFrame._frequencyInHz = samplerate;
			//cout << " + " << samplesIn10Ms-(prevBuffersize-posInBuffer) << endl;

			//server->audioFrame.UpdateFrame(0,GST_BUFFER_TIMESTAMP(buffer),auxBuffer,samplesIn10Ms,samplerate,webrtc::AudioFrame::kNormalSpeech,webrtc::AudioFrame::kVadActive,numChannels,0xffffffff,0xffffffff);

			/*GstClock * clock = gst_pipeline_get_clock(GST_PIPELINE(server->gstAudioIn.getPipeline()));
			gst_object_ref(clock);
			GstClockTime now = gst_clock_get_time (clock) - gst_element_get_base_time(server->gstAudioIn.getPipeline());
			gst_object_unref (clock);
			u_int64_t t_capture = GST_BUFFER_TIMESTAMP(buffer)*0.000001;
			u_int64_t t_process = now*0.000001;
			int delay = (t_process - t_capture) + server->client->getAudioOutLatencyMs();*/

			server->echoCancel->getAudioProcessing()->set_stream_delay_ms(delay);
			server->echoCancel->getAudioProcessing()->echo_cancellation()->set_stream_drift_samples((int64_t)server->client->getAudioFramesProcessed()-(int64_t)server->audioFramesProcessed);
			//cout << "process stream returned ";
			server->echoCancel->getAudioProcessing()->gain_control()->set_stream_analog_level(server->analogAudio);
			server->echoCancel->process(audioFrame->audioFrame);// << endl;
			if(!server->echoCancel->getAudioProcessing()->voice_detection()->stream_has_voice()){
				memset(audioFrame->audioFrame._payloadData,0,samplesIn10Ms*numChannels*sizeof(short));
			}
			server->analogAudio = server->echoCancel->getAudioProcessing()->gain_control()->stream_analog_level();
			g_object_set(server->audiocapture,"volume",server->analogAudio/double(0x10000U),NULL);
			//cout << server->analogAudio/double(0x10000U) << endl;
			server->sendAudioOut(audioFrame);
			posInBuffer = samplesIn10Ms-(prevBuffersize-posInBuffer);
			server->audioFramesProcessed += samplesIn10Ms;
		}else{
			gst_buffer_map (buffer, &server->mapinfo, GST_MAP_READ);
			posInBuffer = 0;
		}

		while(posInBuffer+samplesIn10Ms<=buffersize){
			PooledAudioFrame * audioFrame = server->audioPool.newFrame();
			audioFrame->audioFrame.UpdateFrame(0,GST_BUFFER_TIMESTAMP(buffer),((short*)server->mapinfo.data)  + (posInBuffer*numChannels),samplesIn10Ms,samplerate,webrtc::AudioFrame::kNormalSpeech,webrtc::AudioFrame::kVadActive,numChannels,0xffffffff,0xffffffff);

			/*GstClock * clock = gst_pipeline_get_clock(GST_PIPELINE(server->gstAudioIn.getPipeline()));
			gst_object_ref(clock);
			GstClockTime now = gst_clock_get_time (clock) - gst_element_get_base_time(server->gstAudioIn.getPipeline());
			gst_object_unref (clock);
			u_int64_t t_capture = GST_BUFFER_TIMESTAMP(buffer)*0.000001;
			u_int64_t t_process = now*0.000001;
			int delay = (t_process - t_capture) + server->client->getAudioOutLatencyMs();*/

			server->echoCancel->getAudioProcessing()->set_stream_delay_ms(delay);
			server->echoCancel->getAudioProcessing()->echo_cancellation()->set_stream_drift_samples((int64_t)server->client->getAudioFramesProcessed()-(int64_t)server->audioFramesProcessed);
			server->echoCancel->getAudioProcessing()->gain_control()->set_stream_analog_level(server->analogAudio);
			//cout << "process stream returned ";
			server->echoCancel->process(audioFrame->audioFrame);// << endl;
			if(!server->echoCancel->getAudioProcessing()->voice_detection()->stream_has_voice()){
				memset(audioFrame->audioFrame._payloadData,0,samplesIn10Ms*numChannels*sizeof(short));
			}
			server->analogAudio = server->echoCancel->getAudioProcessing()->gain_control()->stream_analog_level();
			g_object_set(server->audiocapture,"volume",server->analogAudio/double(0x10000U),NULL);
			//cout << server->analogAudio/double(0x10000U) << endl;
			server->sendAudioOut(audioFrame);
			posInBuffer+=samplesIn10Ms;
			server->audioFramesProcessed += samplesIn10Ms;
		};

		if(posInBuffer<buffersize){
			//cout << "server remaining samples: " << buffersize - posInBuffer << endl;
			server->prevAudioBuffer = buffer;
			gst_buffer_ref(server->prevAudioBuffer);
		}else{
			server->prevAudioBuffer = 0;
		}

		/*if(server->audioProcessing->echo_cancellation()->stream_has_echo()){
			cout << "echo? " << server->audioProcessing->echo_cancellation()->stream_has_echo() << " with (" << server->gstAudioIn.getMinLatencyNanos() << ") +" << server->client->getAudioOutLatencyMs() << " = " << delay << endl;
			cout << "audioin latency " << server->gstAudioIn.getMinLatencyNanos() << endl;
		}*/

		gst_buffer_unmap(buffer,&server->mapinfo);
		gst_sample_unref(sample);
	}
	return GST_FLOW_OK;
}

