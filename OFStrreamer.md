#OFStreamer 

OFStreamer is a set of openFrameworks addons that allows peer to peer streaming of video, audio, depth and osc. All the sent streams are synchronized and it can do NAT transversal and session initiation.

To use it you'll need the following addons:

- ofxGstRTP: This is the main addon you'll be interfacing with, it uses gstreamer to implement [RTP](http://en.wikipedia.org/wiki/Real-time_Transport_Protocol) and depends on the other addons to do NAT transversal and session initiation.
- ofxNice: used for NAT transversal, allows two peers to communicate while being behind NAT routers
- ofxXMPP: used for session initiation and chat. When doing NAT transversal, we need to send the information of the opened channels and in which ports and IP they are, this addon uses the XMPP protocol to comunicate that information in order to stablish a call. Although it's usually used from ofxGstRTP, it can be used alone to do text only chat.
- ofxGstreamer: needed on windows and osx to add gstreamer to a project
- ofxDepthCompression: implements some custom codecs for compression of the depth stream
- ofxSnappy: compression library for fast entropy compression, used by ofxDepthCompression and ofxGstRTP

## Protocols:

### RTP 
RTP is a network protocol that allows to send different kinds of streams over a network, receive statistics of the quality of the connection and keep all the different streams in sync. This protocol is implemented by ofxGstRTP

### ICE 
ICE is a protocol that allows two peers to communicate while being behind NAT routers. A NAT router allows several machines to connect to the Internet with only one public IP, while this is really useful it makes it difficult to connect two machines when they are connected through routers of this kind. ICE allows to overcome this by using a third party server to start the connection, once the connection is stablished all the data is sent peer to peer. For that when starting a connection, the peer starting it, connects to an STUNT server in order to open a port in the router. The STUNT server helps knowing which port the router has opened and we can then comunicate that to the other side by sending the IP, port and a username and password in order to make the communication more secure (although not encrypted by default). To send that information to the other peer we need some kind of session initiation protocol like XMPP

### XMPP 
XMPP is a network protocol used to do text chat and session initiation through the subprotocol jingle. There's several public servers that support it like jabber.org or talk.google.com. It allows to send text messages formated as XML tags with text messages, that carry chat messages, state of the peer (away, do not disturb...), and session initiation messages like the information required by ICE to stablish a connection.

## Installation

First of all download all the required addons:

- [ofxGstRTP](https://github.com/arturoc/ofxGstRTP)
- [ofxNice](https://github.com/arturoc/ofxNice)
- [ofxXMPP](https://github.com/arturoc/ofxXMPP)
- [ofxDepthCompression](https://github.com/arturoc/ofxDepthCompression)


Then you'll need to install GStreamer on your computer, if you are in linux it'll be installed by default, in windows or osx follow the instructions in ofxGStreamer to install it:

https://github.com/arturoc/ofxGStreamer/blob/master/README.md


## Workflow

In order to use OFStreamer, you need to create a project using the project generator and add the following addons:

- ofxGstRTP
- ofxNice
- ofxXMPP
- ofxOsc
- ofxDepthCompression

If you want to send depth information you probably want to also include ofxKinect.

ofxGstRTP is the main addon that you'll be using, most of the others are only used as dependencies of ofxGstRTP and won't be used directly.

ofxGstRTP, can stablish an RTP connection with channels for audio, video, depth and osc. That connection can be stablished using an IP address and port in both sides, or using ofxNice to do NAT transversal. It also can start an XMPP session in order to send text chat messages and starting a call.

When using direct IP/port connection, the classes to use are ofxGstRTPServer and ofxGstRTPClient. Both sides need to create an instance of both and add the same channels with matching ports and IPs. Check the inlined or docygen documentation and the example-LAN for more detailed information.

When doing NAT Transversal, the only class needed to use directly is ofxGstXMPPRTP, which will take care of all the workflow. Usually one of the sides, will start a call by specifying the used channels (video, audio...) and call a contanct in the friends list accesible from the ofxGstXMPPRTP instance. The other side will accept or refuse that call. If the call is accepted the addon internally will create the necesary channels and stablish the connection through the jingle subprotocol of XMPP. Once the connection is stablished, you can access the ofxRTPClient and server to receive and send new frames. Check the doxygen documentation for this class or any of the examples in ofxGstRTP for more details.
