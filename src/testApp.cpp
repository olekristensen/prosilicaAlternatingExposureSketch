//add libPvAPI.dylib in core frameworks
#include "testApp.h"

//--------------------------------------------------------------
void testApp::setup(){
	
	gainValue = 0;
	
	aperture = 1.5;
	EvTarget = Ev = 8;
	EvSteps = 3;
	ISO = 150; // says kodak at http://www.kodak.com/global/plugins/acrobat/en/business/ISS/productsummary/Interline/KAI-02150ProductSummary.pdf
	
	splitScreen = true;
	
	animateSplit = false;
	split = 0.5;
	
	binning = 1;
	
	exposureValue = exposureTimeMicros(aperture, Ev);
	
	saveFiles = false;
	
	camWidth = 1920;
	camHeight = 1080;
	
	sprintf(fileTypeSuffix,"tiff");
	
	ofSetLogLevel(OF_LOG_NOTICE);
	Prosilica.setVerbose(true);
	
	blackFrame.loadImage("black.tiff");
	
	ofBackground(50,50,50);
	ofSetWindowTitle("Prosilica");
	
	//659x493
	Prosilica.getVersion();
	Prosilica.listDevices();
	
	Prosilica.pixelFormat = "Bayer8";
	//	Prosilica.pixelFormat = "Rgb24";
	Prosilica.binningX = binning;
	Prosilica.binningY = binning;
	
	Prosilica.frameStartTriggerMode = "Software";
	
	
	ofSetVerticalSync(false);
	ofSetWindowPosition(2000,20);
	fullScreen = true;
	ofSetFullscreen(fullScreen);
	
	myFrameRate=myTime=myFrame=0;
	char reportStr[1024];
	
	lightImg.allocate(camWidth,camHeight, OF_IMAGE_COLOR );
	darkImg.allocate(camWidth,camHeight, OF_IMAGE_COLOR );
	lightImg.setUseTexture(true);
	darkImg.setUseTexture(true);
	
	for(int i=0; i< IMAGE_SAVER_THREADS; i++){
		videoImage[i].allocate(camWidth, camHeight, OF_IMAGE_COLOR);
		videoImage[i].setUseTexture(true);
	}
	
	frameNumber = 0;
	
	
	//get the ball rolling
	
	Prosilica.initGrabber(camWidth,camHeight, true);
	
	Prosilica.setExposureMode("Manual");
	Prosilica.setExposureValue(exposureValue);
	
	Prosilica.setGainMode("Manual");
	Prosilica.setGainValue(gainValue);
	
	Prosilica.setExposureValue(exposureValue);
	Prosilica.frameStartTriggerSoftware();
	dark = false;
	
	pixelBuffer = (unsigned char*) malloc(camWidth*camHeight*3);
	
	
}

//--------------------------------------------------------------
void testApp::update(){
	
	Prosilica.update(); //grabFrame();
	
	if(Prosilica.isFrameNew()){
		
		int currentFrame = Prosilica.doneFrameCount;
		
		bool dark = currentFrame % 2 == 1;
		
		Ev = EvTarget;
		
		fasterThanOf = (exposureValue/1000.0 <= 0.001/ofGetFrameRate());
		fasterThanOf = true;
		
		if (fasterThanOf) {
		 Prosilica.frameStartTriggerSoftware();
		}
		
		if (dark)
			exposureValue = exposureTimeMicros(aperture, Ev+EvSteps);
		else 
			exposureValue = exposureTimeMicros(aperture, Ev);
		
		Prosilica.setExposureValue(exposureValue);
		
		memcpy(pixelBuffer, Prosilica.getPixels(), camWidth*camHeight*3);
		
		int currentTime=(int)ofGetElapsedTimef();
		
		if (dark) {
			darkFrameNumber=currentFrame;
			darkImg.setFromPixels(pixelBuffer, camWidth,camHeight, OF_IMAGE_COLOR );
		} else {
			lightFrameNumber=currentFrame;
			lightImg.setFromPixels(pixelBuffer, camWidth,camHeight, OF_IMAGE_COLOR );
		}
		
		if(saveFiles){
			videoImage[frameNumber%IMAGE_SAVER_THREADS].setFromPixels(pixelBuffer, camWidth,camHeight, OF_IMAGE_COLOR);
			sprintf(imageFileName, "images/%i.%s",frameNumber, fileTypeSuffix);
			videoImage[frameNumber%IMAGE_SAVER_THREADS].saveThreaded(imageFileName);
		}
		frameNumber++;
		
		if (myTime==currentTime){
			myFrame++;
		}else{
			myTime=currentTime;
			myFrameRate=myFrame;
			myFrame=0;
		}
		if (!fasterThanOf) {
			Prosilica.frameStartTriggerSoftware();
		}
		
		
		
	}
	
}

//--------------------------------------------------------------
void testApp::draw(){
	ofEnableAlphaBlending();
	if (!splitScreen) {
		ofSetColor(255, 255, 255, 255);
		lightImg.draw(0,0,camWidth*binning, camHeight*binning);
		ofSetColor(255, 255, 255, 127);
		darkImg.draw(0,0,camWidth*binning, camHeight*binning);
		
		//		Prosilica.draw(0, 0, camWidth*binning, camHeight*binning);
	} else {		
		ofSetColor(255, 255, 255, 255);
		darkImg.draw(0,0,camWidth*binning, camHeight*binning);
		//	lightImg.draw(0,0,camWidth*binning, camHeight*binning);
		
		lightImg.bind();
		
		if (animateSplit) {
			split = 0.5+sin(ofGetElapsedTimef())*0.5;
		}
		
		glBegin(GL_QUADS);
		
		glTexCoord2d(camWidth*binning*split, 0);
		glVertex2d(camWidth*binning*split, 0);
		glTexCoord2d(camWidth*binning, 0);
		glVertex2d(camWidth*binning, 0);
		glTexCoord2d(camWidth*binning, camHeight*binning);
		glVertex2d(camWidth*binning, camHeight*binning);
		glTexCoord2d(camWidth*binning*split, camHeight*binning);
		glVertex2d(camWidth*binning*split, camHeight*binning);
		
		glEnd();
		
		lightImg.unbind();
	}
	
	ofSetColor(255, 255, 255, 255);
	
	char reportStr[1024*2];
	sprintf(reportStr, "Frame: %i\n\nEv: %i  Aperture: %f  ISO: %i  Exposure: %s%f  Gain:%i \n\nOF FPS: %f  Prosilica FPS: %i",lightFrameNumber, EvTarget, aperture, ISO, (exposureTime(aperture, EvTarget)<1.0)?"1/":"", (exposureTime(aperture, EvTarget)<1.0)?(1.0/exposureTime(aperture, EvTarget)):exposureTime(aperture, EvTarget), gainValue, ofGetFrameRate(),  myFrameRate);
	ofDrawBitmapString(reportStr, 20, ofGetHeight()-90);
	
	sprintf(reportStr, "Frame: %i\n\nEv: %i  Aperture: %f  ISO: %i  Exposure: %s%f  Gain:%i", darkFrameNumber, EvTarget+EvSteps, aperture, ISO, (exposureTime(aperture, EvTarget+EvSteps)<1.0)?"1/":"", (exposureTime(aperture, EvTarget+EvSteps)<1.0)?(1.0/exposureTime(aperture, EvTarget+EvSteps)):exposureTime(aperture, EvTarget+EvSteps), gainValue);
	ofDrawBitmapString(reportStr, ofGetWidth()*0.5, ofGetHeight()-90);
	
	if (fasterThanOf) {
		ofFill();
	} else {
		ofNoFill();
	}
	ofSetColor(255, 255, 0,255);
	ofRect(ofGetWidth()-40, ofGetHeight()-90, 10, 10);

	if (subtractBlackFrame) {
		ofFill();
		ofSetColor(255, 255,255,255);
		glBlendFunc(GL_ONE, GL_ONE); 
		glBlendEquationSeparate(GL_FUNC_REVERSE_SUBTRACT, GL_FUNC_ADD);
		blackFrame.draw(0,0,camWidth,camHeight);
		glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	} else {
		ofNoFill();
	}
	ofSetColor(0, 0, 0,255);
	ofRect(ofGetWidth()-60, ofGetHeight()-90, 10, 10);
	
	
}

//--------------------------------------------------------------
void testApp::keyPressed(int key){
	
	if (key == 'f') {
		ofSetFullscreen(fullScreen=!fullScreen);
	}
	
	if (key == 's') {
		saveFiles = !saveFiles;
	}
	
	if (key == 'x') {
		splitScreen = !splitScreen;
	}
	
	if (key == 'c') {
		EvTarget--;
		Prosilica.frameStartTriggerSoftware();
		
		//		ofLog(OF_LOG_NOTICE, "Ev is now down at %i, giving an exposure time of %f with aperture %f which becomes 1/%f", Ev, exposureTime(aperture, Ev), aperture, round(1.0/exposureTime(aperture, Ev))) ;
	}
	
	if (key == 'e') {
		EvTarget++;
		Prosilica.frameStartTriggerSoftware();
		
		//		ofLog(OF_LOG_NOTICE, "Ev is now up at %i, giving an exposure time of %f with aperture %f which becomes 1/%f", Ev, exposureTime(aperture, Ev), aperture, round(1.0/exposureTime(aperture, Ev)));
	}
	
	if (key == 'z') {
		EvSteps--;
	}
	
	if (key == 'q') {
		EvSteps++;
	}
	
	
	if (key == 'a') {
		animateSplit = !animateSplit;
	}
	
	if (key == ' ') {
		Prosilica.frameStartTriggerSoftware();
	}
	
	if (key == 'w') {
		Prosilica.setWhitebalMode("AutoOnce");
	}
	
	if (key == 'g') {
		Prosilica.setGainMode("AutoOnce");
	}
	
	if (key == 't') {
		Prosilica.setGainMode("Manual");
		Prosilica.setGainValue(++gainValue);
		//		ISO = 25^(gainValue+1);
	}
	
	if (key == '.') {
		subtractBlackFrame=!subtractBlackFrame;
	}
	
	if (key == 'b') {
		Prosilica.setGainMode("Manual");
		if (gainValue > 0) {
			Prosilica.setGainValue(--gainValue);
		}
		//		ISO = 25^(gainValue+1);
	}
	
	
}

//--------------------------------------------------------------
void testApp::keyReleased(int key){
	
}

//--------------------------------------------------------------
void testApp::mouseMoved(int x, int y ){
	
}

//--------------------------------------------------------------
void testApp::mouseDragged(int x, int y, int button){
	
}

//--------------------------------------------------------------
void testApp::mousePressed(int x, int y, int button){
	
}

//--------------------------------------------------------------
void testApp::mouseReleased(int x, int y, int button){
	
}

//--------------------------------------------------------------
void testApp::resized(int w, int h){
	
}

double testApp::exposureTime(double _aperture, int _Ev){
	
	double t = pow(_aperture, 2.0)/pow(2.0, _Ev);
	t = t / (ISO / 100.0);
	
	return t;
	
}

unsigned long testApp::exposureTimeMicros(double _aperture, int _Ev){
	
	return roundtol(exposureTime(_aperture, _Ev)*1000*1000);
	
}
