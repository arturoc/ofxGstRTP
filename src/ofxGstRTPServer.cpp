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

#if ENABLE_NAT_TRANSVERSAL
#include <agent.h>
#endif

#include "ofxGstPixelsPool.h"
#include "ofxGstRTPUtils.h"

#include "ofxGstRTPClient.h"

#ifdef TARGET_WIN32
    typedef UINT uint;
#endif // TARGET_WIN32


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
,videoSessionNumber(-1)
,audioSessionNumber(-1)
,depthSessionNumber(-1)
,oscSessionNumber(-1)
,videoSSRC(0)
,audioSSRC(0)
,depthSSRC(0)
,oscSSRC(0)
,sendVideoKeyFrame(true)
,sendDepthKeyFrame(true)
#if ENABLE_NAT_TRANSVERSAL
,videoStream(NULL)
,depthStream(NULL)
,oscStream(NULL)
,audioStream(NULL)
#endif
,firstVideoFrame(true)
,firstOscFrame(true)
,firstDepthFrame(true)
,firstAudioFrame(true)
#if ENABLE_ECHO_CANCEL
,audioChannelReady(false)
,echoCancel(0)
,prevAudioBuffer(NULL)
,audioFramesProcessed(0)
,analogAudio(0x10000U)
#endif
{
	videoBitrate.set("video bitrate (kbps)",300,0,6000);
	videoBitrate.addListener(this,&ofxGstRTPServer::vBitRateChanged);
	depthBitrate.set("depth bitrate (kbps)",1024,0,6000);
	depthBitrate.addListener(this,&ofxGstRTPServer::dBitRateChanged);
	audioBitrate.set("audio bitrate (bps)",64000,4000,650000);
	audioBitrate.addListener(this,&ofxGstRTPServer::aBitRateChanged);
	reverseDriftCalculation.set("reverse drift calc.",false);
	parameters.setName("gst rtp server");

#if ENABLE_ECHO_CANCEL
	GstMapInfo mapinfo = {0,};
	this->mapinfo=mapinfo;
#endif
}

ofxGstRTPServer::~ofxGstRTPServer() {
}


void ofxGstRTPServer::addVideoChannel(int port, int w, int h, int fps){
	videoSessionNumber = lastSessionNumber;
	lastSessionNumber++;
	// video elements
	// ------------------
		// appsrc, allows to pass new frames from the app using the newFrame method
		string velem="appsrc is-live=1 do-timestamp=1 format=time name=appsrcvideo";

		// video format that we are pushing to the pipeline
		string vcaps="video/x-raw,format=RGB,width="+ofToString(w)+ ",height="+ofToString(h)+",framerate="+ofToString(fps)+"/1";

		// queue so the conversion and encoding happen in a different thread to appsrc
		string vsource= velem + " ! " + vcaps + " ! videoconvert name=vconvert1 ";

		// h264 encoder + rtp pay
		// x264 settings from http://stackoverflow.com/questions/12221569/x264-rate-control
		string venc="x264enc tune=zerolatency byte-stream=true bitrate=" + ofToString(videoBitrate) +" speed-preset=ultrafast name=vencoder ! video/x-h264,width="+ofToString(w)+ ",height="+ofToString(h)+",framerate="+ofToString(fps)+"/1 ! rtph264pay pt=96 ! application/x-rtp,media=(string)video,clock-rate=(int)90000,payload=(int)96,encoding-name=(string)H264,rtcp-fb-nack-pli=(int)1 "; //psy-tune=grain me=4 subme=10 b-adapt=1 vbv-buf-capacity=1000

	// video rtpc
	// ------------------
		string vrtpsink;
		string vrtpcsink;
		string vrtpcsrc;

#if ENABLE_NAT_TRANSVERSAL
		if(videoStream){
			vrtpsink="nicesink ts-offset=0 name=vrtpsink max-lateness=5000000000 ";
			vrtpcsink="nicesink  sync=false async=false name=vrtcpsink max-lateness=5000000000 ";
			vrtpcsrc="nicesrc name=vrtcpsrc";
		}else
#endif
		{
			vrtpsink="udpsink port=" + ofToString(port) + " host="+dest+" ts-offset=0 force-ipv4=1 name=vrtpsink";
			vrtpcsink="udpsink port=" + ofToString(port+1) + " host="+dest+" sync=false async=false force-ipv4=1 name=vrtcpsink";
			vrtpcsrc="udpsrc port=" + ofToString(port+3) + " name=vrtcpsrc";
		}

	pipelineStr += " " + vsource + " ! " + venc + " ! rtpbin.send_rtp_sink_" + ofToString(videoSessionNumber) +
				" rtpbin.send_rtp_src_" + ofToString(videoSessionNumber) + " ! " + vrtpsink +
				" rtpbin.send_rtcp_src_" + ofToString(videoSessionNumber) + " ! " + vrtpcsink +
				" " + vrtpcsrc + " ! rtpbin.recv_rtcp_sink_" + ofToString(videoSessionNumber) + " ";

	// create a pixels pool of the correct w,h and bpp to use on newFrame
	bufferPool = new ofxGstBufferPool<unsigned char>(w,h,3);
	parameters.add(videoBitrate);
}


void ofxGstRTPServer::addAudioChannel(int port){
	audioSessionNumber = lastSessionNumber;
	lastSessionNumber++;

	// audio elements
	//-------------------
		// audio source
		string aelem;
#if ENABLE_ECHO_CANCEL
		if(echoCancel){
			parameters.add(reverseDriftCalculation);
			aelem = "appsrc is-live=1 do-timestamp=1 format=time name=audioechosrc ! audio/x-raw,format=S16LE,rate=32000,channels=1 ";
		}else
#endif
		{
		#ifdef TARGET_LINUX
			aelem = "pulsesrc stream-properties=\"props,media.role=phone,filter.want=echo-cancel\" name=audiocapture ";

		#elif defined(TARGET_OSX)
			// for osx we specify the output format since osxaudiosrc doesn't report the formats supported by the hw
			// FIXME: we should detect the format somehow and set it automatically
			aelem = "osxaudiosrc name=audiocapture ! audio/x-raw,rate=44100,channels=1 ";
		#elif defined(TARGET_WIN32)
			aelem = "autoaudiosrc name=audiocapture ! audio/x-raw,rate=44100,channels=1 ";
		#endif
		}

		// audio source + queue for threading + audio resample and convert
		// to change sampling rate and format to something supported by the encoder
		string asource = aelem + " ! audioresample ! audioconvert";

		// opus encoder + opus pay
		// FIXME: audio=0 is voice??
		string aenc = "opusenc name=aencoder audio=0 ! rtpopuspay pt=98";

	// audio rtpc
		string artpsink;
		string artpcsink;
		string artpcsrc;

#if ENABLE_NAT_TRANSVERSAL
		if(audioStream){
			artpsink="nicesink ts-offset=0 name=artpsink max-lateness=5000000000 ";
			artpcsink="nicesink sync=false async=false name=artcpsink max-lateness=5000000000 ";
			artpcsrc="nicesrc name=artcpsrc";
		}else
#endif
		{
			artpsink="udpsink port=" + ofToString(port) + " host="+dest+" ts-offset=0 force-ipv4=1 name=artpsink";
			artpcsink="udpsink port=" + ofToString(port+1) + " host="+dest+" sync=false async=false force-ipv4=1 name=artcpsink";
			artpcsrc="udpsrc port=" + ofToString(port+3) + " name=artcpsrc";
		}

		// audio
	pipelineStr += " " +  asource + " ! " + aenc + " ! rtpbin.send_rtp_sink_" + ofToString(audioSessionNumber) +
			" rtpbin.send_rtp_src_" + ofToString(audioSessionNumber) + " ! " + artpsink +
			" rtpbin.send_rtcp_src_" + ofToString(audioSessionNumber) + " ! " + artpcsink +
			" " + artpcsrc + " ! rtpbin.recv_rtcp_sink_" + ofToString(audioSessionNumber) + " ";

#if ENABLE_ECHO_CANCEL
	audioChannelReady = true;
#endif
	parameters.add(audioBitrate);
}

void ofxGstRTPServer::addDepthChannel(int port, int w, int h, int fps, bool depth16){
	depthSessionNumber = lastSessionNumber;
	lastSessionNumber++;

	// depth elements
	// ------------------
		// appsrc, allows to pass new frames from the app using the newFrame method
		string delem="appsrc is-live=1 do-timestamp=1 format=time name=appsrcdepth";

		// video format that we are pushing to the pipeline
		string dcaps;
		if(depth16){
			dcaps="video/x-raw,format=RGB,width="+ofToString(w)+ ",height="+ofToString(h)+",framerate="+ofToString(fps)+"/1";
		}else{
			dcaps="video/x-raw,format=GRAY8,width="+ofToString(w)+ ",height="+ofToString(h)+",framerate="+ofToString(fps)+"/1";
		}

		// queue so the conversion and encoding happen in a different thread to appsrc
		string dsource= delem + " ! " + dcaps + " ! videoconvert name=dconvert1";

		// h264 encoder + rtp pay
		string denc="x264enc tune=zerolatency byte-stream=true bitrate="+ofToString(depthBitrate)+" speed-preset=superfast psy-tune=psnr me=4 subme=10 b-adapt=0 vbv-buf-capacity=600 name=dencoder ! video/x-h264,width="+ofToString(w)+ ",height="+ofToString(h)+",framerate="+ofToString(fps)+"/1 ! rtph264pay pt=97 ! application/x-rtp,media=(string)video,clock-rate=(int)90000,payload=(int)97,encoding-name=(string)H264,rtcp-fb-nack-pli=(int)1 ";

	// depth rtpc
	// ------------------
		string drtpsink;
		string drtpcsink;
		string drtpcsrc;

#if ENABLE_NAT_TRANSVERSAL
		if(depthStream){
			drtpsink="nicesink ts-offset=0 name=drtpsink max-lateness=5000000000 ";
			drtpcsink="nicesink sync=false async=false name=drtcpsink max-lateness=5000000000 ";
			drtpcsrc="nicesrc name=drtcpsrc";
		}else
#endif
		{
			drtpsink="udpsink port=" + ofToString(port) + " host="+dest+" ts-offset=0 force-ipv4=1 name=drtpsink";
			drtpcsink="udpsink port=" + ofToString(port+1) + " host="+dest+" sync=false async=false force-ipv4=1 name=drtcpsink";
			drtpcsrc="udpsrc port=" + ofToString(port+3) + " name=drtcpsrc";
		}

	// depth
	pipelineStr += " " +  dsource + " ! " + denc + " ! rtpbin.send_rtp_sink_" + ofToString(depthSessionNumber) +
		" rtpbin.send_rtp_src_" + ofToString(depthSessionNumber) + " ! " + drtpsink +
		" rtpbin.send_rtcp_src_" + ofToString(depthSessionNumber) + " ! " + drtpcsink +
		" " + drtpcsrc + " ! rtpbin.recv_rtcp_sink_" + ofToString(depthSessionNumber) + " ";

	if(depth16){
		bufferPoolDepth = new ofxGstBufferPool<unsigned char>(w,h,3);
	}else{
		bufferPoolDepth = new ofxGstBufferPool<unsigned char>(w,h,1);
	}
	parameters.add(depthBitrate);
}

void ofxGstRTPServer::addOscChannel(int port){
	oscSessionNumber = lastSessionNumber;
	lastSessionNumber++;

	// osc elements
	// ------------------
		// appsrc, allows to pass new frames from the app using the newFrame method
		string oelem="appsrc is-live=1 format=time do-timestamp=1 name=appsrcosc ! application/x-osc ";

		// queue so the conversion and encoding happen in a different thread to appsrc
		string osource = oelem;

		// rtp pay
		string oenc=" rtpgstpay pt=99";

	// osc rtpc
	// ------------------
		string ortpsink;
		string ortpcsink;
		string ortpcsrc;

#if ENABLE_NAT_TRANSVERSAL
		if(oscStream){
			ortpsink="nicesink ts-offset=0 name=ortpsink max-lateness=5000000000 ";
			ortpcsink="nicesink sync=false async=false name=ortcpsink max-lateness=5000000000 ";
			ortpcsrc="nicesrc name=ortcpsrc";
		}else
#endif
		{
			ortpsink="udpsink port=" + ofToString(port) + " host="+dest+" ts-offset=0 force-ipv4=1 name=ortpsink";
			ortpcsink="udpsink port=" + ofToString(port+1) + " host="+dest+" sync=false async=false force-ipv4=1 name=ortcpsink";
			ortpcsrc="udpsrc port=" + ofToString(port+3) + " name=ortcpsrc";

		}

	// osc
	pipelineStr += " " + osource + " ! " + oenc + " ! rtpbin.send_rtp_sink_" + ofToString(oscSessionNumber) +
		" rtpbin.send_rtp_src_" + ofToString(oscSessionNumber) + " ! " + ortpsink +
		" rtpbin.send_rtcp_src_" + ofToString(oscSessionNumber) + " ! " + ortpcsink +
		" " + ortpcsrc + " ! rtpbin.recv_rtcp_sink_" + ofToString(oscSessionNumber) + " ";
}

#if ENABLE_NAT_TRANSVERSAL
void ofxGstRTPServer::addVideoChannel(ofxNiceStream * niceStream, int w, int h, int fps){
	videoStream = niceStream;
	addVideoChannel(0,w,h,fps);
}

void ofxGstRTPServer::addAudioChannel(ofxNiceStream * niceStream){
	audioStream = niceStream;
	addAudioChannel(0);
}

void ofxGstRTPServer::addDepthChannel(ofxNiceStream * niceStream, int w, int h, int fps, bool depth16){
	depthStream = niceStream;
	addDepthChannel(0,w,h,fps,depth16);
}

void ofxGstRTPServer::addOscChannel(ofxNiceStream * niceStream){
	oscStream = niceStream;
	addOscChannel(0);
}

void ofxGstRTPServer::setup(){
	setup("");
}
#endif

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

#if ENABLE_ECHO_CANCEL
void ofxGstRTPServer::setRTPClient(ofxGstRTPClient & client){
	this->client = &client;
}

void ofxGstRTPServer::setEchoCancel(ofxEchoCancel & echoCancel){
	if(audioChannelReady){
		ofLogError() << "trying to add echo cancel module after setting audio channel";
	}else{
		this->echoCancel = &echoCancel;
	}
}
#endif

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
#if ENABLE_NAT_TRANSVERSAL
	videoStream = NULL;
	depthStream = NULL;
	oscStream = NULL;
	audioStream = NULL;
#endif
	firstVideoFrame = true;
	firstOscFrame = true;
	firstDepthFrame = true;
}


void ofxGstRTPServer::vBitRateChanged(int & bitrate){
	g_object_set(G_OBJECT(vEncoder),"bitrate",bitrate,NULL);
}

void ofxGstRTPServer::dBitRateChanged(int & bitrate){
	g_object_set(G_OBJECT(dEncoder),"bitrate",bitrate,NULL);
}

void ofxGstRTPServer::aBitRateChanged(int & bitrate){
	g_object_set(G_OBJECT(aEncoder),"bitrate",bitrate,NULL);
}

void ofxGstRTPServer::play(){
	// pass the pipeline to the gstUtils so it starts everything
	gst.setPipelineWithSink(pipelineStr,"",true);

	// get the rtp and rtpc elements from the pipeline so we can read their properties
	// during execution
	rtpbin = gst.getGstElementByName("rtpbin");
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

#if ENABLE_ECHO_CANCEL
	if(echoCancel && audioChannelReady){
		appSrcAudio = gst.getGstElementByName("audioechosrc");
		if(appSrcAudio){
			gst_app_src_set_stream_type((GstAppSrc*)appSrcAudio,GST_APP_STREAM_TYPE_STREAM);
		}

		#ifdef TARGET_LINUX
			gstAudioIn.setPipelineWithSink("pulsesrc stream-properties=\"props,media.role=phone\" name=audiocapture ! audio/x-raw,format=S16LE,rate=44100,channels=1 ! audioresample ! audioconvert ! audio/x-raw,format=S16LE,rate=32000,channels=1 ! appsink name=audioechosink");
			volume = gstAudioIn.getGstElementByName("audiocapture");
		#elif defined(TARGET_OSX)
			// for osx we specify the output format since osxaudiosrc doesn't report the formats supported by the hw
			// FIXME: we should detect the format somehow and set it automatically
			gstAudioIn.setPipelineWithSink("osxaudiosrc name=audiocapture ! audio/x-raw,rate=44100,channels=1 ! volume name=volume ! audioresample ! audioconvert ! audio/x-raw,format=S16LE,rate=32000,channels=1 ! appsink name=audioechosink");
			volume = gstAudioIn.getGstElementByName("volume");
		#endif

		appSinkAudio = gstAudioIn.getGstElementByName("audioechosink");
		audiocapture = gstAudioIn.getGstElementByName("audiocapture");

		// set callbacks to receive audio data
		GstAppSinkCallbacks gstCallbacks;
		gstCallbacks.eos = &on_eos_from_audio;
		gstCallbacks.new_preroll = &on_new_preroll_from_audio;
		gstCallbacks.new_sample = &on_new_buffer_from_audio;
		gst_app_sink_set_callbacks(GST_APP_SINK(appSinkAudio), &gstCallbacks, this, NULL);
		gst_app_sink_set_emit_signals(GST_APP_SINK(appSinkAudio),0);
	}
#endif

#if ENABLE_NAT_TRANSVERSAL
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
#endif


	if(appSrcVideoRGB) gst_app_src_set_stream_type((GstAppSrc*)appSrcVideoRGB,GST_APP_STREAM_TYPE_STREAM);
	if(appSrcDepth) gst_app_src_set_stream_type((GstAppSrc*)appSrcDepth,GST_APP_STREAM_TYPE_STREAM);
	if(appSrcOsc) gst_app_src_set_stream_type((GstAppSrc*)appSrcOsc,GST_APP_STREAM_TYPE_STREAM);

	g_signal_connect(rtpbin,"on-new-ssrc",G_CALLBACK(&ofxGstRTPServer::on_new_ssrc_handler),this);

#if ENABLE_ECHO_CANCEL
	if(echoCancel && audioChannelReady){
		gstAudioIn.startPipeline();
		gstAudioIn.play();
	}
#endif

	gst.startPipeline();
	gst.play();

	ofAddListener(ofEvents().update,this,&ofxGstRTPServer::update);
}

void ofxGstRTPServer::on_new_ssrc_handler(GstBin *rtpbin, guint session, guint ssrc, ofxGstRTPServer * server){
	ofLogVerbose(LOG_NAME) << "new ssrc " << ssrc << " for session " << session;
	if(session==server->audioSessionNumber){
		server->audioSSRC = ssrc;
	}else if(session==server->videoSessionNumber){
		server->videoSSRC = ssrc;
		server->sendVideoKeyFrame = false;
	}else if(session==server->depthSessionNumber){
		server->depthSSRC = ssrc;
		server->sendDepthKeyFrame = false;
	}else if(session==server->oscSessionNumber){
		server->oscSSRC = ssrc;
	}
}

void ofxGstRTPServer::update(ofEventArgs & args){
	if(ofGetFrameNum()%60==0){
		if(videoSSRC!=0 && videoSessionNumber!=-1){
			GObject * internalSession;
			g_signal_emit_by_name(rtpbin,"get-internal-session",videoSessionNumber,&internalSession,NULL);

			if(internalSession){
				// read all the properties in the internal session
				/*GParamSpec** properties;
				guint n_properties;
				properties = g_object_class_list_properties(G_OBJECT_GET_CLASS(internalSession),&n_properties);
				for(guint i=0; i<n_properties; i++){
					cout << properties[i]->name <<endl;
				}*/

				// get internal session stats useful? perhaps sent packages and bitrate?
				GObject * internalSource;
				g_object_get(internalSession,"internal-source",&internalSource, NULL);

				GstStructure *stats;
				g_object_get (internalSource, "stats", &stats, NULL);

				//ofLogNotice(LOG_NAME) << gst_structure_to_string(stats);
				guint64 bitrate;
				gst_structure_get(stats,"bitrate",G_TYPE_UINT64,&bitrate,
										NULL);

				ofLogNotice(LOG_NAME) << "local video bitrate: " << bitrate;
			}else{
				ofLogError() << "couldn't get local stats";
			}

			GObject * remoteSource;
			g_signal_emit_by_name (internalSession, "get-source-by-ssrc", videoSSRC, &remoteSource, NULL);

			if(remoteSource){
				GstStructure *stats;
				g_object_get (remoteSource, "stats", &stats, NULL);

				ofLogNotice(LOG_NAME) << gst_structure_to_string(stats);
				uint rb_round_trip;
				int rb_packetslost;
				uint rb_fractionlost;
				uint rb_jitter;
				gst_structure_get(stats,"rb-round-trip",G_TYPE_UINT,&rb_round_trip,
										"rb-packetslost",G_TYPE_INT,&rb_packetslost,
										"rb-fractionlost",G_TYPE_UINT,&rb_fractionlost,
										"rb-jitter",G_TYPE_UINT,&rb_jitter,
										NULL);
				ofLogNotice(LOG_NAME) << "remote video: round trip:" << rb_round_trip
						<< " packetslost: " << rb_packetslost
						<< " fractionlost: " << rb_fractionlost
						<< " jitter: " << rb_jitter;

				if(videoPacketsLost<rb_packetslost){
					emitVideoKeyFrame();
				}
				videoPacketsLost = rb_packetslost;
			}else{
				ofLogError() << "couldn't get remote stats";
			}
		}

		if(depthSSRC!=0 && depthSessionNumber!=-1){
			GObject * internalSession;
			g_signal_emit_by_name(rtpbin,"get-internal-session",depthSessionNumber,&internalSession,NULL);

			GObject * remoteSource;
			g_signal_emit_by_name (internalSession, "get-source-by-ssrc", depthSSRC, &remoteSource, NULL);

			if(internalSession){
				// get internal session stats useful? perhaps sent packages and bitrate?
				GObject * internalSource;
				g_object_get(internalSession,"internal-source",&internalSource, NULL);

				GstStructure *stats;
				g_object_get (internalSource, "stats", &stats, NULL);

				//ofLogNotice(LOG_NAME) << gst_structure_to_string(stats);
				guint64 bitrate;
				gst_structure_get(stats,"bitrate",G_TYPE_UINT64,&bitrate,
										NULL);

				ofLogNotice(LOG_NAME) << "local audio bitrate: " << bitrate;
			}else{
				ofLogError() << "couldn't get local stats";
			}

			if(remoteSource){
				GstStructure *stats;
				g_object_get (remoteSource, "stats", &stats, NULL);

				//ofLogNotice(LOG_NAME) << gst_structure_to_string(stats);
				uint rb_round_trip;
				int rb_packetslost;
				uint rb_fractionlost;
				uint rb_jitter;
				gst_structure_get(stats,"rb-round-trip",G_TYPE_UINT,&rb_round_trip,
										"rb-packetslost",G_TYPE_INT,&rb_packetslost,
										"rb-fractionlost",G_TYPE_UINT,&rb_fractionlost,
										"rb-jitter",G_TYPE_UINT,&rb_jitter,
										NULL);
				ofLogNotice(LOG_NAME) << "remote audio: round trip:" << rb_round_trip
						<< " packetslost: " << rb_packetslost
						<< " fractionlost: " << rb_fractionlost
						<< " jitter: " << rb_jitter;

				if(depthPacketsLost<rb_packetslost){
					emitDepthKeyFrame();
				}
				depthPacketsLost = rb_packetslost;
			}else{
				ofLogError() << "couldn't get stats";
			}
		}

		if(audioSSRC!=0 && audioSessionNumber!=-1){
			GObject * internalSession;
			g_signal_emit_by_name(rtpbin,"get-internal-session",audioSessionNumber,&internalSession,NULL);

			GObject * remoteSource;
			g_signal_emit_by_name (internalSession, "get-source-by-ssrc", audioSSRC, &remoteSource, NULL);

			if(internalSession){
				// get internal session stats useful? perhaps sent packages and bitrate?
				/*FIXME: this is crashing on linux now
				 *
				 * GObject * internalSource;
				g_object_get(internalSession,"internal-source",&internalSource, NULL);

				GstStructure *stats;
				g_object_get (internalSource, "stats", &stats, NULL);

				//ofLogNotice(LOG_NAME) << gst_structure_to_string(stats);
				guint64 bitrate;
				gst_structure_get(stats,"bitrate",G_TYPE_UINT64,&bitrate,
										NULL);

				ofLogNotice(LOG_NAME) << "local audio bitrate: " << bitrate;*/
			}else{
				ofLogError() << "couldn't get local stats";
			}

			if(remoteSource){
				GstStructure *stats;
				g_object_get (remoteSource, "stats", &stats, NULL);

				//ofLogNotice(LOG_NAME) << gst_structure_to_string(stats);
				uint rb_round_trip;
				int rb_packetslost;
				uint rb_fractionlost;
				uint rb_jitter;
				gst_structure_get(stats,"rb-round-trip",G_TYPE_UINT,&rb_round_trip,
										"rb-packetslost",G_TYPE_INT,&rb_packetslost,
										"rb-fractionlost",G_TYPE_UINT,&rb_fractionlost,
										"rb-jitter",G_TYPE_UINT,&rb_jitter,
										NULL);
				ofLogNotice(LOG_NAME) << "remote audio: round trip:" << rb_round_trip
						<< " packetslost: " << rb_packetslost
						<< " fractionlost: " << rb_fractionlost
						<< " jitter: " << rb_jitter;
			}else{
				ofLogError() << "couldn't get stats";
			}
		}

		if(oscSSRC!=0 && oscSessionNumber!=-1){
			GObject * internalSession;
			g_signal_emit_by_name(rtpbin,"get-internal-session",oscSessionNumber,&internalSession,NULL);

			GObject * remoteSource;
			g_signal_emit_by_name (internalSession, "get-source-by-ssrc", oscSSRC, &remoteSource, NULL);

			if(remoteSource){
				GstStructure *stats;
				g_object_get (remoteSource, "stats", &stats, NULL);

				ofLogNotice(LOG_NAME) << gst_structure_to_string(stats);
			}else{
				ofLogError() << "couldn't get stats";
			}
		}
	}
}

bool ofxGstRTPServer::on_message(GstMessage * msg){
	// read messages from the pipeline like dropped packages
	switch (GST_MESSAGE_TYPE (msg)) {
	case GST_MESSAGE_ELEMENT:{
		GstObject * messageSrc = GST_MESSAGE_SRC(msg);
		ofLogVerbose(LOG_NAME) << "Got " << GST_MESSAGE_TYPE_NAME(msg) << " message from " << GST_MESSAGE_SRC_NAME(msg);
		ofLogVerbose(LOG_NAME) << "Message source type: " << G_OBJECT_CLASS_NAME(G_OBJECT_GET_CLASS(messageSrc));
		ofLogVerbose(LOG_NAME) << "With structure name: " << gst_structure_get_name(gst_message_get_structure(msg));
		ofLogVerbose(LOG_NAME) << gst_structure_to_string(gst_message_get_structure(msg));
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
		//ofLogVerbose(LOG_NAME) << "Got " << GST_MESSAGE_TYPE_NAME(msg) << " message from " << GST_MESSAGE_SRC_NAME(msg);
		return false;
	}
}

void ofxGstRTPServer::emitVideoKeyFrame(){
	GstClock * clock = gst_pipeline_get_clock(GST_PIPELINE(gst.getPipeline()));
	gst_object_ref(clock);
	GstClockTime time = gst_clock_get_time (clock);
	GstClockTime now =  time - gst_element_get_base_time(gst.getPipeline());
	gst_object_unref (clock);
	GstEvent * keyFrameEvent = gst_video_event_new_downstream_force_key_unit(now,
															 time,
															 now,
															 TRUE,
															 0);
	gst_element_send_event(appSrcVideoRGB,keyFrameEvent);

}

void ofxGstRTPServer::emitDepthKeyFrame(){
	GstClock * clock = gst_pipeline_get_clock(GST_PIPELINE(gst.getPipeline()));
	gst_object_ref(clock);
	GstClockTime time = gst_clock_get_time (clock);
	GstClockTime now =  time - gst_element_get_base_time(gst.getPipeline());
	gst_object_unref (clock);
	GstEvent * keyFrameEvent = gst_video_event_new_downstream_force_key_unit(now,
															 time,
															 now,
															 TRUE,
															 0);
	gst_element_send_event(appSrcDepth,keyFrameEvent);

}

void ofxGstRTPServer::newFrame(ofPixels & pixels){
	// here we push new video frames in the pipeline, it's important
	// to timestamp them properly so gstreamer can sync them with the
	// audio.

	if(!bufferPool || !appSrcVideoRGB) return;

	// get current time from the pipeline
	/*GstClock * clock = gst_pipeline_get_clock(GST_PIPELINE(gst.getPipeline()));
	gst_object_ref(clock);
	GstClockTime time = gst_clock_get_time (clock);
	GstClockTime now =  time - gst_element_get_base_time(gst.getPipeline());
	gst_object_unref (clock);

	if(firstVideoFrame){
		prevTimestamp = now;
		firstVideoFrame = false;
		return;
	}*/

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
	/*GST_BUFFER_OFFSET(buffer) = numFrame++;
	GST_BUFFER_OFFSET_END(buffer) = numFrame;
	GST_BUFFER_DTS (buffer) = now;
	GST_BUFFER_PTS (buffer) = now;
	GST_BUFFER_DURATION(buffer) = now-prevTimestamp;
	prevTimestamp = now;*/


	if(sendVideoKeyFrame && numFrame%5==0){
		emitVideoKeyFrame();
	}
	numFrame++;

	// finally push the buffer into the pipeline through the appsrc element
	GstFlowReturn flow_return = gst_app_src_push_buffer((GstAppSrc*)appSrcVideoRGB, buffer);
	if (flow_return != GST_FLOW_OK) {
		ofLogError() << "error pushing video buffer: flow_return was " << flow_return;
	}
}


void ofxGstRTPServer::newFrameDepth(ofPixels & pixels){
	// here we push new depth frames in the pipeline, it's important
	// to timestamp them properly so gstreamer can sync them with the
	// audio.

	if(!bufferPoolDepth || !appSrcDepth) return;

	// get current time from the pipeline
	/*GstClock * clock = gst_pipeline_get_clock(GST_PIPELINE(gst.getPipeline()));
	gst_object_ref(clock);
	GstClockTime time = gst_clock_get_time (clock);
	GstClockTime now = time - gst_element_get_base_time(gst.getPipeline());
	gst_object_unref (clock);

	if(firstDepthFrame){
		prevTimestampDepth = now;
		firstDepthFrame = false;
		return;
	}*/

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
	/*GST_BUFFER_OFFSET(buffer) = numFrameDepth++;
	GST_BUFFER_OFFSET_END(buffer) = numFrameDepth;
	GST_BUFFER_DTS (buffer) = now;
	GST_BUFFER_PTS (buffer) = now;
	GST_BUFFER_DURATION(buffer) = now-prevTimestampDepth;
	prevTimestampDepth = now;*/

	if(sendDepthKeyFrame){
		emitDepthKeyFrame();
	}

	// finally push the buffer into the pipeline through the appsrc element
	GstFlowReturn flow_return = gst_app_src_push_buffer((GstAppSrc*)appSrcDepth, buffer);
	if (flow_return != GST_FLOW_OK) {
		ofLogError() << "error pushing depth buffer: flow_return was " << flow_return;
	}
}


void ofxGstRTPServer::newFrameDepth(ofShortPixels & pixels){
	//unsigned long long time = ofGetElapsedTimeMicros();

	// here we push new depth frames in the pipeline, it's important
	// to timestamp them properly so gstreamer can sync them with the
	// audio.

	if(!bufferPoolDepth || !appSrcDepth) return;

	// get current time from the pipeline
	/*GstClock * clock = gst_pipeline_get_clock(GST_PIPELINE(gst.getPipeline()));
	gst_object_ref(clock);
	GstClockTime now = gst_clock_get_time (clock) - gst_element_get_base_time(gst.getPipeline());
	gst_object_unref (clock);

	if(firstDepthFrame){
		prevTimestampDepth = now;
		firstDepthFrame = false;
		return;
	}*/

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
	/*GST_BUFFER_OFFSET(buffer) = numFrameDepth++;
	GST_BUFFER_OFFSET_END(buffer) = numFrameDepth;
	GST_BUFFER_DTS (buffer) = now;
	GST_BUFFER_PTS (buffer) = now;
	GST_BUFFER_DURATION(buffer) = now-prevTimestampDepth;
	prevTimestampDepth = now;*/

	if(sendDepthKeyFrame){
		GstClock * clock = gst_pipeline_get_clock(GST_PIPELINE(gst.getPipeline()));
		gst_object_ref(clock);
		GstClockTime time = gst_clock_get_time (clock);
		GstClockTime now = time - gst_element_get_base_time(gst.getPipeline());
		gst_object_unref (clock);
		GstEvent * keyFrameEvent = gst_video_event_new_downstream_force_key_unit(now,
																 time,
																 now,
																 TRUE,
																 0);
		gst_element_send_event(gst.getPipeline(),keyFrameEvent);
	}

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
	/*GstClock * clock = gst_pipeline_get_clock(GST_PIPELINE(gst.getPipeline()));
	gst_object_ref(clock);
	GstClockTime now = gst_clock_get_time (clock) - gst_element_get_base_time(gst.getPipeline());
	gst_object_unref (clock);

	if(firstOscFrame){
		prevTimestampOsc = now;
		firstOscFrame = false;
		return;
	}*/

	PooledOscPacket * pooledOscPkg = oscPacketPool.newBuffer();
	appendMessage(msg,pooledOscPkg->packet);

	GstBuffer * buffer = gst_buffer_new_wrapped_full(GST_MEMORY_FLAG_READONLY,(void*)pooledOscPkg->compressedData(),pooledOscPkg->compressedSize(),0,pooledOscPkg->compressedSize(),pooledOscPkg,(GDestroyNotify)&ofxOscPacketPool::relaseBuffer);

	/*GST_BUFFER_OFFSET(buffer) = numFrameOsc++;
	GST_BUFFER_OFFSET_END(buffer) = numFrameOsc;
	GST_BUFFER_DTS (buffer) = now;
	GST_BUFFER_PTS (buffer) = now;
	GST_BUFFER_DURATION(buffer) = now-prevTimestampOsc;
	prevTimestampOsc = now;*/

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

#if ENABLE_ECHO_CANCEL
void ofxGstRTPServer::on_eos_from_audio(GstAppSink * elt, void * rtpClient){

}


GstFlowReturn ofxGstRTPServer::on_new_preroll_from_audio(GstAppSink * elt, void * rtpClient){
	return GST_FLOW_OK;
}

void ofxGstRTPServer::sendAudioOut(PooledAudioFrame * pooledFrame){
	if(firstAudioFrame){
		/*GstClock * clock = gst_pipeline_get_clock(GST_PIPELINE(gst.getPipeline()));
		gst_object_ref(clock);
		GstClockTime now = gst_clock_get_time (clock) - gst_element_get_base_time(gst.getPipeline());
		gst_object_unref (clock);
		prevTimestampAudio = now;
		firstAudioFrame = false;
		return;*/
	}

	int size = pooledFrame->audioFrame._payloadDataLengthInSamples*2*pooledFrame->audioFrame._audioChannel;

	GstBuffer * echoCancelledBuffer = gst_buffer_new_wrapped_full(GST_MEMORY_FLAG_READONLY,(void*)pooledFrame->audioFrame._payloadData,size,0,size,pooledFrame,(GDestroyNotify)&ofxWebRTCAudioPool::relaseFrame);

	/*GstClockTime duration = (pooledFrame->audioFrame._payloadDataLengthInSamples * GST_SECOND / pooledFrame->audioFrame._frequencyInHz);
	GstClockTime now = prevTimestamp + duration;

	GST_BUFFER_OFFSET(echoCancelledBuffer) = numFrameAudio++;
	GST_BUFFER_OFFSET_END(echoCancelledBuffer) = numFrameAudio;
	GST_BUFFER_DTS (echoCancelledBuffer) = now;
	GST_BUFFER_PTS (echoCancelledBuffer) = now;
	GST_BUFFER_DURATION(echoCancelledBuffer) = duration;
	prevTimestampAudio = now;*/


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

		int delay = server->gstAudioIn.getMinLatencyNanos()*0.000001 + server->client->getAudioOutLatencyMs();

		const int numChannels = 1;
		const int samplerate = 32000;
		int buffersize = gst_buffer_get_size(buffer)/2/numChannels;
		const int samplesIn10Ms = samplerate/100;

		if(server->prevAudioBuffer){
			PooledAudioFrame * audioFrame = server->audioPool.newFrame();
			gst_buffer_map (server->prevAudioBuffer, &server->mapinfo, GST_MAP_READ);
			int prevBuffersize = gst_buffer_get_size(server->prevAudioBuffer)/2/numChannels;
			memcpy(audioFrame->audioFrame._payloadData,((short*)server->mapinfo.data)+(posInBuffer*numChannels),(prevBuffersize-posInBuffer)*numChannels*sizeof(short));

			gst_buffer_unmap(server->prevAudioBuffer,&server->mapinfo);
			gst_buffer_unref(server->prevAudioBuffer);

			gst_buffer_map (buffer, &server->mapinfo, GST_MAP_READ);
			memcpy(audioFrame->audioFrame._payloadData+((prevBuffersize-posInBuffer)*numChannels),((short*)server->mapinfo.data),(samplesIn10Ms-(prevBuffersize-posInBuffer))*numChannels*sizeof(short));

			audioFrame->audioFrame._payloadDataLengthInSamples = samplesIn10Ms;
			audioFrame->audioFrame._audioChannel = numChannels;
			audioFrame->audioFrame._frequencyInHz = samplerate;

			if(server->echoCancel->echoCancelEnabled){
				server->echoCancel->getAudioProcessing()->set_stream_delay_ms(delay);
			}
			if(server->echoCancel->echoCancelEnabled && server->echoCancel->driftCompensationEnabled){
				int drift = (-1*(server->reverseDriftCalculation?1:0))*((int64_t)server->client->getAudioFramesProcessed()-(int64_t)server->audioFramesProcessed);
				server->echoCancel->getAudioProcessing()->echo_cancellation()->set_stream_drift_samples(drift);
			}
			if(server->echoCancel->gainControlEnabled){
				server->echoCancel->getAudioProcessing()->gain_control()->set_stream_analog_level(server->analogAudio);
			}
			server->echoCancel->process(audioFrame->audioFrame);// << endl;
			if(server->echoCancel->voiceDetectionEnabled && !server->echoCancel->getAudioProcessing()->voice_detection()->stream_has_voice()){
				memset(audioFrame->audioFrame._payloadData,0,samplesIn10Ms*numChannels*sizeof(short));
			}
			if(server->echoCancel->gainControlEnabled){
				server->analogAudio = server->echoCancel->getAudioProcessing()->gain_control()->stream_analog_level();
				g_object_set(server->volume,"volume",server->analogAudio/double(0x10000U),NULL);
			}

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

			if(server->echoCancel->echoCancelEnabled){
				server->echoCancel->getAudioProcessing()->set_stream_delay_ms(delay);
			}
			if(server->echoCancel->echoCancelEnabled && server->echoCancel->driftCompensationEnabled){
				int drift = (-1*(server->reverseDriftCalculation?1:0))*((int64_t)server->client->getAudioFramesProcessed()-(int64_t)server->audioFramesProcessed);
				server->echoCancel->getAudioProcessing()->echo_cancellation()->set_stream_drift_samples(drift);
			}
			if(server->echoCancel->gainControlEnabled){
				server->echoCancel->getAudioProcessing()->gain_control()->set_stream_analog_level(server->analogAudio);
			}

			server->echoCancel->process(audioFrame->audioFrame);// << endl;
			if(server->echoCancel->voiceDetectionEnabled && !server->echoCancel->getAudioProcessing()->voice_detection()->stream_has_voice()){
				memset(audioFrame->audioFrame._payloadData,0,samplesIn10Ms*numChannels*sizeof(short));
			}
			if(server->echoCancel->gainControlEnabled){
				server->analogAudio = server->echoCancel->getAudioProcessing()->gain_control()->stream_analog_level();
				g_object_set(server->volume,"volume",server->analogAudio/double(0x10000U),NULL);
			}

			server->sendAudioOut(audioFrame);
			posInBuffer+=samplesIn10Ms;
			server->audioFramesProcessed += samplesIn10Ms;
		};

		if(posInBuffer<buffersize){
			server->prevAudioBuffer = buffer;
			gst_buffer_ref(server->prevAudioBuffer);
		}else{
			server->prevAudioBuffer = 0;
		}

		gst_buffer_unmap(buffer,&server->mapinfo);
		gst_sample_unref(sample);
	}
	return GST_FLOW_OK;
}

#endif
