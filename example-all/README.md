# RTP tests only video + audio in sync

## Installing gstreamer

- We are using gstreamer-1.0. To install it under osx use this [packages](http://gstreamer.freedesktop.org/data/pkg/osx/1.0.8/)

    - gstreamer-1.0-1.0.8-universal.pkg           installs the necesary libraries for applications to run
    - gstreamer-1.0-devel-1.0.8-universal.pkg     installs the development files needed to compile gst aplications
    - gstreamer-1.0-1.0.8-universal-packages.dmg  contains some additional packages that we'll need to encode/decode h264 from this we need to install
        -gstreamer-1.0-libav-1.0.8-universal.pkg
        -gstreamer-1.0-codecs-restricted-1.0.8-universal.pkg
        -gstreamer-1.0-net-restricted-1.0.8-universal.pkg
        
- If we only want to run an osx app. The devel package is not needed but by now we need to install the rest of packages

## Running the test application

- This is based on openFramewors 0.8.0, not published yet so you'll need to use latest develop from github or a [nightly build](http://www.openframeworks.cc/nightlybuilds.html)

- The repository contains a settings_example.xml file that needs to be renamed to settings.xml and modified with the correct settings, by now only the ip of the remote end

- The H264 encoder is not emmiting key frames on a predefined time to make the stream lighter, at some point i'll add a detection of a connection through the rtpc elements and make the h264 encoder emit a keyframe since the other end won't be able to decode the stream until a keyframe arrives. By now when the connection starts there needs to be some movement in the image so the h264 encoder generates a key frame.

