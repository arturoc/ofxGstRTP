# All variables and this file are optional, if they are not present the PG and the
# makefiles will try to parse the correct values from the file system.
#
# Variables that specify exclusions can use % as a wildcard to specify that anything in
# that position will match. A partial path can also be specified to, for example, exclude
# a whole folder from the parsed paths from the file system
#
# Variables can be specified using = or +=
# = will clear the contents of that variable both specified from the file or the ones parsed
# from the file system
# += will add the values to the previous ones in the file or the ones parsed from the file 
# system
# 
# The PG can be used to detect errors in this file, just create a new project with this addon 
# and the PG will write to the console the kind of error and in which line it is

meta:
	ADDON_NAME = ofxGstRTP
	ADDON_DESCRIPTION = Addon for sending video, audio, depth and osc to a remote peer using RTP. It can also use ofxXMPP to initiate the session through google talk, jabber or any other xmpp compatible server so you don't need to know the IP of the remote end. It can also use ofxNice to do NAT transversal so you can send data from computers behind NAT routers. Requires gstreamer to work.
	ADDON_AUTHOR = Arturo Castro
	ADDON_TAGS = "RTP" "video" "audio" "depth" "osc" "kinect" "NAT transversal" "google talk" "jabber" "ICE" "NAT" "network"
	ADDON_URL = http://github.com/arturoc/ofxGstRTP

common:
	# dependencies with other addons, a list of them separated by spaces 
	# or use += in several lines
	ADDON_DEPENDENCIES = ofxNice ofxXMPP ofxGStreamer
	
	# include search paths, this will be usually parsed from the file system
	# but if the addon or addon libraries need special search paths they can be
	# specified here separated by spaces or one per line using +=
	# ADDON_INCLUDES =
	
	# any special flag that should be passed to the compiler when using this
	# addon
	# ADDON_CFLAGS = 
	
	# any special flag that should be passed to the linker when using this
	# addon, also used for system libraries with -lname
	# ADDON_LDFLAGS =
	
	# linux only, any library that should be included in the project using
	# pkg-config
	# ADDON_PKG_CONFIG_LIBRARIES = 
	
	# osx/iOS only, any framework that should be included in the project
	# ADDON_FRAMEWORKS = 
	
	# source files, these will be usually parsed from the file system looking
	# in the src folders in libs and the root of the addon. if your addon needs
	# to include files in different places or a different set of files per platform
	# they can be specified here
	# ADDON_SOURCES =
	
	# some addons need resources to be copied to the bin/data folder of the project
	# specify here any files that need to be copied, you can use wildcards like * and ?
	# ADDON_DATA = 
	
	# when parsing the file system looking for libraries exclude this for all or
	# a specific platform
	# ADDON_LIBS_EXCLUDE =
	
	# when parsing the file system looking for sources exclude this for all or
	# a specific platform
	
	# when parsing the file system looking for include paths exclude this for all or
	# a specific platform
    # ADDON_INCLUDES_EXCLUDE =

linux:
	
linux64:
	
linuxarmv6l:
	
linuxarmv7l:
	
win_cb:
	
vs:
	
android/armeabi:	
	
android/armeabi-v7a:	

osx:
    
ios:
	
	

