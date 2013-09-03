# ofxGstRTP 

openFrameworks addon to send video, audio, depth and metadata through osc between to computers. This addon uses ofxXMPP to establish the connection using jabber, google talk or any other xmpp compatible server. There's some examples that demonstrate how to establish a connection with another computer using this services. This allows to avoid having to set static ips. IT also uses ofxNice to allow for NAT transversal which means that you can send data from one computer to another even if they are behind a router without need to open any ports on them. Only in the case that both peers are behind symetric routers (which should only exist in big organizations, hotels...) the NAT transversal won't work.


## Installing gstreamer

This addon also depends on gstreamer, in linux it's installed by default as part of the openFrameworks dependencies. On osx and windows you'll need to install it manually:

### osx

- We are using gstreamer-1.0. To install it under osx use this [packages](http://gstreamer.freedesktop.org/data/pkg/osx/1.0.8/)

    - gstreamer-1.0-1.0.8-universal.pkg           installs the necesary libraries for applications to run
    - gstreamer-1.0-devel-1.0.8-universal.pkg     installs the development files needed to compile gst aplications
    - gstreamer-1.0-1.0.8-universal-packages.dmg  contains some additional packages that we'll need to encode/decode h264 from this we need to install
        - gstreamer-1.0-libav-1.0.8-universal.pkg
        - gstreamer-1.0-codecs-restricted-1.0.8-universal.pkg
        - gstreamer-1.0-net-restricted-1.0.8-universal.pkg
        
- If we only want to run an osx app. The devel package is not needed but by now we need to install the rest of packages
- On some systems it seems it's necesary to remove or rename /Library/Frameworks/GStreamer/Headers/assert.h   or it'll clash with the assert.h in the system and some projects won't compile. As far as i've tested it's safe to just delete it but it's probably better to keep a copy just in case.

## Running the examples

- This is based on openFramewors master (2013.09.01), so you'll need to use latest master from github or a [nightly build](http://www.openframeworks.cc/nightlybuilds.html)

- Almost all examples contain a settings_example.xml file that needs to be renamed to settings.xml and modified with the correct settings.

- The H264 encoder is not emmiting key frames on a predefined time to make the stream lighter, at some point i'll add a detection of a connection through the rtpc elements and make the h264 encoder emit a keyframe since the other end won't be able to decode the stream until a keyframe arrives. By now when the connection starts there needs to be some movement in the image so the h264 encoder generates a key frame.

