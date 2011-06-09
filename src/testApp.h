#ifndef _TEST_APP
#define _TEST_APP


#include "ofMain.h"
#include "ofxOpenCv.h"
#include "ofxVideoGrabberProsilica.h"
#include "ofxThreadedImageSaver.h"



#define IMAGE_SAVER_THREADS 6


class testApp : public ofBaseApp{
	
public:
	
	void setup();
	void update();
	void draw();
	
	void keyPressed  (int key);
	void keyReleased(int key);
	void mouseMoved(int x, int y );
	void mouseDragged(int x, int y, int button);
	void mousePressed(int x, int y, int button);
	void mouseReleased(int x, int y, int button);
	void resized(int w, int h);
	
	unsigned long binning;
	
	char imageFileName[255];
	char fileTypeSuffix[255];
	
	unsigned long exposureValue;
	unsigned long darkFraction;
	
	double aperture;
	
	bool animateSplit;

	float split;
	
	int Ev;
	int EvSteps;
	int EvTarget;
	int ISO;
	int gainValue;
	bool dark;
	bool saveFiles;
	bool splitScreen;
	bool fasterThanOf;
	int darkFrameNumber;
	int lightFrameNumber;
	bool subtractBlackFrame;
	unsigned char* pixelBuffer;

	bool trigger;
	
	ofxThreadedImageSaver videoImage[IMAGE_SAVER_THREADS];
	
	int frameNumber;
	
	bool fullScreen;
	ofxThreadedImageSaver  lightImg;
	ofxThreadedImageSaver  darkImg;
	
	ofImage blackFrame;
	
	ofxVideoGrabberProsilica Prosilica;
	int camWidth,camHeight;
	int myTime;
	int myFrame;
	int myFrameRate;
	
	double exposureTime(double aperture, int Ev);
	unsigned long exposureTimeMicros(double _aperture, int _Ev);

};

#endif
