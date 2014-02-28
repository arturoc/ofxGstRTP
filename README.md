# ofxGstRTP 

ofxGstRTP is part of a toolkit in development called OFStreamer, which is a set of openFrameworks addons designed to make it easier to use GStreamer on OSX and Windows. The toolkit is being developed by [Arturo Castro](http://arturocastro.net/) and [Seth Hunter](http://www.perspectum.com/) for applications that require streaming video, audio and metadata between remote locations. 

You can use the examples included in ofxGstRTP to send multiple compressed channels of data between remote peer-to-peer networks. For example, send h.264 compressed video and depth, compressed audio, and osc metadata between applications in different countries.  

The addon uses [ofxXMPP](https://github.com/arturoc/ofxXMPP) to establish the connection using jabber, google talk or any other xmpp compatible server. There's some examples that demonstrate how to establish a connection with another computer using these services. In addition, we resolve NAT traversal issues (computers being behind routers) using [ofxNice](https://github.com/arturoc/ofxXMPP) so you don't have to use static IPs or setup a VPN, potentially making your applicaiton scalable to many clients. NAT traversal will work with most routers (92%) except symetric routers in big organizations and hotels.

Currently we have examples working on OSX and Linux and are working on porting the examples to windows 7 and 8. 


## Installing gstreamer

This addon also depends on gstreamer, in linux it's installed by default as part of openFrameworks. On OSX and Windows you'll need to install it manually. To make this process easier please use the [ofxGtreamer](https://github.com/arturoc/ofxGstreamer) addon and follow the directions included there to install the dependencies. 

## Running the examples

- This is based on openFramewors master (2014.02.27), so you'll need to use latest master from github or a [nightly build](http://www.openframeworks.cc/nightlybuilds.html) To generate the projects correctly you'll also need the latest version of the PG as of 2014.02.27

- Almost all examples contain a settings_example.xml file that needs to be renamed to settings.xml and modified with the correct settings.


