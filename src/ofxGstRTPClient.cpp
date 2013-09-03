/*
 * ofxGstRTPClient.cpp
 *
 *  Created on: Jul 20, 2013
 *      Author: arturo
 */

#include "ofxGstRTPClient.h"

#include <gst/app/gstappsrc.h>
#include <gst/video/video.h>
#include <gst/video/gstvideometa.h>

#include <gst/rtp/gstrtcpbuffer.h>

#include <glib-object.h>
#include <glib.h>
#include <list>
//#include <gst/gstnice.h>

#include "ofxGstRTPUtils.h"

string ofxGstRTPClient::LOG_NAME="ofxGstRTPClient";


//  receives H264 encoded RTP video on port 5000, RTCP is received on  port 5001.
//  the receiver RTCP reports are sent to port 5005
//
//  receives OPUS encoded RTP audio on port 5002, RTCP is received on  port 5003.
//  the receiver RTCP reports are sent to port 5007
//
//             .-------.      .----------.     .---------.   .-------.   .-----------.
//  RTP        |udpsrc |      | rtpbin   |     |h264depay|   |h264dec|   |appsink    |
//  port=5000  |      src->recv_rtp recv_rtp->sink     src->sink   src->sink         |
//             '-------'      |          |     '---------'   '-------'   '-----------'
//                            |          |
//                            |          |     .-------.
//                            |          |     |udpsink|  RTCP
//                            |    send_rtcp->sink     | port=5005
//             .-------.      |          |     '-------' sync=false
//  RTCP       |udpsrc |      |          |               async=false
//  port=5001  |     src->recv_rtcp      |
//             '-------'      |          |
//                            |          |
//             .-------.      |          |     .---------.   .-------.   .-------------.
//  RTP        |udpsrc |      | rtpbin   |     |opusdepay|   |opusdec|   |autoaudiosink|
//  port=5002  |      src->recv_rtp recv_rtp->sink     src->sink   src->sink           |
//             '-------'      |          |     '---------'   '-------'   '-------------'
//                            |          |
//                            |          |     .-------.
//                            |          |     |udpsink|  RTCP
//                            |    send_rtcp->sink     | port=5007
//             .-------.      |          |     '-------' sync=false
//  RTCP       |udpsrc |      |          |               async=false
//  port=5003  |     src->recv_rtcp      |
//             '-------'      '----------'

ofxGstRTPClient::ofxGstRTPClient()
:width(0)
,height(0)
,pipeline(0)
,rtpbin(0)
,vh264depay(0)
,opusdepay(0)
,dh264depay(0)
,gstdepay(0)
,videoSink(0)
,depthSink(0)
,oscSink(0)
,vudpsrc(0)
,audpsrc(0)
,dudpsrc(0)
,oudpsrc(0)
,vudpsrcrtcp(0)
,audpsrcrtcp(0)
,dudpsrcrtcp(0)
,oudpsrcrtcp(0)
,depth16(false)
,latency(0)
,videoSessionNumber(-1)
,audioSessionNumber(-1)
,depthSessionNumber(-1)
,oscSessionNumber(-1)
,videoSSRC(0)
,audioSSRC(0)
,depthSSRC(0)
,oscSSRC(0)
,videoReady(false)
,audioReady(false)
,depthReady(false)
,oscReady(false)
,lastSessionNumber(0)
,videoStream(NULL)
,depthStream(NULL)
,oscStream(NULL)
,audioStream(NULL)
{
	GstMapInfo initMapinfo		= {0,};
	mapinfo = initMapinfo;
}

ofxGstRTPClient::~ofxGstRTPClient() {
}


static string get_object_structure_property (GObject * object, const string & property){
	GstStructure *structure;
	gchar *str;

	if(object==NULL) return "";

	/* get the source stats */
	g_object_get (object, property.c_str(), &structure, NULL);

	/* simply dump the stats structure */
	str = gst_structure_to_string (structure);

	gst_structure_free (structure);
	string ret = str;
	g_free (str);
	return ret;
}


void ofxGstRTPClient::on_ssrc_active_handler(GstBin * rtpbin, guint session, guint ssrc, ofxGstRTPClient * rtpClient){
	GObject * internalSession;
	g_signal_emit_by_name(rtpbin,"get-internal-session",session,&internalSession,NULL);
	ofLogVerbose(LOG_NAME) << "ssrc active " << G_OBJECT_CLASS_NAME(G_OBJECT_GET_CLASS(internalSession));

	GObject * internalSource;
	g_object_get (internalSession, "internal-source", &internalSource, NULL);
	//ofLogVerbose(LOG_NAME) << get_object_structure_property(internalSource,"stats");

	GObject * remoteSource;
	g_signal_emit_by_name (internalSession, "get-source-by-ssrc", ssrc, &remoteSource, NULL);
	//ofLogVerbose(LOG_NAME) << get_object_structure_property(remoteSource,"stats");
}


void ofxGstRTPClient::on_new_ssrc_handler(GstBin *rtpbin, guint session, guint ssrc, ofxGstRTPClient * rtpClient){
	ofLogVerbose(LOG_NAME) << "new ssrc " << ssrc << " for session " << session;
	GObject * internalSession;
	g_signal_emit_by_name(rtpbin,"get-internal-session",session,&internalSession,NULL);

	GObject * remoteSource;
	g_signal_emit_by_name (internalSession, "get-source-by-ssrc", ssrc, &remoteSource, NULL);


	GObject * internalSource;
	g_object_get (internalSession, "internal-source", &internalSource, NULL);
	ofLogVerbose(LOG_NAME) << get_object_structure_property(internalSource,"stats");

	GstStructure *stats;
	gchar* remoteAddress=0;
	g_object_get (remoteSource, "stats", &stats, NULL);
	gst_structure_get(stats,"rtcp-from",G_TYPE_STRING,&remoteAddress,NULL);

	if(remoteAddress){
		ofLogVerbose(LOG_NAME) << "new client connected from " << remoteAddress;
		g_free(remoteAddress);
	}else{
		ofLogVerbose(LOG_NAME) << "couldn't get remote";
	}
	ofLogVerbose(LOG_NAME) << get_object_structure_property(remoteSource,"stats");

	//GstPad * srcpad = gst_element_get_request_pad(rtpbin,("recv_rtp_src_"+ofToString(session)+"_"+ofToString(ssrc));
}


void ofxGstRTPClient::on_pad_added(GstBin *rtpbin, GstPad *pad, ofxGstRTPClient * rtpClient){
	// when a pad is added to the rtbbin, connect the video and depth elements to the correct pads
	// FIXME: there must be a better way to detect the correct pad than it's partial name

	string padName = gst_object_get_name(GST_OBJECT(pad));


	ofLogVerbose(LOG_NAME) << "new pad " << gst_object_get_name(GST_OBJECT(pad));

	if(ofIsStringInString(padName,"recv_rtp_src_"+ofToString(rtpClient->videoSessionNumber))){
		ofLogVerbose(LOG_NAME) << "video pad created";
		rtpClient->linkVideoPad(pad);

	}else if(ofIsStringInString(padName,"recv_rtp_src_"+ofToString(rtpClient->audioSessionNumber))){
		ofLogVerbose(LOG_NAME) << "audio pad created";
		rtpClient->linkAudioPad(pad);

	}else if(ofIsStringInString(padName,"recv_rtp_src_"+ofToString(rtpClient->depthSessionNumber))){
		ofLogVerbose(LOG_NAME) << "depth pad created";
		rtpClient->linkDepthPad(pad);

	}else if(ofIsStringInString(padName,"recv_rtp_src_"+ofToString(rtpClient->oscSessionNumber))){
		ofLogVerbose(LOG_NAME) << "osc pad created";
		rtpClient->linkOscPad(pad);
	}
}

void ofxGstRTPClient::linkAudioPad(GstPad * pad){
	//FIXME: this should get the state instead of async set
	//gst_element_set_state(gst.getPipeline(),GST_STATE_PAUSED);

	GstPad * sinkPad = gst_element_get_static_pad(opusdepay,"sink");
	if(sinkPad){
		if(gst_pad_link(pad,sinkPad)!=GST_PAD_LINK_OK){
			ofLogError(LOG_NAME) << "couldn't link rtp source pad to audio depay";
		}else{
			cout << "audio pipeline complete!" << endl;
			audioReady = true;
			//gst_element_set_state(gst.getPipeline(),GST_STATE_PLAYING);
		}
	}else{
		ofLogError(LOG_NAME) << "couldn't get sink pad for opus depay";
	}

	//g_signal_emit_by_name(gst.getGstElementByName("rtpbin"),"reset-sync",NULL);

	//gst_element_set_state(gst.getPipeline(),GST_STATE_PLAYING);
}

void ofxGstRTPClient::linkVideoPad(GstPad * pad){
	//FIXME: this should get the state instead of async set
	//gst_element_set_state(gst.getPipeline(),GST_STATE_PAUSED);

	GstPad * sinkPad = gst_element_get_static_pad(vh264depay,"sink");
	if(sinkPad){
		if(gst_pad_link(pad,sinkPad)!=GST_PAD_LINK_OK){
			ofLogError(LOG_NAME) << "couldn't link rtp source pad to video depay";
		}else{
			cout << "video pipeline complete!" << endl;
			videoReady = true;
			//gst_element_set_state(gst.getPipeline(),GST_STATE_PLAYING);
		}
	}else{
		ofLogError(LOG_NAME) << "couldn't get sink pad for h264 depay";
	}

	//g_signal_emit_by_name(gst.getGstElementByName("rtpbin"),"reset-sync",NULL);

	//gst_element_set_state(gst.getPipeline(),GST_STATE_PLAYING);
}

void ofxGstRTPClient::linkDepthPad(GstPad * pad){
	//FIXME: this should get the state instead of async set
	//gst_element_set_state(gst.getPipeline(),GST_STATE_PAUSED);

	GstPad * sinkPad = gst_element_get_static_pad(dh264depay,"sink");
	if(sinkPad){
		if(gst_pad_link(pad,sinkPad)!=GST_PAD_LINK_OK){
			ofLogError(LOG_NAME) << "couldn't link rtp source pad to depth depay";
		}else{
			cout << "depth pipeline complete!" << endl;
			depthReady = true;
			//gst_element_set_state(gst.getPipeline(),GST_STATE_PLAYING);
		}
	}else{
		ofLogError(LOG_NAME) << "couldn't get sink pad for h264 depay";
	}

	//g_signal_emit_by_name(gst.getGstElementByName("rtpbin"),"reset-sync",NULL);

	//gst_element_set_state(gst.getPipeline(),GST_STATE_PLAYING);
}


void ofxGstRTPClient::linkOscPad(GstPad * pad){

	//FIXME: this should get the state instead of async set
	//gst_element_set_state(gst.getPipeline(),GST_STATE_PAUSED);

	GstPad * sinkPad = gst_element_get_static_pad(gstdepay,"sink");
	if(sinkPad){
		if(gst_pad_link(pad,sinkPad)!=GST_PAD_LINK_OK){
			ofLogError(LOG_NAME) << "couldn't link rtp source pad to osc depay";
		}else{
			cout << "osc pipeline complete!" << endl;
			oscReady = true;
			//gst_element_set_state(gst.getPipeline(),GST_STATE_PLAYING);
		}
	}else{
		ofLogError(LOG_NAME) << "couldn't get sink pad for osc depay";
	}

	//g_signal_emit_by_name(gst.getGstElementByName("rtpbin"),"reset-sync",NULL);

	//gst_element_set_state(gst.getPipeline(),GST_STATE_PLAYING);
}

void ofxGstRTPClient::on_bye_ssrc_handler(GstBin *rtpbin, guint session, guint ssrc, ofxGstRTPClient * rtpClient){
	ofLogVerbose(LOG_NAME) << "client disconnected";

}

void ofxGstRTPClient::createNetworkElements(NetworkElementsProperties properties, ofxNiceStream * niceStream){

	// create video nicesrc elements and add them to the pipeline
	GstCaps * caps = gst_caps_from_string(properties.capsstr.c_str());

	if(niceStream){
		*properties.source = gst_element_factory_make("nicesrc",properties.sourceName.c_str());
		if(!*properties.source){
			ofLogError(LOG_NAME) << "couldn't create rtp nicesrc";
		}
		g_object_set(G_OBJECT(*properties.source),"agent",niceStream->getAgent(),"stream",niceStream->getStreamID(),"component",1,NULL);

		GstElement * capsfilter = gst_element_factory_make("capsfilter",properties.capsfiltername.c_str());
		g_object_set(G_OBJECT(capsfilter),"caps",caps,NULL);
		gst_caps_unref(caps);

		gst_bin_add_many(GST_BIN(pipeline),*properties.source,capsfilter,NULL);
		gst_element_link_many(*properties.source,capsfilter,NULL);

		GstPad * sinkpad = gst_element_get_request_pad(rtpbin,("recv_rtp_sink_"+ofToString(properties.sessionNumber)).c_str());
		GstPad * srcpad = gst_element_get_static_pad(capsfilter,"src");
		if(!sinkpad){
			ofLogError(LOG_NAME) << "couldn't get rtpbin sink for session " << properties.sessionNumber;
		}
		if(gst_pad_link(srcpad,sinkpad)!=GST_PAD_LINK_OK){
			ofLogError(LOG_NAME) << "couldn't link src to rtpbin";
		}
	}else{
		*properties.source = gst_element_factory_make("udpsrc",properties.sourceName.c_str());
		g_object_set(G_OBJECT(*properties.source),"port",properties.port,"caps",caps,NULL);
		gst_caps_unref(caps);

		gst_bin_add(GST_BIN(pipeline),*properties.source);

		GstPad * sinkpad = gst_element_get_request_pad(rtpbin,("recv_rtp_sink_"+ofToString(properties.sessionNumber)).c_str());
		GstPad * srcpad = gst_element_get_static_pad(*properties.source,"src");
		if(!sinkpad){
			ofLogError(LOG_NAME) << "couldn't get rtpbin sink for session " << properties.sessionNumber;
		}
		if(gst_pad_link(srcpad,sinkpad)!=GST_PAD_LINK_OK){
			ofLogError(LOG_NAME) << "couldn't link src to rtpbin";
		}
	}

	// create video nicesrc for rtpc
	if(niceStream){
		*properties.rtpcsource = gst_element_factory_make("nicesrc",properties.rtpcSourceName.c_str());

		if(!*properties.rtpcsource){
			ofLogError(LOG_NAME) << "couldn't create rtcp nicesrc";
		}

		g_object_set(G_OBJECT(*properties.rtpcsource),"agent",niceStream->getAgent(),"stream",niceStream->getStreamID(),"component",2,NULL);
	}else{
		*properties.rtpcsource = gst_element_factory_make("udpsrc",properties.rtpcSourceName.c_str());
		g_object_set(G_OBJECT(*properties.rtpcsource),"port",properties.rtpcsrcport,NULL);
	}

	gst_bin_add(GST_BIN(pipeline),*properties.rtpcsource);
	GstPad * rtcpsinkpad = gst_element_get_request_pad(rtpbin,("recv_rtcp_sink_"+ofToString(properties.sessionNumber)).c_str());
	GstPad * rtcpsrcpad = gst_element_get_static_pad(*properties.rtpcsource,"src");
	if(gst_pad_link(rtcpsrcpad,rtcpsinkpad)!=GST_PAD_LINK_OK){
		ofLogError(LOG_NAME) << "couldn't link rtpc src to rtpbin";
	}

	// create rtcp sink
	if(niceStream){
		*properties.rtpcsink = gst_element_factory_make("nicesink",properties.rtpcSinkName.c_str());

		if(!*properties.rtpcsink){
			ofLogError(LOG_NAME) << "couldn't create rtcp nicesink";
		}

		g_object_set(G_OBJECT(*properties.rtpcsink),"agent",niceStream->getAgent(),"stream",niceStream->getStreamID(),"component",3,NULL);
	}else{
		*properties.rtpcsink = gst_element_factory_make("udpsink",properties.rtpcSinkName.c_str());
		g_object_set(G_OBJECT(*properties.rtpcsink),"port",properties.rtpcsinkport, "host", properties.srcIP.c_str(), "sync",0, "force-ipv4",1, "async",0,NULL);
	}

	gst_bin_add(GST_BIN(pipeline),*properties.rtpcsink);

	rtcpsrcpad = gst_element_get_request_pad(rtpbin,("send_rtcp_src_"+ofToString(properties.sessionNumber)).c_str());
	rtcpsinkpad = gst_element_get_static_pad(*properties.rtpcsink,"sink");
	if(gst_pad_link(rtcpsrcpad,rtcpsinkpad)!=GST_PAD_LINK_OK){
		ofLogError(LOG_NAME) << "couldn't link rptbin src to rtpc sink";
	}
}


void ofxGstRTPClient::createVideoChannel(string rtpCaps, int w, int h, int fps){
	videoSessionNumber = lastSessionNumber;
	lastSessionNumber++;


	// create and add video and depth elements and connect them to the correct pad.
	// if we don't do this after the pad has been created when the connection is detected,
	// gstreamer tries to link this elements by their capabilities and
	// since video and depth have the same it sometimes swap them and you end getting rgb on the depth sink
	// and viceversa

	// rgb pipeline to be connected to the corresponding recv_rtp_send pad:
	// rtph264depay ! avdec_h264 ! videoconvert ! appsink
	vh264depay = gst_element_factory_make("rtph264depay","rtph264depay_video");
	GstElement * vqueue = gst_element_factory_make("queue","vqueue");

	// TODO: this improves sync but makes the streams way more noisy
	//g_object_set(vqueue,"leaky",2, "max-size-buffers",5,NULL);

	GstElement * avdec_h264 = gst_element_factory_make("avdec_h264","avdec_h264_video");
	GstElement * vconvert = gst_element_factory_make("videoconvert","vconvert");
	videoSink = (GstAppSink*)gst_element_factory_make("appsink","videosink");

	// set format for video appsink to rgb
	GstCaps * caps = NULL;
	caps = gst_caps_new_simple("video/x-raw",
					"format",G_TYPE_STRING,"RGB",
					"width", G_TYPE_INT,w,
					"height", G_TYPE_INT,h,
					NULL);

	if(!caps){
		ofLogError(LOG_NAME) << "couldn't get caps";
	}else{
		gst_app_sink_set_caps(videoSink,caps);
		gst_caps_unref(caps);
	}

	// set callbacks to receive rgb data
	GstAppSinkCallbacks gstCallbacks;
	gstCallbacks.eos = &on_eos_from_video;
	gstCallbacks.new_preroll = &on_new_preroll_from_video;
	gstCallbacks.new_sample = &on_new_buffer_from_video;
	gst_app_sink_set_callbacks(GST_APP_SINK(videoSink), &gstCallbacks, this, NULL);
	gst_app_sink_set_emit_signals(GST_APP_SINK(videoSink),0);

	// add elements to the pipeline and link them (but not yet to the rtpbin)
	gst_bin_add_many(GST_BIN(pipeline), vh264depay, vqueue, avdec_h264, vconvert, videoSink, NULL);
	if(!gst_element_link_many(vh264depay, vqueue, avdec_h264, vconvert, videoSink, NULL)){
		ofLogError(LOG_NAME) << "couldn't link video elements";
	}
}

void ofxGstRTPClient::createAudioChannel(string rtpCaps){
	audioSessionNumber = lastSessionNumber;
	lastSessionNumber++;


	// audio OPUS, RTP depay and decoder
	string adec="rtpopusdepay ! opusdec";
	// audio format conversion (float -> int or similar) + resampling (48000 -> 44100)
#ifdef TARGET_LINUX
	string asink="audioconvert ! audioresample ! pulsesink stream-properties=\"props,media.role=phone,filter.want=echo-cancel\"";
#else
	string asink="audioconvert ! audioresample ! autoaudiosink";
#endif

	// create and add audio elements and connect them to the correct pad.
	// audio pipeline to be connected to the corresponding recv_rtp_send pad:
	// Linux:
	// rtpopusdepay ! opusdec ! audioconvert ! audioresample ! pulsesink stream-properties=\"props,media.role=phone,filter.want=echo-cancel\"
	// everything else:
	// rtpopusdepay ! opusdec ! audioresample ! autoaudiosink

	opusdepay = gst_element_factory_make("rtpopusdepay","rtpopusdepay1");
	GstElement * opusdec = gst_element_factory_make("opusdec","opusdec1");
	GstElement * audioconvert = gst_element_factory_make("audioconvert","audioconvert1");
	GstElement * audioresample = gst_element_factory_make("audioresample","audioresample1");
#ifdef TARGET_LINUX
	GstElement * audiosink = gst_element_factory_make("pulsesink","pulsesink1");
	GstStructure * pulseProperties = gst_structure_new("props","media.role",G_TYPE_STRING,"phone","filter.want",G_TYPE_STRING,"echo-cancel",NULL);
	g_object_set(audiosink,"stream-properties",pulseProperties,NULL);
#else
	GstElement * audiosink = gst_element_factory_make("autoaudiosink","autoaudiosink1");
#endif


	// add elements to the pipeline and link them (but not yet to the rtpbin)
	gst_bin_add_many(GST_BIN(pipeline), opusdepay, opusdec, audioconvert, audioresample, audiosink, NULL);
	if(!gst_element_link_many(opusdepay, opusdec, audioconvert, audioresample, audiosink, NULL)){
		ofLogError(LOG_NAME) << "couldn't link audio elements";
	}

}

void ofxGstRTPClient::createDepthChannel(string rtpCaps, int w, int h, int fps, bool depth16){
	depthSessionNumber = lastSessionNumber;
	lastSessionNumber++;

	this->depth16 = depth16;


	// create and add depth elements and connect them to the correct pad.
	// depth pipeline to be connected to the corresponding recv_rtp_send pad:
	// rtph264depay ! avdec_h264 ! videoconvert ! appsink
	dh264depay = gst_element_factory_make("rtph264depay","rtph264depay_depth");
	GstElement * dqueue = gst_element_factory_make("queue","dqueue");

	// TODO: this improves sync but makes the streams way more noisy
	//g_object_set(dqueue,"leaky",2, "max-size-buffers",5,NULL);

	GstElement * avdec_h264 = gst_element_factory_make("avdec_h264","avdec_h264_depth");
	GstElement * vconvert = gst_element_factory_make("videoconvert","dconvert");
	depthSink = (GstAppSink*)gst_element_factory_make("appsink","depthsink");

	// set format for depth appsink to gray 8bits
	GstCaps * caps;

	if(depth16){
		caps = gst_caps_new_simple("video/x-raw",
				"format",G_TYPE_STRING,"RGB",
				"width", G_TYPE_INT,w,
				"height", G_TYPE_INT,h,
				NULL);
	}else{
		caps = gst_caps_new_simple("video/x-raw",
				"format",G_TYPE_STRING,"GRAY8",
				"width", G_TYPE_INT,w,
				"height", G_TYPE_INT,h,
				NULL);
	}
	if(!caps){
		ofLogError(LOG_NAME) << "couldn't get caps";
	}else{
		gst_app_sink_set_caps(depthSink,caps);
		gst_caps_unref(caps);
	}

	// set callbacks to receive depth data
	GstAppSinkCallbacks gstCallbacks;
	gstCallbacks.eos = &on_eos_from_depth;
	gstCallbacks.new_preroll = &on_new_preroll_from_depth;
	gstCallbacks.new_sample = &on_new_buffer_from_depth;
	gst_app_sink_set_callbacks(GST_APP_SINK(depthSink), &gstCallbacks, this, NULL);
	gst_app_sink_set_emit_signals(GST_APP_SINK(depthSink),0);

	// add elements to the pipeline and link them (but not yet to the rtpbin)
	gst_bin_add_many(GST_BIN(pipeline), dh264depay, dqueue, avdec_h264, vconvert, depthSink, NULL);
	if(!gst_element_link_many(dh264depay, dqueue, avdec_h264, vconvert, depthSink, NULL)){
		ofLogError(LOG_NAME) << "couldn't link depth elements";
	}
}

void ofxGstRTPClient::createOscChannel(string rtpCaps){

	oscSessionNumber = lastSessionNumber;
	lastSessionNumber++;


	// create and add osc elements and connect them to the correct pad.
	// osc pipeline to be connected to the corresponding recv_rtp_send pad:
	// rtpgstdepay ! appsink
	gstdepay = gst_element_factory_make("rtpgstdepay","rtpgstdepay_osc");
	oscSink = (GstAppSink*)gst_element_factory_make("appsink","oscsink");


	// set format for depth appsink to osc
	GstCaps * caps;

	caps = gst_caps_new_empty_simple("application/x-osc");

	if(!caps){
		ofLogError(LOG_NAME) << "couldn't get caps";
	}else{
		//caps = gst_caps_fixate(caps);
		gst_app_sink_set_caps(oscSink,caps);
		gst_caps_unref(caps);
	}


	// set callbacks to receive osc data
	GstAppSinkCallbacks gstCallbacks;
	gstCallbacks.eos = &on_eos_from_osc;
	gstCallbacks.new_preroll = &on_new_preroll_from_osc;
	gstCallbacks.new_sample = &on_new_buffer_from_osc;
	gst_app_sink_set_callbacks(GST_APP_SINK(oscSink), &gstCallbacks, this, NULL);
	gst_app_sink_set_emit_signals(GST_APP_SINK(oscSink),0);

	// add elements to the pipeline and link them (but not yet to the rtpbin)
	gst_bin_add_many(GST_BIN(pipeline), gstdepay, oscSink, NULL);
	if(!gst_element_link_many(gstdepay, GST_ELEMENT(oscSink), NULL)){
		ofLogError(LOG_NAME) << "couldn't link osc elements";
	}
}

void ofxGstRTPClient::addVideoChannel(int port, int w, int h, int fps){

	// the caps of the sender RTP stream.
	// FIXME: This is usually negotiated out of band with
	// SDP or RTSP. normally these caps will also include SPS and PPS but we don't
	// have that yet
	string vcaps="application/x-rtp,media=(string)video,clock-rate=(int)90000,payload=(int)96,encoding-name=(string)H264";

	createVideoChannel(vcaps, w,h,fps);

	GstElement * rtcpsink;
	NetworkElementsProperties properties;
	properties.capsstr = vcaps;
	properties.source = &vudpsrc;
	properties.rtpcsource = &vudpsrcrtcp;
	properties.rtpcsink = &rtcpsink;
	properties.port = port;
	properties.rtpcsrcport = port+1;
	properties.rtpcsinkport = port+3;
	properties.srcIP = src;
	properties.sessionNumber = videoSessionNumber;
	properties.sourceName = "vrtpsrc";
	properties.rtpcSourceName = "vrtcpsrc";
	properties.rtpcSinkName = "vrtcpsink";
	createNetworkElements(properties,NULL);

	/*GstPad * pad = gst_element_get_request_pad(rtpbin,("recv_rtp_src_"+ofToString(videoSessionNumber)).c_str());
	linkVideoPad(pad);*/
}

void ofxGstRTPClient::addDepthChannel(int port, int w, int h, int fps, bool depth16){

	// the caps of the sender RTP stream.
	// FIXME: This is usually negotiated out of band with
	// SDP or RTSP. normally these caps will also include SPS and PPS but we don't
	// have that yet
	string dcaps="application/x-rtp,media=(string)video,clock-rate=(int)90000,payload=(int)97,encoding-name=(string)H264";

	createDepthChannel(dcaps, w,h,fps,depth16);


	GstElement * rtcpsink;
	NetworkElementsProperties properties;
	properties.capsstr = dcaps;
	properties.source = &dudpsrc;
	properties.rtpcsource = &dudpsrcrtcp;
	properties.rtpcsink = &rtcpsink;
	properties.port = port;
	properties.rtpcsrcport = port+1;
	properties.rtpcsinkport = port+3;
	properties.srcIP = src;
	properties.sessionNumber = depthSessionNumber;
	properties.sourceName = "drtpsrc";
	properties.rtpcSourceName = "drtcpsrc";
	properties.rtpcSinkName = "drtcpsink";
	createNetworkElements(properties, NULL);

	/*GstPad * pad = gst_element_get_request_pad(rtpbin,("recv_rtp_src_"+ofToString(depthSessionNumber)).c_str());
	linkDepthPad(pad);*/
}

void ofxGstRTPClient::addAudioChannel(int port){

	// the caps of the sender RTP stream.
	// FIXME: This is usually negotiated out of band with
	// SDP or RTSP. normally these caps will also include SPS and PPS but we don't
	// have that yet
	string acaps="application/x-rtp,media=(string)audio,clock-rate=(int)48000,payload=(int)98,encoding-name=(string)X-GST-OPUS-DRAFT-SPITTKA-00";

	createAudioChannel(acaps);

	GstElement * rtcpsink;
	NetworkElementsProperties properties;
	properties.capsstr = acaps;
	properties.source = &audpsrc;
	properties.rtpcsource = &audpsrcrtcp;
	properties.rtpcsink = &rtcpsink;
	properties.port = port;
	properties.rtpcsrcport = port+1;
	properties.rtpcsinkport = port+3;
	properties.srcIP = src;
	properties.sessionNumber = audioSessionNumber;
	properties.sourceName = "artpsrc";
	properties.rtpcSourceName = "artcpsrc";
	properties.rtpcSinkName = "artcpsink";
	createNetworkElements(properties, NULL);

	/*GstPad * pad = gst_element_get_request_pad(rtpbin,("recv_rtp_src_"+ofToString(audioSessionNumber)).c_str());
	linkAudioPad(pad);*/
}

void ofxGstRTPClient::addOscChannel(int port){

	// the caps of the sender RTP stream.
	// FIXME: This is usually negotiated out of band with
	// SDP or RTSP. normally these caps will also include SPS and PPS but we don't
	// have that yet
	string ocaps="application/x-rtp,media=(string)application,clock-rate=(int)90000,payload=(int)99,encoding-name=(string)X-GST,caps=(string)\"YXBwbGljYXRpb24veC1vc2M\\=\"";

	createOscChannel(ocaps);

	GstElement * rtcpsink;
	NetworkElementsProperties properties;
	properties.capsstr = ocaps;
	properties.source = &oudpsrc;
	properties.rtpcsource = &oudpsrcrtcp;
	properties.rtpcsink = &rtcpsink;
	properties.port = port;
	properties.rtpcsrcport = port+1;
	properties.rtpcsinkport = port+3;
	properties.srcIP = src;
	properties.sessionNumber = oscSessionNumber;
	properties.sourceName = "ortpsrc";
	properties.rtpcSourceName = "ortcpsrc";
	properties.rtpcSinkName = "ortcpsink";
	createNetworkElements(properties,NULL);

	/*GstPad * pad = gst_element_get_request_pad(rtpbin,("recv_rtp_src_"+ofToString(oscSessionNumber)).c_str());
	linkOscPad(pad);*/
}

void ofxGstRTPClient::addVideoChannel(ofxNiceStream * niceStream, int w, int h, int fps){
	videoStream = niceStream;

	// the caps of the sender RTP stream.
	// FIXME: This is usually negotiated out of band with
	// SDP or RTSP. normally these caps will also include SPS and PPS but we don't
	// have that yet
	string vcaps="application/x-rtp,media=(string)video,clock-rate=(int)90000,payload=(int)96,encoding-name=(string)H264";

	createVideoChannel(vcaps, w,h,fps);

	GstElement * rtcpsink;
	NetworkElementsProperties properties;
	properties.capsstr = vcaps;
	properties.capsfiltername = "vcapsfilter";
	properties.source = &vudpsrc;
	properties.rtpcsource = &vudpsrcrtcp;
	properties.rtpcsink = &rtcpsink;
	properties.srcIP = src;
	properties.sessionNumber = videoSessionNumber;
	properties.sourceName = "vrtpsrc";
	properties.rtpcSourceName = "vrtcpsrc";
	properties.rtpcSinkName = "vrtcpsink";
	createNetworkElements(properties,niceStream);

	/*GstPad * pad = gst_element_get_request_pad(rtpbin,("recv_rtp_src_"+ofToString(videoSessionNumber)).c_str());
	linkVideoPad(pad);*/

}

void ofxGstRTPClient::addAudioChannel(ofxNiceStream * niceStream){
	audioStream = niceStream;

	// the caps of the sender RTP stream.
	// FIXME: This is usually negotiated out of band with
	// SDP or RTSP. normally these caps will also include SPS and PPS but we don't
	// have that yet
	string acaps="application/x-rtp,media=(string)audio,clock-rate=(int)48000,payload=(int)97,encoding-name=(string)X-GST-OPUS-DRAFT-SPITTKA-00";

	createAudioChannel(acaps);

	GstElement * rtcpsink;
	NetworkElementsProperties properties;
	properties.capsstr = acaps;
	properties.capsfiltername = "acapsfilter";
	properties.source = &audpsrc;
	properties.rtpcsource = &audpsrcrtcp;
	properties.rtpcsink = &rtcpsink;
	properties.srcIP = src;
	properties.sessionNumber = audioSessionNumber;
	properties.sourceName = "artpsrc";
	properties.rtpcSourceName = "artcpsrc";
	properties.rtpcSinkName = "artcpsink";
	createNetworkElements(properties, niceStream);

	/*GstPad * pad = gst_element_get_request_pad(rtpbin,("recv_rtp_src_"+ofToString(audioSessionNumber)).c_str());
	linkAudioPad(pad);*/
}

void ofxGstRTPClient::addDepthChannel(ofxNiceStream * niceStream, int w, int h, int fps, bool depth16){
	depthStream = niceStream;

	// the caps of the sender RTP stream.
	// FIXME: This is usually negotiated out of band with
	// SDP or RTSP. normally these caps will also include SPS and PPS but we don't
	// have that yet
	string dcaps="application/x-rtp,media=(string)video,clock-rate=(int)90000,payload=(int)98,encoding-name=(string)H264";

	createDepthChannel(dcaps,w,h,fps,depth16);

	GstElement * rtcpsink;
	NetworkElementsProperties properties;
	properties.capsstr = dcaps;
	properties.capsfiltername = "dcapsfilter";
	properties.source = &dudpsrc;
	properties.rtpcsource = &dudpsrcrtcp;
	properties.rtpcsink = &rtcpsink;
	properties.srcIP = src;
	properties.sessionNumber = depthSessionNumber;
	properties.sourceName = "drtpsrc";
	properties.rtpcSourceName = "drtcpsrc";
	properties.rtpcSinkName = "drtcpsink";
	createNetworkElements(properties, niceStream);

	/*GstPad * pad = gst_element_get_request_pad(rtpbin,("recv_rtp_src_"+ofToString(depthSessionNumber)).c_str());
	linkDepthPad(pad);*/

}

void ofxGstRTPClient::addOscChannel(ofxNiceStream * niceStream){
	oscStream = niceStream;

	// the caps of the sender RTP stream.
	// FIXME: This is usually negotiated out of band with
	// SDP or RTSP. normally these caps will also include SPS and PPS but we don't
	// have that yet
	string ocaps="application/x-rtp,media=(string)application,clock-rate=(int)90000,payload=(int)99,encoding-name=(string)X-GST,caps=(string)\"YXBwbGljYXRpb24veC1vc2M\\=\"";


	createOscChannel(ocaps);


	GstElement * rtcpsink;
	NetworkElementsProperties properties;
	properties.capsstr = ocaps;
	properties.source = &oudpsrc;
	properties.capsfiltername = "ocapsfilter";
	properties.rtpcsource = &oudpsrcrtcp;
	properties.rtpcsink = &rtcpsink;
	properties.srcIP = src;
	properties.sessionNumber = oscSessionNumber;
	properties.sourceName = "ortpsrc";
	properties.rtpcSourceName = "ortcpsrc";
	properties.rtpcSinkName = "ortcpsink";
	createNetworkElements(properties,niceStream);

	/*GstPad * pad = gst_element_get_request_pad(rtpbin,("recv_rtp_src_"+ofToString(oscSessionNumber)).c_str());
	linkOscPad(pad);*/

}

void ofxGstRTPClient::setup(string srcIP, int latency){
	this->src = srcIP;
	this->latency = latency;

	pipeline = gst_pipeline_new("rtpclientpipeline");
	rtpbin	 = gst_element_factory_make("rtpbin","rtpbin");
	g_object_set(rtpbin,"latency",latency,NULL);

	if(!gst_bin_add(GST_BIN(pipeline),rtpbin)){
		ofLogError() << "couldn't add rtpbin to pipeline";
	}

	// set this instance as listener to receive messages from the pipeline
	gst.setSinkListener(this);

	doubleBufferVideo.setup(640,480,3);
	if(depth16){
		doubleBufferDepth.setup(640,480,3);
		depth16Pixels.allocate(640,480,1);
	}else{
		doubleBufferDepth.setup(640,480,1);
	}
}

void ofxGstRTPClient::setup(int latency){
	setup("",latency);
}


void ofxGstRTPClient::play(){
	// pass the pipeline to ofGstVideoUtils so it starts it and allocates the needed resources
	gst.setPipelineWithSink(pipeline,NULL,true);

	// connect callback to the on-ssrc-active signal
	g_signal_connect(gst.getGstElementByName("rtpbin"),"pad-added", G_CALLBACK(&ofxGstRTPClient::on_pad_added),this);
	g_signal_connect(gst.getGstElementByName("rtpbin"),"on-ssrc-active",G_CALLBACK(&ofxGstRTPClient::on_ssrc_active_handler),this);
	g_signal_connect(gst.getGstElementByName("rtpbin"),"on-bye-ssrc",G_CALLBACK(&ofxGstRTPClient::on_bye_ssrc_handler),this);
	g_signal_connect(gst.getGstElementByName("rtpbin"),"on-new-ssrc",G_CALLBACK(&ofxGstRTPClient::on_new_ssrc_handler),this);


	gst.startPipeline();

	gst.play();
}


void ofxGstRTPClient::update(){
	doubleBufferVideo.update();
	doubleBufferDepth.update();
	doubleBufferOsc.update();
	if(depth16 && doubleBufferDepth.isFrameNew()){
		ofxGstRTPUtils::convertColoredDepthToShort(doubleBufferDepth.getPixels(),depth16Pixels,pow(2.f,14.f));
	}
}


bool ofxGstRTPClient::isFrameNewVideo(){
	return doubleBufferVideo.isFrameNew();
}


bool ofxGstRTPClient::isFrameNewDepth(){
	return doubleBufferDepth.isFrameNew();
}

bool ofxGstRTPClient::isFrameNewOsc(){
	return doubleBufferOsc.isFrameNew();
}

ofPixels & ofxGstRTPClient::getPixelsVideo(){
	return doubleBufferVideo.getPixels();
}


ofPixels & ofxGstRTPClient::getPixelsDepth(){
	return doubleBufferDepth.getPixels();
}

ofShortPixels & ofxGstRTPClient::getPixelsDepth16(){
	return depth16Pixels;
}


void appendMessage(ofxOscMessage & ofMessage, osc::ReceivedMessage & m){
	// set the address
	ofMessage.setAddress( m.AddressPattern() );

	// transfer the arguments
	for ( osc::ReceivedMessage::const_iterator arg = m.ArgumentsBegin();
		  arg != m.ArgumentsEnd();
		  ++arg )
	{
		if ( arg->IsInt32() )
			ofMessage.addIntArg( arg->AsInt32Unchecked() );
		else if ( arg->IsInt64() )
			ofMessage.addInt64Arg( arg->AsInt64Unchecked() );
		else if ( arg->IsFloat() )
			ofMessage.addFloatArg( arg->AsFloatUnchecked() );
		else if ( arg->IsString() )
			ofMessage.addStringArg( arg->AsStringUnchecked() );
		else
		{
			ofLogError("ofxOscReceiver") << "ProcessMessage: argument in message " << m.AddressPattern() << " is not an int, float, or string";
		}
	}
}

void appendBundle(ofxOscMessage & ofMessage, osc::ReceivedBundle & b){
	for(osc::ReceivedBundleElementIterator i=b.ElementsBegin();i!=b.ElementsEnd();i++){
		if(i->IsMessage()){
			osc::ReceivedPacket p(i->Contents(),i->Size());
			osc::ReceivedMessage m(p);
			appendMessage(ofMessage,m);
		}else if(i->IsBundle()){
			osc::ReceivedPacket p(i->Contents(),i->Size());
			osc::ReceivedBundle b(p);
			appendBundle(ofMessage,b);
		}
	}
}

ofxOscMessage ofxGstRTPClient::getOscMessage(){

	osc::ReceivedPacket * packet = doubleBufferOsc.getOscReceivedPacket();

	ofxOscMessage ofMessage;

	if(packet==NULL)
	{
		ofLogError() << "packet NULL ";
		return ofMessage;
	}

	if(packet->IsMessage()){
		try{
			osc::ReceivedMessage m(*packet);
			appendMessage(ofMessage,m);
		}catch(osc::MalformedMessageException & e){
		}
	}else if(packet->IsBundle()){
		try{
			osc::ReceivedBundle b(*packet);
			appendBundle(ofMessage,b);
		}catch(osc::MalformedBundleException & e){
		}
	}else{
		ofLogError() << "received packet of type != message not supported yet";
	}

	return ofMessage;
}

bool ofxGstRTPClient::on_message(GstMessage * msg){
	// read messages from the pipeline like dropped packages
	switch (GST_MESSAGE_TYPE (msg)) {
	case GST_MESSAGE_ELEMENT:{
		GstObject * messageSrc = GST_MESSAGE_SRC(msg);
		ofLogVerbose(LOG_NAME) << "Got " << GST_MESSAGE_TYPE_NAME(msg) << " message from " << GST_MESSAGE_SRC_NAME(msg);
		ofLogVerbose(LOG_NAME) << "Message source type: " << G_OBJECT_CLASS_NAME(G_OBJECT_GET_CLASS(messageSrc));
		if(string(G_OBJECT_CLASS_NAME(G_OBJECT_GET_CLASS(messageSrc)))=="GstRtpSession"){
			ofLogVerbose(LOG_NAME) << "message from " <<  G_OBJECT_CLASS_NAME(G_OBJECT_GET_CLASS(messageSrc));

			//gst_element_get_request_pad(rtpbin,"recv_rtp_src_"+ofToString(session)+"_"+ofToString(ssrc)+"_"+ofToString(pt));
		}
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
		return false;
	}
}


GstFlowReturn ofxGstRTPClient::on_preroll(GstSample * buffer){
	// shouldn't happen on a live pipeline
	return GST_FLOW_OK;
}


GstFlowReturn ofxGstRTPClient::on_buffer(GstSample * buffer){
	// we don't need to do anything by now when there's a new buffer
	// ofGstVideoUtils already manages it and puts the data in it's internal
	// ofPixels
	return GST_FLOW_OK;
}


void ofxGstRTPClient::on_stream_prepared(){
	// when we receive the first buffer allocate ofGstVideoUtils with the
	// correct sizes
	//gst.allocate(width,height,24);
};


void ofxGstRTPClient::on_eos_from_video(GstAppSink * elt, void * rtpClient){

}


GstFlowReturn ofxGstRTPClient::on_new_preroll_from_video(GstAppSink * elt, void * rtpClient){
	return GST_FLOW_OK;
}


GstFlowReturn ofxGstRTPClient::on_new_buffer_from_video(GstAppSink * elt, void * data){
	ofxGstRTPClient* rtpClient = (ofxGstRTPClient*) data;
	return rtpClient->on_new_buffer_from_video(elt);
}


GstFlowReturn ofxGstRTPClient::on_new_buffer_from_video(GstAppSink * elt){
	GstSample *sample = gst_app_sink_pull_sample (GST_APP_SINK (elt));
	doubleBufferVideo.newSample(sample);
	return GST_FLOW_OK;
}


void ofxGstRTPClient::on_eos_from_depth(GstAppSink * elt, void * rtpClient){

}


GstFlowReturn ofxGstRTPClient::on_new_preroll_from_depth(GstAppSink * elt, void * rtpClient){
	return GST_FLOW_OK;
}


GstFlowReturn ofxGstRTPClient::on_new_buffer_from_depth(GstAppSink * elt, void * data){
	ofxGstRTPClient* rtpClient = (ofxGstRTPClient*) data;
	return rtpClient->on_new_buffer_from_depth(elt);
}

GstFlowReturn ofxGstRTPClient::on_new_buffer_from_depth(GstAppSink * elt){
	GstSample *sample = gst_app_sink_pull_sample (GST_APP_SINK (elt));
	doubleBufferDepth.newSample(sample);
	return GST_FLOW_OK;
}


void ofxGstRTPClient::on_eos_from_osc(GstAppSink * elt, void * rtpClient){

}


GstFlowReturn ofxGstRTPClient::on_new_preroll_from_osc(GstAppSink * elt, void * rtpClient){
	return GST_FLOW_OK;
}


GstFlowReturn ofxGstRTPClient::on_new_buffer_from_osc(GstAppSink * elt, void * data){
	ofxGstRTPClient* rtpClient = (ofxGstRTPClient*) data;
	return rtpClient->on_new_buffer_from_osc(elt);
}

GstFlowReturn ofxGstRTPClient::on_new_buffer_from_osc(GstAppSink * elt){
	GstSample *sample = gst_app_sink_pull_sample (GST_APP_SINK (elt));
	doubleBufferOsc.newSample(sample);
	return GST_FLOW_OK;
}
