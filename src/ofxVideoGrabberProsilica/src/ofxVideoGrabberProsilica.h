#ifndef _OFX_VIDEO_GRABBER_PROSILICA
#define _OFX_VIDEO_GRABBER_PROSILICA

#include "ofConstants.h"
#include "ofTexture.h"
#include "ofGraphics.h"
#include "ofTypes.h"
#include "ofUtils.h"
#include "ofAppRunner.h"

#include <unistd.h>
#include <time.h>
#define _x86
#define _OSX

#include <PvApi.h>
#include <ImageLib.h>


#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define CAMERA_FRAMES_COUNT 15

class ofxVideoGrabberProsilica : public ofBaseVideo {
	
	public :
	
	ofxVideoGrabberProsilica();
	virtual ~ofxVideoGrabberProsilica();
	
	void getVersion();
	
	void 			listDevices();
	bool 			isFrameNew();
	void			grabFrame();
	void			close();
	bool			initGrabber(int w, int h, bool bTexture = true);
	
	pthread_mutex_t	pixelsMutex;
	
	void			videoSettings();
	unsigned char 	* getPixels();
	ofTexture &		getTextureReference();
	void 			setVerbose(bool bTalkToMe);
	void			setDeviceID(int _deviceID);
	void 			setUseTexture(bool bUse);
	void 			draw(float x, float y, float w, float h);
	void 			draw(float x, float y);
	void			update();

	unsigned long	doneFrameCount;
	unsigned long	grabbedFrameCount;
	
	//the anchor is the point the image is drawn around. 
	//this can be useful if you want to rotate an image around a particular point. 
	void			setAnchorPercent(float xPct, float yPct);	//set the anchor as a percentage of the image width/height ( 0.0-1.0 range )
	void			setAnchorPoint(int x, int y);				//set the anchor point in pixels
	void			resetAnchor();								//resets the anchor to (0, 0)
	
	
	float 			getHeight();
	float 			getWidth();
	
	int			height;
	int			width;
	
	bool		setExposureValue(unsigned long _exposureValue);
	bool		setExposureMode(char * _exposureMode);
	bool		setGainValue(unsigned long _gainValue);
	bool		setGainMode(char * _gainMode);
	bool		setWhitebalMode(char * _whitebalMode);
	bool		setWhitebalValueRed(unsigned long _whitebalValueRed);
	bool		setWhitebalValueBlue(unsigned long _whitebalValueBlue);
	
	char *			frameStartTriggerMode;
	bool			frameStartTriggerSoftware();
	char *			pixelFormat;
	unsigned long	binningX;
	unsigned long	binningY;
	
	bool					bChooseDevice;
	int						deviceID;
	bool					bUseTexture;
	ofTexture 				tex;
	bool 					bVerbose;
	bool 					bGrabberInited;
	unsigned char * 		pixels;
	unsigned char * 		threadPixels;
	bool 					bIsFrameNew;
	
	int             maxConcurrentCams;
	bool            bPvApiInitiated;
	
	int				iWaitingForFrames;
	
	// camera fields
	
	typedef struct 
	{
		unsigned long   UID;
		tPvHandle       Handle;
		tPvFrame        Frames[CAMERA_FRAMES_COUNT];
#ifdef _WINDOWS
		HANDLE          ThHandle;
		DWORD           ThId;
#else
		pthread_t       ThHandle;
#endif    
		bool            Abort;
		
	} tCamera;
	
	
	// global camera data
	tCamera         camera;
	
};

void FrameDoneCB(tPvFrame* pFrame);



#endif

