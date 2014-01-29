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

#define RTPBIN_MAX_LATENCY 2000
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
,vqueue(0)
,dqueue(0)
,vudpsrc(0)
,audpsrc(0)
,dudpsrc(0)
,oudpsrc(0)
,vudpsrcrtcp(0)
,audpsrcrtcp(0)
,dudpsrcrtcp(0)
,oudpsrcrtcp(0)
,depth16(false)
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

#if ENABLE_NAT_TRANSVERSAL
,videoStream(NULL)
,depthStream(NULL)
,oscStream(NULL)
,audioStream(NULL)
#endif

#if ENABLE_ECHO_CANCEL
,audioChannelReady(false)
,echoCancel(NULL)
,prevTimestampAudio(0)
,numFrameAudio(0)
,firstAudioFrame(true)
,prevAudioBuffer(0)
,audioFramesProcessed(0)
#endif

,audioechosrc(NULL)
{
	GstMapInfo initMapinfo		= {0,};
	mapinfo = initMapinfo;

	latency.set("latency",200,0,RTPBIN_MAX_LATENCY);
	latency.addListener(this,&ofxGstRTPClient::latencyChanged);
	drop.set("drop",false);
	drop.addListener(this,&ofxGstRTPClient::dropChanged);
	parameters.setName("gst rtp client");
	parameters.add(latency);
	parameters.add(drop);
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
	GstPad * sinkPad = gst_element_get_static_pad(opusdepay,"sink");
	if(sinkPad){
		if(gst_pad_link(pad,sinkPad)!=GST_PAD_LINK_OK){
			ofLogError(LOG_NAME) << "couldn't link rtp source pad to audio depay";
		}else{
			ofLogVerbose(LOG_NAME) << "audio pipeline complete!";
			audioReady = true;
		}
	}else{
		ofLogError(LOG_NAME) << "couldn't get sink pad for opus depay";
	}
}

void ofxGstRTPClient::linkVideoPad(GstPad * pad){
	GstPad * sinkPad = gst_element_get_static_pad(vh264depay,"sink");
	if(sinkPad){
		if(gst_pad_link(pad,sinkPad)!=GST_PAD_LINK_OK){
			ofLogError(LOG_NAME) << "couldn't link rtp source pad to video depay";
		}else{
			ofLogVerbose(LOG_NAME) << "video pipeline complete!";
			videoReady = true;

			int currentLatency = latency;
			latencyChanged(currentLatency);
		}
	}else{
		ofLogError(LOG_NAME) << "couldn't get sink pad for h264 depay";
	}
}

void ofxGstRTPClient::linkDepthPad(GstPad * pad){
	GstPad * sinkPad = gst_element_get_static_pad(dh264depay,"sink");
	if(sinkPad){
		if(gst_pad_link(pad,sinkPad)!=GST_PAD_LINK_OK){
			ofLogError(LOG_NAME) << "couldn't link rtp source pad to depth depay";
		}else{
			ofLogVerbose(LOG_NAME) << "depth pipeline complete!";
			depthReady = true;

			int currentLatency = latency;
			latencyChanged(currentLatency);
		}
	}else{
		ofLogError(LOG_NAME) << "couldn't get sink pad for h264 depay";
	}
}


void ofxGstRTPClient::linkOscPad(GstPad * pad){
	GstPad * sinkPad = gst_element_get_static_pad(gstdepay,"sink");
	if(sinkPad){
		if(gst_pad_link(pad,sinkPad)!=GST_PAD_LINK_OK){
			ofLogError(LOG_NAME) << "couldn't link rtp source pad to osc depay";
		}else{
			ofLogVerbose(LOG_NAME) << "osc pipeline complete!";
			oscReady = true;
		}
	}else{
		ofLogError(LOG_NAME) << "couldn't get sink pad for osc depay";
	}
}

void ofxGstRTPClient::on_bye_ssrc_handler(GstBin *rtpbin, guint session, guint ssrc, ofxGstRTPClient * rtpClient){
	ofLogVerbose(LOG_NAME) << "client disconnected";

}

#if ENABLE_NAT_TRANSVERSAL
void ofxGstRTPClient::createNetworkElements(NetworkElementsProperties properties, ofxNiceStream * niceStream){
#else
void ofxGstRTPClient::createNetworkElements(NetworkElementsProperties properties, void *){
#endif

	// create network elements for the stream and add them to the pipeline
	GstCaps * caps = gst_caps_from_string(properties.capsstr.c_str());

#if ENABLE_NAT_TRANSVERSAL
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
	}else
#endif
	{
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

	// create src for rtpc
#if ENABLE_NAT_TRANSVERSAL
	if(niceStream){
		*properties.rtpcsource = gst_element_factory_make("nicesrc",properties.rtpcSourceName.c_str());

		if(!*properties.rtpcsource){
			ofLogError(LOG_NAME) << "couldn't create rtcp nicesrc";
		}

		g_object_set(G_OBJECT(*properties.rtpcsource),"agent",niceStream->getAgent(),"stream",niceStream->getStreamID(),"component",2,NULL);
	}else
#endif
	{
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
#if ENABLE_NAT_TRANSVERSAL
	if(niceStream){
		*properties.rtpcsink = gst_element_factory_make("nicesink",properties.rtpcSinkName.c_str());

		if(!*properties.rtpcsink){
			ofLogError(LOG_NAME) << "couldn't create rtcp nicesink";
		}

		g_object_set(G_OBJECT(*properties.rtpcsink),"agent",niceStream->getAgent(),"stream",niceStream->getStreamID(),"component",3,NULL);
	}else
#endif
	{
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


void ofxGstRTPClient::createVideoChannel(string rtpCaps){
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

	GstElement * avdec_h264 = gst_element_factory_make("avdec_h264","avdec_h264_video");
	GstElement * vconvert = gst_element_factory_make("videoconvert","vconvert");
	videoSink = (GstAppSink*)gst_element_factory_make("appsink","videosink");

	// set format for video appsink to rgb
	GstCaps * caps = NULL;
	caps = gst_caps_new_simple("video/x-raw",
					"format",G_TYPE_STRING,"RGB",
					NULL);

	if(!caps){
		ofLogError(LOG_NAME) << "couldn't get caps";
	}else{
		gst_app_sink_set_caps(videoSink,caps);
		gst_caps_unref(caps);
	}

	// set callbacks to receive rgb data
	GstAppSinkCallbacks gstCallbacks;
	gstCallbacks.eos = &ofxGstRTPClient::on_eos_from_video;
	gstCallbacks.new_preroll = &ofxGstRTPClient::on_new_preroll_from_video;
	gstCallbacks.new_sample = &ofxGstRTPClient::on_new_buffer_from_video;
	gst_app_sink_set_callbacks(GST_APP_SINK(videoSink), &gstCallbacks, this, NULL);
	gst_app_sink_set_emit_signals(GST_APP_SINK(videoSink),0);

	// add elements to the pipeline and link them (but not yet to the rtpbin)
	gst_bin_add_many(GST_BIN(pipeline), vh264depay, avdec_h264, vconvert, videoSink, NULL);
	if(!gst_element_link_many(vh264depay, avdec_h264, vconvert, videoSink, NULL)){
		ofLogError(LOG_NAME) << "couldn't link video elements";
	}
}

void ofxGstRTPClient::createAudioChannel(string rtpCaps){
	audioSessionNumber = lastSessionNumber;
	lastSessionNumber++;

	// create and add audio elements and connect them to the correct pad.
	// audio pipeline to be connected to the corresponding recv_rtp_send pad:
	// Linux:
	// rtpopusdepay ! opusdec ! audioconvert ! audioresample ! pulsesink stream-properties=\"props,media.role=phone,filter.want=echo-cancel\"
	// everything else:
	// rtpopusdepay ! opusdec ! audioconvert ! audioresample ! autoaudiosink

	opusdepay = gst_element_factory_make("rtpopusdepay","rtpopusdepay1");
	GstElement * opusdec = gst_element_factory_make("opusdec","opusdec1");
	GstElement * audioconvert = gst_element_factory_make("audioconvert","audioconvert1");
	GstElement * audioresample = gst_element_factory_make("audioresample","audioresample1");
#if ENABLE_ECHO_CANCEL
	GstElement * audioconvert2;
	GstElement * audioresample2;
	if(echoCancel){
		audioconvert2 = gst_element_factory_make("audioconvert","audioconvert2");
		audioresample2 = gst_element_factory_make("audioresample","audioresample2");

		audioechosink = gst_element_factory_make("appsink","audioechosink");

		audioechosrc = gst_element_factory_make("appsrc","audioechosrc");
		g_object_set(audioechosrc,"is-live",1,"format",GST_FORMAT_TIME,NULL);

		// set format for video appsink to rgb
		GstCaps * caps = NULL;
		caps = gst_caps_new_simple("audio/x-raw",
						"format",G_TYPE_STRING,"S16LE",
						"rate",G_TYPE_INT,32000,
						"channels", G_TYPE_INT,2,
						"layout",G_TYPE_STRING,"interleaved",
						//"channel-mask",GST_TYPE_BITMASK,0x0000000000000003,
						NULL);

		if(!caps){
			ofLogError(LOG_NAME) << "couldn't get caps";
		}else{
			gst_app_sink_set_caps(GST_APP_SINK(audioechosink),caps);
			gst_app_src_set_caps(GST_APP_SRC(audioechosrc),caps);

		}
		gst_caps_unref(caps);
	}
#endif

#ifdef TARGET_LINUX
	GstElement * audiosink = gst_element_factory_make("pulsesink","pulsesink1");
	GstStructure * pulseProperties;
	#if ENABLE_ECHO_CANCEL
		if(echoCancel){
			pulseProperties = gst_structure_new("props","media.role",G_TYPE_STRING,"phone",NULL);
		}else
	#endif
		pulseProperties = gst_structure_new("props","media.role",G_TYPE_STRING,"phone","filter.want",G_TYPE_STRING,"echo-cancel",NULL);

	g_object_set(audiosink,"stream-properties",pulseProperties,NULL);
#else
	GstElement * audiosink = gst_element_factory_make("autoaudiosink","autoaudiosink1");
#endif

#if ENABLE_ECHO_CANCEL
	if(echoCancel){
		GstAppSinkCallbacks gstCallbacks;
		gstCallbacks.eos = &on_eos_from_audio;
		gstCallbacks.new_preroll = &on_new_preroll_from_audio;
		gstCallbacks.new_sample = &on_new_buffer_from_audio;
		gst_app_sink_set_callbacks(GST_APP_SINK(audioechosink), &gstCallbacks, this, NULL);
		gst_app_sink_set_emit_signals(GST_APP_SINK(audioechosink),0);

		// add elements to the pipeline and link them (but not yet to the rtpbin)
		gst_bin_add_many(GST_BIN(pipeline), opusdepay, opusdec, audioconvert, audioresample, audioechosink, NULL);
		if(!gst_element_link_many(opusdepay, opusdec, audioconvert, audioresample, audioechosink, NULL)){
			ofLogError(LOG_NAME) << "couldn't link audio elements";
		}

		gst_bin_add_many(GST_BIN(pipelineAudioOut), audioechosrc, audioconvert2, audioresample2, audiosink, NULL);
		if(!gst_element_link_many(audioechosrc, audiosink, NULL)){
			ofLogError(LOG_NAME) << "couldn't link audio elements";
		}
		audioChannelReady = true;
	}else
#endif
	{
		// add elements to the pipeline and link them (but not yet to the rtpbin)
		gst_bin_add_many(GST_BIN(pipeline), opusdepay, opusdec, audioconvert, audioresample, audiosink, NULL);
		if(!gst_element_link_many(opusdepay, opusdec, audioconvert, audioresample, audiosink, NULL)){
			ofLogError(LOG_NAME) << "couldn't link audio elements";
		}
	}


}

void ofxGstRTPClient::createDepthChannel(string rtpCaps, bool depth16){
	depthSessionNumber = lastSessionNumber;
	lastSessionNumber++;

	this->depth16 = depth16;


	// create and add depth elements and connect them to the correct pad.
	// depth pipeline to be connected to the corresponding recv_rtp_send pad:
	// rtph264depay ! avdec_h264 ! videoconvert ! appsink
	dh264depay = gst_element_factory_make("rtph264depay","rtph264depay_depth");

	GstElement * avdec_h264 = gst_element_factory_make("avdec_h264","avdec_h264_depth");
	GstElement * vconvert = gst_element_factory_make("videoconvert","dconvert");
	depthSink = (GstAppSink*)gst_element_factory_make("appsink","depthsink");

	// set format for depth appsink to gray 8bits
	GstCaps * caps;

	if(depth16){
		caps = gst_caps_new_simple("video/x-raw",
				"format",G_TYPE_STRING,"RGB",
				NULL);
	}else{
		caps = gst_caps_new_simple("video/x-raw",
				"format",G_TYPE_STRING,"GRAY8",
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
	gstCallbacks.eos = &ofxGstRTPClient::on_eos_from_depth;
	gstCallbacks.new_preroll = &ofxGstRTPClient::on_new_preroll_from_depth;
	gstCallbacks.new_sample = &ofxGstRTPClient::on_new_buffer_from_depth;
	gst_app_sink_set_callbacks(GST_APP_SINK(depthSink), &gstCallbacks, this, NULL);
	gst_app_sink_set_emit_signals(GST_APP_SINK(depthSink),0);

	// add elements to the pipeline and link them (but not yet to the rtpbin)
	gst_bin_add_many(GST_BIN(pipeline), dh264depay, avdec_h264, vconvert, depthSink, NULL);
	if(!gst_element_link_many(dh264depay, avdec_h264, vconvert, depthSink, NULL)){
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
		gst_app_sink_set_caps(oscSink,caps);
		gst_caps_unref(caps);
	}


	// set callbacks to receive osc data
	GstAppSinkCallbacks gstCallbacks;
	gstCallbacks.eos = &ofxGstRTPClient::on_eos_from_osc;
	gstCallbacks.new_preroll = &ofxGstRTPClient::on_new_preroll_from_osc;
	gstCallbacks.new_sample = &ofxGstRTPClient::on_new_buffer_from_osc;
	gst_app_sink_set_callbacks(GST_APP_SINK(oscSink), &gstCallbacks, this, NULL);
	gst_app_sink_set_emit_signals(GST_APP_SINK(oscSink),0);

	// add elements to the pipeline and link them (but not yet to the rtpbin)
	gst_bin_add_many(GST_BIN(pipeline), gstdepay, oscSink, NULL);
	if(!gst_element_link_many(gstdepay, GST_ELEMENT(oscSink), NULL)){
		ofLogError(LOG_NAME) << "couldn't link osc elements";
	}
}

void ofxGstRTPClient::addVideoChannel(int port){

	// the caps of the sender RTP stream.
	// FIXME: This is usually negotiated out of band with
	// SDP or RTSP. normally these caps will also include SPS and PPS but we don't
	// have that yet
	string vcaps="application/x-rtp,media=(string)video,clock-rate=(int)90000,payload=(int)96,encoding-name=(string)H264,rtcp-fb-nack-pli=(int)1";

	createVideoChannel(vcaps);

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

}

void ofxGstRTPClient::addDepthChannel(int port, bool depth16){

	// the caps of the sender RTP stream.
	// FIXME: This is usually negotiated out of band with
	// SDP or RTSP. normally these caps will also include SPS and PPS but we don't
	// have that yet
	string dcaps="application/x-rtp,media=(string)video,clock-rate=(int)90000,payload=(int)97,encoding-name=(string)H264,rtcp-fb-nack-pli=(int)1";

	createDepthChannel(dcaps,depth16);


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

}

#if ENABLE_NAT_TRANSVERSAL
void ofxGstRTPClient::addVideoChannel(ofxNiceStream * niceStream){
	videoStream = niceStream;

	// the caps of the sender RTP stream.
	// FIXME: This is usually negotiated out of band with
	// SDP or RTSP. normally these caps will also include SPS and PPS but we don't
	// have that yet
	string vcaps="application/x-rtp,media=(string)video,clock-rate=(int)90000,payload=(int)96,encoding-name=(string)H264,rtcp-fb-nack-pli=(int)1 ";

	createVideoChannel(vcaps);

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
}

void ofxGstRTPClient::addDepthChannel(ofxNiceStream * niceStream, bool depth16){
	depthStream = niceStream;

	// the caps of the sender RTP stream.
	// FIXME: This is usually negotiated out of band with
	// SDP or RTSP. normally these caps will also include SPS and PPS but we don't
	// have that yet
	string dcaps="application/x-rtp,media=(string)video,clock-rate=(int)90000,payload=(int)98,encoding-name=(string)H264,rtcp-fb-nack-pli=(int)1 ";

	createDepthChannel(dcaps,depth16);

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
}

void ofxGstRTPClient::setup(int latency){
	setup("",latency);
}
#endif

#if ENABLE_ECHO_CANCEL
void ofxGstRTPClient::setEchoCancel(ofxEchoCancel & echoCancel){
	if(audioChannelReady){
		ofLogError(LOG_NAME) << "trying to add echo cancel module after audio channel setup";
	}else{
		this->echoCancel = &echoCancel;
	}
}
#endif

void ofxGstRTPClient::setup(string srcIP, int latency){
	this->src = srcIP;
	this->latency = latency;

	pipeline = gst_pipeline_new("rtpclientpipeline");
	if(!pipeline){
        ofLogError() << "couldn't create pipeline";
	}
	gst.setSinkListener(this);

#if ENABLE_ECHO_CANCEL
	if(echoCancel){
		pipelineAudioOut = gst_pipeline_new("audioclientpipeline");
		gstAudioOut.setSinkListener(this);
	}
#endif

	rtpbin	 = gst_element_factory_make("rtpbin","rtpbinclient");
	if(!rtpbin){
        ofLogError() << "couldn't create rtpbin";
	}
	g_object_set(rtpbin,"latency",RTPBIN_MAX_LATENCY,NULL);
	g_object_set(rtpbin,"drop-on-latency",(bool)drop,NULL);
	g_object_set(rtpbin,"do-lost",TRUE,NULL);

	if(!gst_bin_add(GST_BIN(pipeline),rtpbin)){
		ofLogError() << "couldn't add rtpbin to pipeline";
	}
}

void ofxGstRTPClient::close(){
	gst.close();
#if ENABLE_ECHO_CANCEL
	if(echoCancel){
		gstAudioOut.close();
	}
#endif

	width = 0;
	height = 0;
	pipeline = 0;
	rtpbin = 0;
	vh264depay = 0;
	opusdepay = 0;
	dh264depay = 0;
	gstdepay = 0;
	videoSink = 0;
	depthSink = 0;
	oscSink = 0;
	vqueue = 0;
	dqueue = 0;
	vudpsrc = 0;
	audpsrc = 0;
	dudpsrc = 0;
	oudpsrc = 0;
	vudpsrcrtcp = 0;
	audpsrcrtcp = 0;
	dudpsrcrtcp = 0;
	oudpsrcrtcp = 0;
	depth16 = false;
	videoSessionNumber = -1;
	audioSessionNumber = -1;
	depthSessionNumber = -1;
	oscSessionNumber = -1;
	videoSSRC = 0;
	audioSSRC = 0;
	depthSSRC = 0;
	oscSSRC = 0;
	videoReady = false;
	audioReady = false;
	depthReady = false;
	oscReady = false;
	lastSessionNumber = 0;
#if ENABLE_NAT_TRANSVERSAL
	videoStream = NULL;
	depthStream = NULL;
	oscStream = NULL;
	audioStream = NULL;
#endif
}

void ofxGstRTPClient::requestKeyFrame(){
	GstClock * clock = gst_pipeline_get_clock(GST_PIPELINE(gst.getPipeline()));
	gst_object_ref(clock);
	GstClockTime time = gst_clock_get_time (clock);
	GstClockTime now =  time - gst_element_get_base_time(gst.getPipeline());
	gst_object_unref (clock);
	GstEvent * keyFrameEvent = gst_video_event_new_upstream_force_key_unit(now,
															 TRUE,
															 0);
	gst_element_send_event(gst.getPipeline(),keyFrameEvent);
}

void ofxGstRTPClient::latencyChanged(int & latency){
	if(gst.isLoaded()){
		g_object_set(rtpbin,"latency",latency,NULL);
		if(gst.isPlaying()){
			gst_element_set_state(gst.getPipeline(),GST_STATE_PLAYING);
			requestKeyFrame();
			g_signal_emit_by_name(rtpbin,"reset-sync",NULL);
		}
	}
}

void ofxGstRTPClient::dropChanged(bool & drop){
	g_object_set(rtpbin,"drop-on-latency",drop,NULL);
}

void ofxGstRTPClient::play(){
	// pass the pipeline to ofGstVideoUtils so it starts it and allocates the needed resources
	gst.setPipelineWithSink(pipeline,NULL,true);
#if ENABLE_ECHO_CANCEL
	gstAudioOut.setPipelineWithSink(pipelineAudioOut,NULL,true);
#endif

	// connect callback to the on-ssrc-active signal
	g_signal_connect(gst.getGstElementByName("rtpbinclient"),"pad-added", G_CALLBACK(&ofxGstRTPClient::on_pad_added),this);
	g_signal_connect(gst.getGstElementByName("rtpbinclient"),"on-ssrc-active",G_CALLBACK(&ofxGstRTPClient::on_ssrc_active_handler),this);
	g_signal_connect(gst.getGstElementByName("rtpbinclient"),"on-bye-ssrc",G_CALLBACK(&ofxGstRTPClient::on_bye_ssrc_handler),this);
	g_signal_connect(gst.getGstElementByName("rtpbinclient"),"on-new-ssrc",G_CALLBACK(&ofxGstRTPClient::on_new_ssrc_handler),this);

	if(audioechosrc){
		gst_app_src_set_stream_type((GstAppSrc*)audioechosrc,GST_APP_STREAM_TYPE_STREAM);
	}

#if ENABLE_ECHO_CANCEL
	if(echoCancel){
		gstAudioOut.startPipeline();
		gstAudioOut.play();
	}
#endif

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

#if ENABLE_ECHO_CANCEL
u_int64_t ofxGstRTPClient::getAudioOutLatencyMs(){
	return gstAudioOut.getMinLatencyNanos()*0.000001;
}

u_int64_t ofxGstRTPClient::getAudioFramesProcessed(){
	return audioFramesProcessed;
}
#endif

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
	if(!doubleBufferVideo.isAllocated()){
		GstCaps * sampleCaps = gst_sample_get_caps(sample);
		if(sampleCaps){
			GstVideoInfo sampleInfo;
			if(gst_video_info_from_caps(&sampleInfo,sampleCaps)){
				doubleBufferVideo.setup( sampleInfo.width , sampleInfo.height , 3);
			}
		}
	}
	if(doubleBufferVideo.isAllocated()){
		doubleBufferVideo.newSample(sample);
	}
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

	if(!doubleBufferDepth.isAllocated()){
		GstCaps * sampleCaps = gst_sample_get_caps(sample);
		if(sampleCaps){
			GstVideoInfo sampleInfo;
			if(gst_video_info_from_caps(&sampleInfo,sampleCaps)){
				if(depth16){
					doubleBufferDepth.setup(sampleInfo.width , sampleInfo.height,3);
					depth16Pixels.allocate(sampleInfo.width , sampleInfo.height,1);
				}else{
					doubleBufferDepth.setup(sampleInfo.width , sampleInfo.height,1);
				}
			}
		}
	}

	if(doubleBufferDepth.isAllocated()){
		doubleBufferDepth.newSample(sample);
	}
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



#if ENABLE_ECHO_CANCEL
void ofxGstRTPClient::on_eos_from_audio(GstAppSink * elt, void * rtpClient){

}


GstFlowReturn ofxGstRTPClient::on_new_preroll_from_audio(GstAppSink * elt, void * rtpClient){
	return GST_FLOW_OK;
}

void ofxGstRTPClient::sendAudioOut(PooledAudioFrame * pooledFrame){
	// TODO: the echo appsrc doesn't seem to sync when latency is changed so we need
	// to generate the timestamps for the audio out to compensate for latency - max_latency
	if(firstAudioFrame){
		GstClock * clock = gst_pipeline_get_clock(GST_PIPELINE(gst.getPipeline()));
		gst_object_ref(clock);
		prevTimestampAudio = (gint64)gst_clock_get_time (clock) - (gint64)gst_element_get_base_time(gst.getPipeline());
		gst_object_unref (clock);
		firstAudioFrame = false;
	}
	int size = pooledFrame->audioFrame._payloadDataLengthInSamples*2*pooledFrame->audioFrame._audioChannel;

	GstBuffer * echoCancelledBuffer = gst_buffer_new_wrapped_full(GST_MEMORY_FLAG_READONLY,(void*)pooledFrame->audioFrame._payloadData,size,0,size,pooledFrame,(GDestroyNotify)&ofxWebRTCAudioPool::relaseFrame);

	GstClockTime duration = (pooledFrame->audioFrame._payloadDataLengthInSamples * GST_SECOND / pooledFrame->audioFrame._frequencyInHz);
	GstClockTime now = prevTimestampAudio;

	GST_BUFFER_OFFSET(echoCancelledBuffer) = numFrameAudio++;
	GST_BUFFER_OFFSET_END(echoCancelledBuffer) = numFrameAudio;
	GST_BUFFER_DTS (echoCancelledBuffer) = max((gint64)now + (gint64)((latency-RTPBIN_MAX_LATENCY)*GST_MSECOND),(gint64)0);
	GST_BUFFER_PTS (echoCancelledBuffer) = max((gint64)now + (gint64)((latency-RTPBIN_MAX_LATENCY)*GST_MSECOND),(gint64)0);
	GST_BUFFER_DURATION(echoCancelledBuffer) = duration;
	prevTimestampAudio = now+duration;


	GstFlowReturn flow_return = gst_app_src_push_buffer((GstAppSrc*)audioechosrc, echoCancelledBuffer);
	if (flow_return != GST_FLOW_OK) {
		ofLogError(LOG_NAME) << "error pushing audio buffer: flow_return was " << flow_return;
	}
}

GstFlowReturn ofxGstRTPClient::on_new_buffer_from_audio(GstAppSink * elt, void * data){
	static int posInBuffer=0;
	ofxGstRTPClient * client = (ofxGstRTPClient *)data;
	if(client->echoCancel){
		GstSample * sample = gst_app_sink_pull_sample(elt);
		GstBuffer * buffer = gst_sample_get_buffer(sample);


		const int numChannels = 2;
		const int samplerate = 32000;
		int buffersize = gst_buffer_get_size(buffer)/2/numChannels;
		const int samplesIn10Ms = samplerate/100;

		if(client->prevAudioBuffer){
			PooledAudioFrame * audioFrame = client->audioPool.newFrame();
			gst_buffer_map (client->prevAudioBuffer, &client->mapinfo, GST_MAP_READ);
			int prevBuffersize = gst_buffer_get_size(client->prevAudioBuffer)/2/numChannels;
			memcpy(audioFrame->audioFrame._payloadData,((short*)client->mapinfo.data)+(posInBuffer*numChannels),(prevBuffersize-posInBuffer)*numChannels*sizeof(short));
			gst_buffer_unmap(client->prevAudioBuffer,&client->mapinfo);
			gst_buffer_unref(client->prevAudioBuffer);

			gst_buffer_map (buffer, &client->mapinfo, GST_MAP_READ);
			memcpy(audioFrame->audioFrame._payloadData+((prevBuffersize-posInBuffer)*numChannels),((short*)client->mapinfo.data),(samplesIn10Ms-(prevBuffersize-posInBuffer))*numChannels*sizeof(short));

			audioFrame->audioFrame._payloadDataLengthInSamples = samplesIn10Ms;
			audioFrame->audioFrame._audioChannel = numChannels;
			audioFrame->audioFrame._frequencyInHz = samplerate;

			client->echoCancel->analyzeReverse(audioFrame->audioFrame);// << endl;
			client->sendAudioOut(audioFrame);
			posInBuffer = samplesIn10Ms-(prevBuffersize-posInBuffer);
			client->audioFramesProcessed += samplesIn10Ms;
		}else{
			gst_buffer_map (buffer, &client->mapinfo, GST_MAP_READ);
			posInBuffer = 0;
		}

		while(posInBuffer+samplesIn10Ms<=buffersize){
			PooledAudioFrame * audioFrame = client->audioPool.newFrame();
			audioFrame->audioFrame.UpdateFrame(0,GST_BUFFER_TIMESTAMP(buffer),((short*)client->mapinfo.data) + (posInBuffer*numChannels),samplesIn10Ms,samplerate,webrtc::AudioFrame::kNormalSpeech,webrtc::AudioFrame::kVadActive,numChannels,0xffffffff,0xffffffff);

			client->echoCancel->analyzeReverse(audioFrame->audioFrame);// << endl;
			client->sendAudioOut(audioFrame);
			posInBuffer+=samplesIn10Ms;
			client->audioFramesProcessed += samplesIn10Ms;
		};

		if(posInBuffer<buffersize){
			client->prevAudioBuffer = buffer;
			gst_buffer_ref(client->prevAudioBuffer);
		}else{
			client->prevAudioBuffer = 0;
		}

		gst_buffer_unmap(buffer,&client->mapinfo);
		gst_sample_unref(sample);

	}
	return GST_FLOW_OK;
}
#endif
