/* this is a prototype version which includes a skeleton main program */

#include "stdio.h"
#include "opencv2/highgui/highgui_c.h"
#include "ASICamera2.h"
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <math.h>

static int bDisplay = 0;

static int Pred = 7;  /* map real pixels to display */

#define NARG 3
#define DEBUG

/* args for main program:  CamID, exptime, gain, Pred, filename  
 * use switches; defaults CamID to zero, gain to 0, Pred to 5, filename to out.fits
 */
int  main(int argc, char *argv[])
{
        if (argc != NARG+1){
            printf("\nUSAGE: snap exposuretime(ms) gain Pred");
            printf("\nGain is in centibels in range 0-300. Real gain G=exp10(gain/200)");
            printf("\nPred is factor by which image (4656x3520) is reduced to fit on your screen.");
            exit(-1);
        }
        int exp_ms =       atoi(argv[1]);
        int asi_gain =     atoi(argv[2]); /* gain in centibels */
        int Pred =         atoi(argv[3]);
        
        /* compute ASI gain in integral cB: */
        double asi_fgain = exp10((double)asi_fgain/200.);
        if(asi_gain < 0) asi_gain = 0;
       
        
	int width;
	int height;
	int i, j;
	char c;
        int bin = 1;
        int Image_type = 2; /* unsigned short, ~32MB */
        int iMaxwidth ;     /* 4656, read from camera */
        int iMaxheight ;    /* 3520, read from camera */
	int CamNum=0;

	IplImage *pRgb;

	unsigned short *ldat; /* QL data pointer */
        unsigned char *pCv8bit; /* display data line pointer */
        
        /* find out about camera */
	int numDevices = ASIGetNumOfConnectedCameras();
	if(numDevices <= 0){
            printf("no camera connected, press any key to exit\n");
            getchar();
            return -1;
	}else{
            printf("attached cameras:\n");
	}

	ASI_CAMERA_INFO ASICameraInfo;
	for(i = 0; i < numDevices; i++){                   /* no, just for selected camaera if exists */
            ASIGetCameraProperty(&ASICameraInfo, i);
            width = ASICameraInfo.MaxWidth;
            height = ASICameraInfo.MaxHeight;
            printf("%d %s %dX%d\n",i, ASICameraInfo.Name, width, height);
	}

        /* this goes away--camnum is an input
        if(numDevices > 1) {
            printf("\nselect one to use\n");
	    scanf("%d", &CamNum);
        }else{
            CamNum = 0;
        }



/* ABOVE STUFF GOES IN MAIN PROGRAM */
        /* get info for selected camera */
        ASIGetCameraProperty(&ASICameraInfo, CamNum);

	if(ASIOpenCamera(CamNum) != ASI_SUCCESS){
            if(CamNum > numDevices - 1){
                printf("Camera %d is not connected\n");
            }else{
                printf("OpenCamera error:\n");
                printf("If you have not set udev permissions, you must be root.\n");
                printf("Hit any key to exit\n");
		getchar();
		return -1;
            }
	}
	ASIInitCamera(CamNum);

	printf("%s information\n",ASICameraInfo.Name);
	int iMaxWidth, iMaxHeight;
	iMaxWidth = ASICameraInfo.MaxWidth;
	iMaxHeight =  ASICameraInfo.MaxHeight;
	printf("resolution:%dX%d\n", iMaxWidth, iMaxHeight);
	if(ASICameraInfo.IsColorCam){
		printf("This code only handles mono cameras; sorry");
		return -1;
	}else{
		printf("Mono camera\n");
	}
	
	ASI_CONTROL_CAPS ControlCaps;
	int iNumOfCtrl = 0;
	ASIGetNumOfControls(CamNum, &iNumOfCtrl);
	for( i = 0; i < iNumOfCtrl; i++){
		ASIGetControlCaps(CamNum, i, &ControlCaps);
		printf("%s\n", ControlCaps.Name);
	}

	long ltemp = 0;
	ASI_BOOL bAuto = ASI_FALSE;
	ASIGetControlValue(CamNum, ASI_TEMPERATURE, &ltemp, &bAuto);
	printf("sensor temperature:%5.2f\n", (float)ltemp/10.0);

        width = iMaxWidth;
        height = iMaxHeight ;   /* full-frame */
     
        do{
        }while(ASI_SUCCESS != ASISetROIFormat(CamNum, width, height, bin, (ASI_IMG_TYPE)Image_type));

	printf("\nset image format %d %d %d %d success"  , width, height, bin, Image_type);
        int rwid = -1, rht = -1, rbin = -1, rtype = -1;
	ASIGetROIFormat(CamNum, &rwid, &rht, &rbin, (ASI_IMG_TYPE*)&rtype);
        printf("\nrecovered parameters: %d %d %d %d", width, height, bin, rtype);

        long imgSize   = width*height*sizeof(short int);
	
        printf("\nimgSize= %d\n",imgSize); 

        /* allocate image buffer; we allocate full frame buffer and will fill it with two exposures with
         * two exposures with overlapping ROIs to work around exposure bug 
         */
        unsigned char* imgBuf = new unsigned char[imgSize];

        /* set gain (units are cB (!!!) */
	ASISetControlValue(CamNum, ASI_GAIN, asi_gain, ASI_FALSE);
	printf("\nGain set to %3.1f: %d centiBels", asi_fgain, asi_gain);
        
        /* set exposure time */
	ASISetControlValue(CamNum, ASI_EXPOSURE, exp_ms*1000, ASI_FALSE);

        /* set bandwidth overload--this matches cam output b/w with host capability */
	ASISetControlValue(CamNum, ASI_BANDWIDTHOVERLOAD, 40, ASI_FALSE); 

	ASI_EXPOSURE_STATUS status;

        printf("\nBeginning Exposure");

        ASIStartExposure(CamNum, ASI_FALSE);
        usleep(10000);//10ms
        status = ASI_EXP_WORKING;
        while(status == ASI_EXP_WORKING) ASIGetExpStatus(CamNum, &status); /* wait to finish */
        /* if successful, store image in imgBuf and write to display */ 
        if(status == ASI_EXP_SUCCESS){
            ASIGetDataAfterExp(CamNum, imgBuf, imgSize);
            /* note the imgBuf is uchar*, so offset must be given in BYTES */
            /* get some data for quick look; */
            ldat =  (unsigned short *)imgBuf + width*1760 + width/2 ;
            printf("\nData in Middle: %d %d %d %d \n ", *ldat++, *ldat++, *ldat++, *ldat ++);
                               
        } else {
            printf("Exposure failed, status =%d  ", status);            
        }

	ASIStopExposure(CamNum);
	ASICloseCamera(CamNum);
	printf("camera closed\n");

        /* done with camera; have data in imgBuf */

/* above stuff goes in function ASIsnap(int camID, int exptimems,unsigned short *imgBuf )

/* now make display in main program-- uses imgBuf, height, wid */


        /* set display parameters */
        int displayWid = iMaxWidth/Pred;
        int displayHei = iMaxHeight/Pred;
        displayWid = (displayWid/8) * 8;  /* displayWid must be a multiple of 8; cv must use bitplanes */
        
        /* create uchar DISPLAY image array */
        long displaySize = displayWid*displayHei;
        pRgb=cvCreateImage(cvSize(displayWid, displayHei), IPL_DEPTH_8U, 1);
        memset(pRgb->imageData, 0, displaySize);

        /* populate display buffer */
        pCv8bit = (unsigned char *)(pRgb->imageData);
        unsigned short *pImg16bit =  (unsigned short *)(imgBuf); 
        for(int y = 0; y < displayHei; y++){
            for( int j=0; j < displayWid; j++){
                pCv8bit[j] = (pImg16bit[Pred*j]>>8);
            }
            pCv8bit+=displayWid;
            pImg16bit+=Pred*width;
        }

        printf("\nHit ESC in image window to exit\n");   
	IplImage *pImg = (IplImage *)pRgb;
	cvNamedWindow("video", 1);

        bDisplay = 1;
        while(bDisplay){    
            cvShowImage("video", pImg);
            char c=cvWaitKey(1);
            if ( c == 27 ) bDisplay = 0;
        }

	/* printf("\nHit any key to exit");
	(void)getchar(); */

        /* clean up */

	if(imgBuf) delete[] imgBuf;
	cvReleaseImage(&pRgb);
	return 1;
}



