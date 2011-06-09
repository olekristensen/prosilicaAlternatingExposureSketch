#include "ofxVideoGrabberProsilica.h"

//--------------------------------------------------------------------
ofxVideoGrabberProsilica::ofxVideoGrabberProsilica(){
	bIsFrameNew				= false;
	bVerbose 				= false;
	bGrabberInited 			= false;
	bUseTexture				= true;
	bChooseDevice			= false;
	deviceID				= 0;
	width 					= 640;	// default setting
	height 					= 480;	// default setting
	pixels					= NULL;
	threadPixels			= NULL;
    
    maxConcurrentCams       = 20;
    bPvApiInitiated         = false;
	iWaitingForFrames		= 0;
	
	doneFrameCount			= 0;
	grabbedFrameCount		= 0;
	
	frameStartTriggerMode = "Freerun";
	
	binningX = 1;
	binningY = 1;
	
	pixelFormat = "Bayer8";
	
	pthread_mutex_init(&pixelsMutex, NULL);
}


//--------------------------------------------------------------------
ofxVideoGrabberProsilica::~ofxVideoGrabberProsilica(){
	close();
}


void ofxVideoGrabberProsilica::getVersion(){
	UInt32 majorV, minorV;
	PvVersion(&majorV, &minorV);
	ofLog(OF_LOG_VERBOSE,"PvApi Version = %i.%i", majorV, minorV);
}


//--------------------------------------------------------------------
void ofxVideoGrabberProsilica::listDevices(){
    // lazy initialization of the Prosilica API
    if( !bPvApiInitiated ){
        int ret = PvInitialize();
        if( ret == ePvErrSuccess ) {
            ofLog(OF_LOG_VERBOSE, "PvAPI initialized");
        } else {
            ofLog(OF_LOG_ERROR, "unable to initialize PvAPI");
            return;
        }
        bPvApiInitiated = true;
    }
	
    ofLog(OF_LOG_NOTICE, "-------------------------------------");
    
    // get a camera from the list
    tPvUint32 count, connected;
    tPvCameraInfo list[maxConcurrentCams];
	
    count = PvCameraList( list, maxConcurrentCams, &connected );
    if( connected > maxConcurrentCams ) {
        ofLog(OF_LOG_NOTICE, "more cameras connected than will be listed");
    }
    
    if(count >= 1) {
        ofLog(OF_LOG_NOTICE, "listing available capture devices");
        for( int i=0; i<count; ++i) {
            ofLog(OF_LOG_NOTICE, "got camera %s - %s - id: %i\n", list[i].DisplayName, list[i].SerialString, list[i].UniqueId);
        }
    } else {
        ofLog(OF_LOG_NOTICE, "no cameras found");
    }
    
    ofLog(OF_LOG_NOTICE, "-------------------------------------");
}

//--------------------------------------------------------------------
bool ofxVideoGrabberProsilica::initGrabber(int w, int h, bool setUseTexture){
	
    width = w;
    height = h;
    tPvErr ret;   //PvAPI return codes
	bUseTexture = setUseTexture;
	
	//char argv[4];
	//	argv[0] = '164';
	//	argv[1] = "164";
	//	argv[2] = "164";
	//	argv[3] = "164";
	//	
	//	unsigned long IP = inet_addr(argv[1]);
	//    PvCameraOpenByAddr(IP,ePvAccessMaster,&(GCamera.Handle));
    //---------------------------------- 1 - open the sequence grabber
    // lazy initialization of the Prosilica API
    if( !bPvApiInitiated ){
        ret = PvInitialize();
        if( ret == ePvErrSuccess ) {
            ofLog(OF_LOG_VERBOSE, "PvAPI initialized");
        } else {
            ofLog(OF_LOG_ERROR, "unable to initialize PvAPI");
            return false;
        }
        
        bPvApiInitiated = true;
    }
	
	/*
	 char address_string[] = "169.254.42.97";
	 unsigned long IP = inet_addr(address_string); //inet_addr(argv[1]);
	 PvCameraOpenByAddr(IP,ePvAccessMaster,&camera.Handle);
	 */
	
    //---------------------------------- 3 - buffer allocation
    // Create a buffer big enough to hold the video data,
    // make sure the pointer is 32-byte aligned.
    // also the rgb image that people will grab
    
	pixels = new unsigned char[width*height*3];
	threadPixels = new unsigned char[width*height*3];
	
    // check for any cameras plugged in
    int waitIterations = 0;
    while( PvCameraCount() < 1 ) {
        ofSleepMillis(250);
		cout << ".";
        waitIterations++;
        
        if( waitIterations > 30 ) {
            ofLog(OF_LOG_ERROR, "error: no camera found (PvCameraCount() < 1)");
            return false;        
        }
    }
	
	
    //---------------------------------- 4 - device selection
    
    tPvUint32 count, connected;
    tPvCameraInfo list[maxConcurrentCams];
	
    count = PvCameraList( list, maxConcurrentCams, &connected );
	
	cout<<"camera count "<<count<<endl;
    if(count >= 1) {
        bool bSelectedDevicePresent = false;
        if(bChooseDevice) {
            //check if selected device is available
            for( int i=0; i<count; ++i) {
                if( deviceID == list[i].UniqueId ) {
                    bSelectedDevicePresent = true;
                    camera.UID = list[i].UniqueId;
					ofLog(OF_LOG_VERBOSE, "%i camera.UID %d", i,camera.UID);
                }
            }
        }
        
        if( !bSelectedDevicePresent ){
            camera.UID = list[0].UniqueId;
			ofLog(OF_LOG_VERBOSE, "camera.UID %d",camera.UID);
            ofLog(OF_LOG_NOTICE, "cannot find selected camera -> defaulting to first available");
            ofLog(OF_LOG_VERBOSE, "there is currently an arbitrary hard limit of %i concurrent cams", maxConcurrentCams);
        }        
    } else {
        ofLog(OF_LOG_ERROR, "no cameras available (count >= 1)");
        return false;     
    }
	// ofSleepMillis(2250);
	//	cameraUID = 33334;
    //---------------------------------- 5 - final initialization steps
    
    ret = PvCameraOpen( camera.UID, ePvAccessMaster, &(camera.Handle) );
    if( ret == ePvErrSuccess ){
        ofLog(OF_LOG_VERBOSE, "camera opened");
    } else {
        if( ret == ePvErrAccessDenied ) {
            ofLog(OF_LOG_ERROR, "camera access denied, probably already in use");
        }
        ofLog(OF_LOG_ERROR, "failed to open camera");
        return false;     
    }
	
	ret = PvAttrEnumSet(camera.Handle,"FrameStartTriggerMode",frameStartTriggerMode);
    if( ret == ePvErrSuccess ){
        ofLog(OF_LOG_VERBOSE, "camera set to %s mode", frameStartTriggerMode);
    } else {
        ofLog(OF_LOG_ERROR, "cannot set to %s mode", frameStartTriggerMode);
        return false;    
    }    
	
	ret = PvAttrEnumSet(camera.Handle,"PixelFormat",pixelFormat);
    if( ret == ePvErrSuccess ){
        ofLog(OF_LOG_VERBOSE, "camera set to %s pixel format", pixelFormat);
    } else {
        ofLog(OF_LOG_ERROR, "cannot set to %s pixel format", pixelFormat);
        return false;    
    }    
	
	ret = PvAttrUint32Set(camera.Handle,"BinningX",binningX);
    if( ret == ePvErrSuccess ){
        ofLog(OF_LOG_VERBOSE, "camera horisontal binning factor set to %i", binningX);
    } else {
        ofLog(OF_LOG_ERROR, "cannot set the horisontal binning factor to %i", binningX);
        return false;    
    }
    
	ret = PvAttrUint32Set(camera.Handle,"BinningY",binningY);
    if( ret == ePvErrSuccess ){
        ofLog(OF_LOG_VERBOSE, "camera horisontal binning factor set to %i", binningY);
    } else {
        ofLog(OF_LOG_ERROR, "cannot set the horisontal binning factor to %i", binningY);
        return false;    
    }
    
	unsigned long FrameSize = 0;
    ret = PvAttrUint32Get( camera.Handle, "TotalBytesPerFrame", &FrameSize );
    if( ret == ePvErrSuccess ){
		ofLog(OF_LOG_VERBOSE, "camera asked for TotalBytesPerFrame");
    } else { 
        ofLog(OF_LOG_ERROR, "failed to allocate capture buffer");
        return false;    
    }    
	
	bool failed = false;
	
	// allocate the buffer for each frames
	for(int i=0;i<CAMERA_FRAMES_COUNT && !failed;i++)
	{
		camera.Frames[i].Context[0] = this;
		camera.Frames[i].ImageBuffer = new char[FrameSize];
		if(camera.Frames[i].ImageBuffer)
			camera.Frames[i].ImageBufferSize = FrameSize;
		else
			failed = true;
	}
	
	if(failed){
		ofLog(OF_LOG_ERROR, "cannot allocate camera frame buffers");
		return false;
	}
	
    ret = PvCaptureStart(camera.Handle);
    if( ret == ePvErrSuccess ){
        ofLog(OF_LOG_VERBOSE, "camera set to capture mode");
    } else { 
        if( ret == ePvErrUnplugged ){
            ofLog(OF_LOG_ERROR, "cannot start capture, camera was unplugged");
        }
        ofLog(OF_LOG_ERROR, "cannot set capture mode");
        return false;    
    }
	
    ret = PvCommandRun(camera.Handle,"AcquisitionStart");
    if( ret == ePvErrSuccess ){
        ofLog(OF_LOG_VERBOSE, "camera continuous acquisition started");
    } else {
        // if that fail, we reset the camera to non capture mode
        PvCaptureEnd(camera.Handle) ;
        ofLog(OF_LOG_ERROR, "cannot start continuous acquisition");
        return false;    
    }
    
	bGrabberInited = true;
	//loadSettings();    
    ofLog(OF_LOG_NOTICE,"camera is ready now");  
	
    //---------------------------------- 6 - setup texture if needed
	
    if (bUseTexture){
        // create the texture, set the pixels to black and
        // upload them to the texture (so at least we see nothing black the callback)
		
		tex.allocate(width,height,GL_RGB);
		memset(pixels, 0, width*height*3);
		memset(threadPixels, 0, width*height*3);
		tex.loadData(pixels, width, height, GL_RGB);
    }
	
	for(int i=0;i<CAMERA_FRAMES_COUNT;i++){
		PvCaptureQueueFrame(camera.Handle,&(camera.Frames[i]),FrameDoneCB);
		iWaitingForFrames++;
	}
	
    ofLog(OF_LOG_NOTICE,"camera frames queued");
	
	return true;                
    
}


//--------------------------------------------------------------------
void ofxVideoGrabberProsilica::setVerbose(bool bTalkToMe){
	bVerbose = bTalkToMe;
	
}


//--------------------------------------------------------------------
void ofxVideoGrabberProsilica::setDeviceID(int _deviceID){
	deviceID		= _deviceID;
	bChooseDevice	= true;
}

//---------------------------------------------------------------------------
unsigned char * ofxVideoGrabberProsilica::getPixels(){
	return pixels;
}

//------------------------------------
//for getting a reference to the texture
ofTexture & ofxVideoGrabberProsilica::getTextureReference(){
	if(!tex.bAllocated() ){
		ofLog(OF_LOG_WARNING, "ofVideoGrabber - getTextureReference - texture is not allocated");
	}
	return tex;
}

//---------------------------------------------------------------------------
bool  ofxVideoGrabberProsilica::isFrameNew(){
	return bIsFrameNew;
}

//--------------------------------------------------------------------
void ofxVideoGrabberProsilica::update(){
	grabFrame();
}

//--------------------------------------------------------------------
void ofxVideoGrabberProsilica::grabFrame(){
    if( bGrabberInited ){
		if (doneFrameCount != grabbedFrameCount) {
			pthread_mutex_lock(&pixelsMutex);
			memcpy(pixels, threadPixels, width*height*3);
			grabbedFrameCount = doneFrameCount;
			bIsFrameNew = true;
			pthread_mutex_unlock(&pixelsMutex);
			if (bUseTexture){
				tex.loadData(pixels, width, height, GL_RGB );
			}
		} else {
			bIsFrameNew = false;
		}

    } else {
		ofLog(OF_LOG_WARNING, "GrabberInited not");
	}
}

bool ofxVideoGrabberProsilica::frameStartTriggerSoftware(){
	
	if (strcmp(frameStartTriggerMode, "Software") == 0) {
		tPvErr ret;   //PvAPI return codes
		
		ret = PvCommandRun(camera.Handle,"FrameStartTriggerSoftware");
		if( ret == ePvErrSuccess ){
			ofLog(OF_LOG_VERBOSE, "camera triggered");
			return true;
		} else {
			// if that fail, we reset the camera to non capture mode
			PvCaptureEnd(camera.Handle) ;
			ofLog(OF_LOG_ERROR, "cannot trigger camera");
			return false;    
		}
	} else {
		ofLog(OF_LOG_WARNING, "cannot trigger from software when in %s trigger mode", frameStartTriggerMode);
	}
	
	
	
}

bool ofxVideoGrabberProsilica::setExposureMode(char * _exposureMode){
	tPvErr ret;   //PvAPI return codes
	
	ret = PvAttrEnumSet(camera.Handle,"ExposureMode",_exposureMode);
	if( ret == ePvErrSuccess ){
		ofLog(OF_LOG_VERBOSE, "camera set to %s exposure mode", _exposureMode);
		return false;
	} else {
		ofLog(OF_LOG_ERROR, "cannot set %s exposure mode", _exposureMode);
		return false;    
	}    
	
}

bool ofxVideoGrabberProsilica::setExposureValue(unsigned long _exposureValue){
	tPvErr ret;   //PvAPI return codes
	
    ret = PvAttrUint32Set(camera.Handle,"ExposureValue",_exposureValue);
    if( ret == ePvErrSuccess ){
        ofLog(OF_LOG_VERBOSE, "camera exposure set to %f ms", _exposureValue * 0.001);
		return true;
    } else {
        ofLog(OF_LOG_ERROR, "cannot set camera exposure to %f ms", _exposureValue * 0.001);
        return false;    
    }
	
}

bool ofxVideoGrabberProsilica::setGainMode(char * _gainMode){
	tPvErr ret;   //PvAPI return codes
	
	ret = PvAttrEnumSet(camera.Handle,"GainMode",_gainMode);
	if( ret == ePvErrSuccess ){
		ofLog(OF_LOG_VERBOSE, "camera set to %s gain mode", _gainMode);
		return false;
	} else {
		ofLog(OF_LOG_ERROR, "cannot set %s gain mode", _gainMode);
		return false;    
	}    
	
}

bool ofxVideoGrabberProsilica::setGainValue(unsigned long _gainValue){
	tPvErr ret;   //PvAPI return codes
	
	ret = PvAttrUint32Set(camera.Handle,"GainValue",_gainValue);
    if( ret == ePvErrSuccess ){
        ofLog(OF_LOG_VERBOSE, "camera gain set to %i", _gainValue);
		return true;
	} else {
        ofLog(OF_LOG_ERROR, "cannot set camera gain to %i", _gainValue);
        return false;    
    }
	
}

bool ofxVideoGrabberProsilica::setWhitebalMode(char * _whitebalMode){
	tPvErr ret;   //PvAPI return codes
	
	ret = PvAttrEnumSet(camera.Handle,"WhitebalMode",_whitebalMode);
	if( ret == ePvErrSuccess ){
		ofLog(OF_LOG_VERBOSE, "camera set to %s whitebalance mode", _whitebalMode);
		return false;
	} else {
		ofLog(OF_LOG_ERROR, "cannot set %s whitebalance mode", _whitebalMode);
		return false;   
	}    
	
}


bool ofxVideoGrabberProsilica::setWhitebalValueRed(unsigned long _whitebalValueRed){
	tPvErr ret;   //PvAPI return codes
	
	ret = PvAttrUint32Set(camera.Handle,"WhitebalValueRed",_whitebalValueRed);
    if( ret == ePvErrSuccess ){
        ofLog(OF_LOG_VERBOSE, "camera whitebalance red gain set to %i", _whitebalValueRed);
		return true;
	} else {
        ofLog(OF_LOG_ERROR, "cannot set camera whitebalance red gain to %i", _whitebalValueRed);
        return false;    
    }
	
}

bool ofxVideoGrabberProsilica::setWhitebalValueBlue(unsigned long _whitebalValueBlue){
	tPvErr ret;   //PvAPI return codes
	
	ret = PvAttrUint32Set(camera.Handle,"WhitebalValueBlue",_whitebalValueBlue);
    if( ret == ePvErrSuccess ){
        ofLog(OF_LOG_VERBOSE, "camera whitebalance blue gain set to %i", _whitebalValueBlue);
		return true;
	} else {
        ofLog(OF_LOG_ERROR, "cannot set camera whitebalance blue gain to %i", _whitebalValueBlue);
        return false;    
    }
	
}



//--------------------------------------------------------------------
void ofxVideoGrabberProsilica::close(){
	
    if( bGrabberInited ) {
        // stop the streaming
        PvCommandRun(camera.Handle,"AcquisitionStop");
        PvCaptureEnd(camera.Handle);  
        
        // unsetup the camera
		PvCaptureQueueClear(camera.Handle);
        PvCameraClose(camera.Handle);
		// delete all the allocated buffers
		for(int i=0;i<CAMERA_FRAMES_COUNT;i++)
			delete [] (char*)camera.Frames[i].ImageBuffer;  
    }
	
    if( bPvApiInitiated ) {
        // uninitialise the API
        PvUnInitialize();
    }
	
	if (pixels != NULL){
		delete[] pixels;
		pixels = NULL;
	}
	if (threadPixels != NULL){
		delete[] threadPixels;
		threadPixels = NULL;
	}
	
	pthread_mutex_destroy(&pixelsMutex);
	
	tex.clear();
	
}

//--------------------------------------------------------------------
void ofxVideoGrabberProsilica::videoSettings(void){
    ofLog(OF_LOG_NOTICE, "videoSettings not implemented");
}

//------------------------------------
void ofxVideoGrabberProsilica::setUseTexture(bool bUse){
	bUseTexture = bUse;
}

//we could cap these values - but it might be more useful
//to be able to set anchor points outside the image

//----------------------------------------------------------
void ofxVideoGrabberProsilica::setAnchorPercent(float xPct, float yPct){
    if (bUseTexture)tex.setAnchorPercent(xPct, yPct);
}

//----------------------------------------------------------
void ofxVideoGrabberProsilica::setAnchorPoint(int x, int y){
    if (bUseTexture)tex.setAnchorPoint(x, y);
}

//----------------------------------------------------------
void ofxVideoGrabberProsilica::resetAnchor(){
   	if (bUseTexture)tex.resetAnchor();
}

//------------------------------------
void ofxVideoGrabberProsilica::draw(float _x, float _y, float _w, float _h){
	if(bUseTexture) {
		tex.draw(_x, _y, _w, _h);
	}
}

//------------------------------------
void ofxVideoGrabberProsilica::draw(float _x, float _y){
	draw(_x, _y, (float)width, (float)height);
}


//----------------------------------------------------------
float ofxVideoGrabberProsilica::getHeight(){
	return (float)height;
}

//----------------------------------------------------------
float ofxVideoGrabberProsilica::getWidth(){
	return (float)width;
}

// callback called when a frame is done
void FrameDoneCB(tPvFrame* pFrame)
{
	ofxVideoGrabberProsilica * grabber = (ofxVideoGrabberProsilica *)pFrame->Context[0];

	grabber->iWaitingForFrames--;
	
	if(pFrame->Status == ePvErrSuccess){

		ofLog(OF_LOG_VERBOSE, "frame done");

		if (strcmp(grabber->pixelFormat, "Bayer8") == 0) {
			
			int BPP = 3; //samplesPerPixel
			UInt32 linePadding = 0;
			UInt32 lineSize; // = width * BPP + linePadding;
			UInt32 bufferSize;
			unsigned char* buffer;
			
			lineSize = (grabber->width * BPP) + linePadding;
			bufferSize = lineSize * grabber->height;
			buffer = (unsigned char*) malloc(bufferSize);
			
			if (BPP == 3) {  
				PvUtilityColorInterpolate(pFrame, &buffer[0], &buffer[1], &buffer[2], 2, linePadding);
//				memcpy(&buffer[0], pFrame->ImageBuffer, pFrame->ImageBufferSize);
//				memcpy(&buffer[pFrame->ImageBufferSize], pFrame->ImageBuffer, pFrame->ImageBufferSize);
//				memcpy(&buffer[pFrame->ImageBufferSize*2], pFrame->ImageBuffer, pFrame->ImageBufferSize);
			}
			
			UInt8 *srcPtr = &buffer[0];
			
			pthread_mutex_lock(&(grabber->pixelsMutex));

			memcpy(grabber->threadPixels, srcPtr, bufferSize);
			grabber->doneFrameCount = pFrame->FrameCount;

			pthread_mutex_unlock(&(grabber->pixelsMutex));

				
			free(buffer);
			
		} else if (strcmp(grabber->pixelFormat, "Rgb24") == 0) {
			
			pthread_mutex_lock(&(grabber->pixelsMutex));

				memcpy(grabber->threadPixels, pFrame->ImageBuffer, pFrame->ImageSize);
			grabber->doneFrameCount = pFrame->FrameCount;

			pthread_mutex_unlock(&(grabber->pixelsMutex));

		} else {

			pthread_mutex_lock(&(grabber->pixelsMutex));

			memcpy(grabber->threadPixels, pFrame->ImageBuffer, grabber->width*grabber->height);
			grabber->doneFrameCount = pFrame->FrameCount;

			pthread_mutex_unlock(&(grabber->pixelsMutex));

		}
	}
	
	// if the frame was completed (or if data were missing/lost) we re-enqueue it
	if(pFrame->Status == ePvErrSuccess  || 
	   pFrame->Status == ePvErrDataLost ||
	   pFrame->Status == ePvErrDataMissing){
		PvCaptureQueueFrame(grabber->camera.Handle,pFrame,FrameDoneCB); 
		grabber->iWaitingForFrames++;        
		ofLog(OF_LOG_VERBOSE, "frame requeued");
	}
}




#pragma mark PROSILICA CODE


/*
 | ==============================================================================
 | Copyright (C) 2006-2007 Prosilica.  All Rights Reserved.
 |
 | Redistribution of this header file, in original or modified form, without
 | prior written consent of Prosilica is prohibited.
 |
 |==============================================================================
 |
 | This sample code, open the first camera found on the host computer and streams
 | frames from it until the user terminate it
 |
 |==============================================================================
 |
 | THIS SOFTWARE IS PROVIDED BY THE AUTHOR "AS IS" AND ANY EXPRESS OR IMPLIED
 | WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF TITLE,
 | NON-INFRINGEMENT, MERCHANTABILITY AND FITNESS FOR A PARTICULAR  PURPOSE ARE
 | DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 | INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 | LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 | OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED  AND ON ANY THEORY OF
 | LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 | NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 | EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 |
 |==============================================================================
 */

/*
 
 #ifdef _WINDOWS
 #include "StdAfx.h"
 #endif
 
 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>
 
 #ifdef _WINDOWS
 #define WIN32_LEAN_AND_MEAN
 #include <Windows.h>
 #include <Winsock2.h>
 #endif
 
 #if defined(_LINUX) || defined(_QNX) || defined(_OSX)
 #include <unistd.h>
 #include <pthread.h>
 #include <signal.h>
 #include <sys/times.h>
 #include <arpa/inet.h>
 #endif
 
 #include <PvApi.h>
 
 #ifdef _WINDOWS
 #define _STDCALL __stdcall
 #else
 #define _STDCALL
 #define TRUE     0
 #endif
 
 #define FRAMESCOUNT 10
 
 typedef struct 
 {
 unsigned long   UID;
 tPvHandle       Handle;
 tPvFrame        Frames[FRAMESCOUNT];
 #ifdef _WINDOWS
 HANDLE          ThHandle;
 DWORD           ThId;
 #else
 pthread_t       ThHandle;
 #endif    
 bool            Abort;
 
 } tCamera;
 
 
 // global camera data
 tCamera         GCamera;
 
 #if defined(_LINUX) || defined(_QNX) || defined(_OSX)
 struct tms      gTMS;
 unsigned long   gT00 = times(&gTMS);
 
 void Sleep(unsigned int time)
 {
 struct timespec t,r;
 
 t.tv_sec    = time / 1000;
 t.tv_nsec   = (time % 1000) * 1000000;    
 
 while(nanosleep(&t,&r)==-1)
 t = r;
 }
 
 unsigned long GetTickCount()
 {
 unsigned long lNow = times(&gTMS);
 
 if(lNow < gT00)
 gT00 = lNow;
 
 return (unsigned long)((float)(lNow - gT00) * 10000000.0 / (float)CLOCKS_PER_SEC);
 }
 
 void SetConsoleCtrlHandler(void (*func)(int), int junk)
 {
 signal(SIGINT, func);
 }
 #endif
 
 #ifdef _WINDOWS
 unsigned long __stdcall ThreadFunc(void *pContext)
 #else
 void *ThreadFunc(void *pContext)
 #endif
 {
 unsigned long Completed,Dropped,Done;
 unsigned long Before,Now,Total,Elapsed;
 unsigned long Missed,Errs;
 double Fps;
 float Rate;
 tPvErr Err;
 
 Fps = 0;
 Elapsed = 0;
 Total = 0;
 Done = 0;
 Before = GetTickCount();
 
 while(!GCamera.Abort &&
 !(Err = PvAttrUint32Get(GCamera.Handle,"StatFramesCompleted",&Completed)) &&
 !(Err = PvAttrUint32Get(GCamera.Handle,"StatFramesDropped",&Dropped)) &&
 !(Err = PvAttrUint32Get(GCamera.Handle,"StatPacketsMissed",&Missed)) &&
 !(Err = PvAttrUint32Get(GCamera.Handle,"StatPacketsErroneous",&Errs)) &&
 !(Err = PvAttrFloat32Get(GCamera.Handle,"StatFrameRate",&Rate)))
 {
 Now = GetTickCount();
 Total += Completed - Done;
 Elapsed += Now-Before;
 
 if(Elapsed >= 500)
 {
 Fps = (double)Total * 1000.0 / (double)Elapsed;
 Elapsed = 0;
 Total = 0;
 }
 
 printf("completed : %9lu dropped : %9lu missed : %9lu err. : %9lu rate : %5.2f (%5.2f)\r",
 Completed,Dropped,Missed,Errs,Rate,Fps);
 Before = GetTickCount();
 Done = Completed;
 
 Sleep(20);
 }
 
 printf("\n");
 
 return 0;
 }
 
 // spawn a thread
 void SpawnThread()
 {
 #ifdef _WINDOWS	
 GCamera.ThHandle = CreateThread(NULL,NULL,ThreadFunc,&GCamera,NULL,&(GCamera.ThId));
 #else
 pthread_create(&GCamera.ThHandle,NULL,ThreadFunc,(void *)&GCamera);
 #endif    
 }
 
 // wait for the thread to be over
 void WaitThread()
 {
 #ifdef _WINDOWS		
 WaitForSingleObject(GCamera.ThHandle,INFINITE);
 #else
 pthread_join(GCamera.ThHandle,NULL);
 #endif
 }
 
 // wait for a camera to be plugged
 void WaitForCamera()
 {
 printf("waiting for a camera ...\n");
 while(!PvCameraCount() && !GCamera.Abort)
 Sleep(250);
 printf("\n");
 }
 
 // wait forever (at least until CTRL-C)
 void WaitForEver()
 {
 while(!GCamera.Abort)
 Sleep(500);
 }
 
 // callback called when a frame is done
 void _STDCALL FrameDoneCB(tPvFrame* pFrame)
 {
 // if the frame was completed (or if data were missing/lost) we re-enqueue it
 if(pFrame->Status == ePvErrSuccess  || 
 pFrame->Status == ePvErrDataLost ||
 pFrame->Status == ePvErrDataMissing)
 PvCaptureQueueFrame(GCamera.Handle,pFrame,FrameDoneCB);  
 }
 
 // get the first camera found
 bool CameraGrab()
 {
 tPvUint32 count,connected;
 tPvCameraInfo list;
 
 count = PvCameraList(&list,1,&connected);
 if(count == 1)
 {
 GCamera.UID = list.UniqueId;
 printf("grabbing camera %s\n",list.SerialString);
 return true;
 }
 else
 return false;
 }
 
 // open the camera
 bool CameraSetup()
 {
 return !PvCameraOpen(GCamera.UID,ePvAccessMaster,&(GCamera.Handle));   
 }
 
 // open the camera
 bool CameraSetup(unsigned long IP)
 {
 return !PvCameraOpenByAddr(IP,ePvAccessMaster,&(GCamera.Handle));
 }
 
 // setup and start streaming
 bool CameraStart()
 {
 unsigned long FrameSize = 0;
 
 // Auto adjust the packet size to max supported by the network, up to a max of 8228.
 // NOTE: In Vista, if the packet size on the network card is set lower than 8228,
 //       this call may break the network card's driver. See release notes.
 //
 //PvCaptureAdjustPacketSize(GCamera.Handle,8228);
 
 // how big should the frame buffers be?
 if(!PvAttrUint32Get(GCamera.Handle,"TotalBytesPerFrame",&FrameSize))
 {
 bool failed = false;
 
 // allocate the buffer for each frames
 for(int i=0;i<FRAMESCOUNT && !failed;i++)
 {
 GCamera.Frames[i].ImageBuffer = new char[FrameSize];
 if(GCamera.Frames[i].ImageBuffer)
 GCamera.Frames[i].ImageBufferSize = FrameSize;
 else
 failed = true;
 }
 
 if(!failed)
 
 {
 // set the camera is acquisition mode
 if(!PvCaptureStart(GCamera.Handle))
 {
 // start the acquisition and make sure the trigger mode is "freerun"
 if(PvCommandRun(GCamera.Handle,"AcquisitionStart") ||
 PvAttrEnumSet(GCamera.Handle,"FrameStartTriggerMode","Freerun"))
 {
 // if that fail, we reset the camera to non capture mode
 PvCaptureEnd(GCamera.Handle) ;
 return false;
 }
 else                
 {
 // then enqueue all the frames
 
 for(int i=0;i<FRAMESCOUNT;i++)
 PvCaptureQueueFrame(GCamera.Handle,&(GCamera.Frames[i]),FrameDoneCB);
 
 printf("frames queued ...\n");
 
 return true;
 }
 }
 else
 return false;
 }
 else
 return false;
 }
 else
 return false;
 }
 
 // stop streaming
 void CameraStop()
 {
 printf("stopping streaming\n");
 PvCommandRun(GCamera.Handle,"AcquisitionStop");
 PvCaptureEnd(GCamera.Handle);  
 }
 
 // unsetup the camera
 void CameraUnsetup()
 {
 // dequeue all the frame still queued (this will block until they all have been dequeued)
 PvCaptureQueueClear(GCamera.Handle);
 // then close the camera
 PvCameraClose(GCamera.Handle);
 
 // delete all the allocated buffers
 for(int i=0;i<FRAMESCOUNT;i++)
 delete [] (char*)GCamera.Frames[i].ImageBuffer;
 }
 
 // CTRL-C handler
 #ifdef _WINDOWS
 BOOL WINAPI CtrlCHandler(DWORD dwCtrlType)
 #else
 void CtrlCHandler(int Signo)
 #endif	
 {  
 GCamera.Abort = true;    
 
 #ifndef _WINDOWS
 signal(SIGINT, CtrlCHandler);
 #else
 return true;
 #endif
 }
 
 int main(int argc, char* argv[])
 {
 // initialise the Prosilica API
 if(!PvInitialize())
 { 
 memset(&GCamera,0,sizeof(tCamera));
 
 SetConsoleCtrlHandler(&CtrlCHandler, TRUE);
 
 // the only command line argument accepted is the IP@ of the camera to be open
 if(argc>1)
 {
 unsigned long IP = inet_addr(argv[1]);
 
 if(IP != INADDR_NONE)
 {
 // setup the camera
 if(CameraSetup(IP))
 {
 // strat streaming from the camera
 if(CameraStart())
 {
 printf("camera is streaming now. Press CTRL-C to terminate\n");
 
 // spawn a thread
 SpawnThread();
 
 // we wait until the user press CTRL-C
 WaitForEver();
 
 // then wait for the thread to finish
 if(GCamera.ThHandle)
 WaitThread(); 
 
 // stop the camera
 CameraStop();                        
 }
 else
 printf("failed to start streaming\n");
 
 // unsetup the camera
 CameraUnsetup();
 }
 else
 printf("failed to setup the camera\n");                
 }
 
 }
 else
 {
 // wait for a camera to be plugged
 WaitForCamera();
 
 // grab a camera from the list
 if(!GCamera.Abort && CameraGrab())
 {
 // setup the camera
 if(CameraSetup())
 {
 // strat streaming from the camera
 if(CameraStart())
 {
 printf("camera is streaming now. Press CTRL-C to terminate\n");
 
 // spawn a thread
 SpawnThread();
 
 // we wait until there is no more camera
 WaitForEver();
 
 // then wait for the thread to finish
 if(GCamera.ThHandle)
 WaitThread();     
 
 // stop the camera
 CameraStop();                            
 }
 else
 printf("failed to start streaming\n");
 
 // unsetup the camera
 CameraUnsetup();
 }
 else
 printf("failed to setup the camera\n");
 }
 else
 printf("failed to find a camera\n");
 }
 
 // uninitialise the API
 PvUnInitialize();
 }
 else
 printf("failed to initialise the API\n");
 
 
 return 0;
 }
 
 */