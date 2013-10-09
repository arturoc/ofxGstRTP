/*
 * ofxGstRTPConstants.h
 *
 *  Created on: Oct 9, 2013
 *      Author: arturo
 */

#ifndef OFXGSTRTPCONSTANTS_H_
#define OFXGSTRTPCONSTANTS_H_

/// when enabled the addon will have dependencies with ofxXMPP and ofxNice
/// if disabled ofxGstXMPPRTP won't work and connection will need to be done
/// through ofxGstServer/client to a specific port and IP address
#define ENABLE_NAT_TRANSVERSAL 1

/// when enabled the addon will have dependencies with ofxEchoCancel on linux
/// the pulse server can do echo cancellation natively so there's no need to enable
/// this flag
#define ENABLE_ECHO_CANCEL 1

#endif /* OFXGSTRTPCONSTANTS_H_ */
