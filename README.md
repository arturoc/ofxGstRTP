# ofxGstRTP 

ofxGstRTP is part of a toolkit in development called OFStreamer, which is a set of openFrameworks addons designed to make it easier to use GStreamer on OSX and Windows. The toolkit is being developed by [Arturo Castro](http://arturocastro.net/) and [Seth Hunter](http://www.perspectum.com/) for applications that require streaming video, audio and metadata between remote locations. 

You can use the examples included in ofxGstRTP to send multiple compressed channels of data between remote peer-to-peer networks. For example, send h.264 compressed video and depth, compressed audio, and osc metadata between applications in different countries.  

The addon uses [ofxXMPP](https://github.com/arturoc/ofxXMPP) to establish the connection using jabber, google talk or any other xmpp compatible server. There's some examples that demonstrate how to establish a connection with another computer using these services. In addition, we resolve NAT traversal issues (computers being behind routers) using [ofxNice](https://github.com/arturoc/ofxXMPP) so you don't have to use static IPs or setup a VPN, potentially making your applicaiton scalable to many clients. NAT traversal will work with most routers (92%) except symetric routers in big organizations and hotels. You'll also need [ofxDepthStreamCompression](https://github.com/arturoc/ofxDepthStreamCompression) that commpresses the depth stream to have good quality point clouds and [ofxSnappy](https://github.com/arturoc/ofxSnappy) used also to compress the depth and osc streams.

There's an experimental echo cancelation module, disabled by default that can be enabled in src/ofxGstRTPConstants.h. It depends on [ofxEchoCancel](https://github.com/arturoc/ofxEchoCancel)

Currently we have examples working on OSX and Linux and are working on porting the examples to windows 7 and 8. 


## Installing gstreamer

This addon also depends on gstreamer, in linux it's installed by default as part of openFrameworks. On OSX and Windows you'll need to install it manually. To make this process easier please use the [ofxGstreamer](https://github.com/arturoc/ofxGstreamer) addon and follow the directions included there to install the dependencies. 

## Running the examples

- This is based on openFramewors master, so you'll need to use latest master from github or a [nightly build](http://www.openframeworks.cc/nightlybuilds.html) To generate the projects correctly you'll also need to build the latest version of the project generator.

- Almost all examples contain a settings_example.xml file that needs to be renamed to settings.xml and modified with the correct settings for your gmail account. 

- Since google moved from XMPP chat to google hangouts, its become more difficult to allow apps to access your gmail account. You will need to reduce the security in your gmail settings to allow external apps to access your contact data. This is not ideal, and we have plans to install our own XMPP server, but for now we use dummy gmail accounts for hacking. 


