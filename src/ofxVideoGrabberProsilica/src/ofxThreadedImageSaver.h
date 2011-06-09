/*
 *  imageSaver.h
 *  movieGrabberExample
 *
 *  Created by ole kristensen on 21/12/10.
 *  Copyright 2010 Recoil Performance Group. All rights reserved.
 *
 */

#include "ofMain.h"
#include "ofxThread.h"

class ofxThreadedImageSaver : public ofxThread, public ofImage {
public:
	string fileName;
	
	void threadedFunction() {
		if(lock()) {
			saveImage(fileName);
			unlock();
		} else {
			printf("ofxThreadedImageSaver - cannot save %s cos I'm locked", fileName.c_str());
		}
		stopThread();
	}
	
	void saveThreaded(string fileName) {
		this->fileName = fileName;
		startThread(false, false);   // blocking, verbose
	}
};