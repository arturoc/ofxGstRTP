/*
 * Utils.cpp
 *
 *  Created on: Aug 1, 2013
 *      Author: arturo
 */

#include "Utils.h"


ofColor * colorGradientLUT;

void Utils::CreateColorGradientLUT(double maxDepth){
	colorGradientLUT = new ofColor[(int)maxDepth];
	ofFloatColor c;
	for(int i=0;i<maxDepth;i++){
		double p = i/maxDepth;
		c.setHsb(p,1-p,1-p);
		colorGradientLUT[i] = c;
		swap(colorGradientLUT[i].r,colorGradientLUT[i].g);
	}
}

Utils::Utils() {
	// TODO Auto-generated constructor stub

}

Utils::~Utils() {
	// TODO Auto-generated destructor stub
}

void Utils::getColoredDepth(double maxDepth, unsigned short depth, ofColor& color) {
	color = colorGradientLUT[depth];

	/*color.r = (depth >> 8) & 0xFF;
	color.g = depth & 0xFF;
	color.b = (depth >> 8) & 0xFF;*/
}


void Utils::getRawDepthFromColored(double maxDepth, const ofColor & color, unsigned short & depth){
	ofFloatColor f = color;
	swap(f.r,f.g);
	float h,s,b;
	f.getHsb(h,s,b);
	depth = ((h*maxDepth) + ((1-b)*maxDepth) + ((1-s)*maxDepth))/3.;

	//depth = (((int(color.r)<<8) & 0xFF00) + ((int(color.b)<<8) & 0xFF00))*.5 + color.g;

}

void Utils::convertShortToColoredDepth(const ofShortPixels& rawDepth, ofPixels& coloredDepth, double maxDepth) {
	for(int i=0;i<rawDepth.size();i++){
		double depth = rawDepth[i];
		getColoredDepth(maxDepth,depth,*((ofColor*)&coloredDepth[i*3]));
	}
}

void Utils::convertColoredDepthToShort(const ofPixels& coloredDepth, ofShortPixels& rawDepth, double maxDepth) {
	for(int i=0;i<rawDepth.size();i++){
		const ofColor & color = *((ofColor*)&coloredDepth[i*3]);
		getRawDepthFromColored(maxDepth,color,rawDepth[i]);
	}
}
