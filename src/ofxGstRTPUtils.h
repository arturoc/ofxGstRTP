/*
 * Utils.h
 *
 *  Created on: Aug 1, 2013
 *      Author: arturo
 */

#ifndef UTILS_H_
#define UTILS_H_

#include "ofColor.h"
#include "ofPixels.h"

class ofxGstRTPUtils {
public:
	ofxGstRTPUtils();
	virtual ~ofxGstRTPUtils();

	static void CreateColorGradientLUT(double maxDepth);

	static void getColoredDepth(double maxDepth, unsigned short depth, ofColor & color);
	static void convertShortToColoredDepth(const ofShortPixels & rawDepth, ofPixels & coloredDepth, double maxDepth);
	static void getRawDepthFromColored(double maxDepth, const ofColor & color, unsigned short & depth);
	static void convertColoredDepthToShort(const ofPixels & coloredDepth, ofShortPixels & rawDepth, double maxDepth);
};

#endif /* UTILS_H_ */
