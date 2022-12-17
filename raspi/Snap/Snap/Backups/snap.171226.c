/* this is a prototype version which includes a skeleton main program */

#include "stdio.h"
#include "opencv2/highgui/highgui_c.h"
#include "ASICamera2.h"
#include <unistd.h>
#include <math.h>
#include <stdlib.h>
#include <argp.h>



const char *argp_program_version="snap_beta.171226";
static char doc[] =
" Program to operate Zwo ASI1600 camera, display image using the \n\
 opencv library and produce a simple fits file output";
 

#define NARG 3
#define DEBUG



static int bDisplay = 0;

static int Pred = 7;  /* map real pixels to display */



/* args for main program:  CamID, exptime, gain, filename, verbosity, display(Pred)
 * use switches; defaults CamID to zero, gain to 0, Pred to 5, filename to out.fits
 */

/* this version snap expms gain(cB) Pred */ 

int  main(int argc, char *argv[])
{
        int snap();
        
        
        
   
        if (argc != NARG+1){
            printf("\nUSAGE: snap exposuretime(ms) gain Pred");
            printf("\nGain is in centibels in range 0-300. Real gain G=exp10(gain/200)");
            printf("\nPred is factor by which image (4656x3520) is reduced to fit on your screen.");
            exit(-1);
        }
        int vflg = 2;
        int exp_ms =       atoi(argv[1]);
        int asi_gain =     atoi(argv[2]); /* gain in centibels */
        int Pred =         atoi(argv[3]);
        
        /* compute ASI gain as multiplier: */
        double asi_fgain = ((double)asi_gain/200.)/exp(10.);
        
	int width;
	int height;
	int i, j;
	char c;
        int bin = 1;
        int Image_type = 2; /* unsigned short, ~32MB */
	int CamNum=0;
	int ret;
	int imgSiz;


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
            if(vflg > 0)
                printf("%d %s %dX%d\n",i, ASICameraInfo.Name, width, height);
	}
        imgSiz = width * height * sizeof(unsigned short);
        
        unsigned char* imgBuf = (unsigned char *)malloc(imgSiz);

        ret=snap(CamNum, exp_ms, asi_gain, imgBuf, 2);

        /* now make display in main program-- uses imgBuf, height, wid */

        /* set display parameters */
        int displayWid = width/Pred;
        int displayHei = height/Pred;
        displayWid = (displayWid/8) * 8;  /* displayWid must be a multiple of 8; cv must use bitplanes */
        
        /* create uchar DISPLAY image array and clear it */
        long displaySize = displayWid*displayHei;
	IplImage *pRgb=cvCreateImage(cvSize(displayWid, displayHei), IPL_DEPTH_8U, 1);
	memset(pRgb->imageData, 0, displaySize);

        /* populate display buffer with scaled and subsampled image*/
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

	if(imgBuf) free(imgBuf);
	cvReleaseImage(&pRgb);
	return 1;
}


#include "stdio.h"
#include "ASICamera2.h"
#include <unistd.h>
#include <stdlib.h>

/********************** SNAP() **********************************************
 * 
 * function to operate ASI1600MM (only) cameras. The arguments are
 *
 * int CamNo            : Camera number (0-3). Returns with  -2 if requested
 *                              camera is not connected or other setup error.
 * int exp_ms           : Exposure time in milliseconds
 * int gain             : Gain in centibels (0-300 -> mult. gain 1-30)
 * unsigned char *pBuf  : Pointer to picture buffer, at least 32MB
 * int vflg             : verbosity level for debugging. 0=silent (except
 *                             for errors), 2=chatty
 * Returns 0 if successful, -1 if exposure fails, -2 if there is a fundamental
 * setup error: no camera, bad permissions, etc.
 */

int
snap(int CamNum, int exp_ms, int gain, unsigned char *pBuf, int vflg)
{
	int width, height;        
	int iNumOfCtrl = 0;  
	ASI_CAMERA_INFO ASICameraInfo;
	ASI_CONTROL_CAPS ControlCaps;
	int numDevices;
	long ltemp = 0;
	ASI_BOOL bAuto = ASI_FALSE;
	int imgSize;
	ASI_EXPOSURE_STATUS status;
	unsigned short *ldat;
	int i;
	int bin=1;
		
        /* get info for selected camera */

        ASIGetCameraProperty(&ASICameraInfo, CamNum);
	if(ASIOpenCamera(CamNum) != ASI_SUCCESS){
	    printf("Cannot open camera %d\n", CamNum);
	    printf("Camera %d is not connected, or you have not set udev permissions\n");
	    printf("if the latter, you must be root to run the camera\n");
            return -2;
	}
	ASIInitCamera(CamNum);

	if(vflg > 0) printf("%s information\n",ASICameraInfo.Name);

	width = ASICameraInfo.MaxWidth;
	height =  ASICameraInfo.MaxHeight;
	if(vflg > 0) printf("resolution:%dX%d\n", width, height);
	if(ASICameraInfo.IsColorCam){
		printf("This code only handles mono cameras; sorry");
		return -2;
	}else if(vflg == 2){
		printf("Mono camera\n");
	}

	ASIGetNumOfControls(CamNum, &iNumOfCtrl);
	for( i = 0; i < iNumOfCtrl; i++){
            ASIGetControlCaps(CamNum, i, &ControlCaps);
            if(vflg == 2)  printf("%s\n", ControlCaps.Name);
	}

	ASIGetControlValue(CamNum, ASI_TEMPERATURE, &ltemp, &bAuto);
	if(vflg>0) printf("sensor temperature:%5.2f\n", (float)ltemp/10.0);

        do{
        }while(ASI_SUCCESS != ASISetROIFormat(
            CamNum, width, height, bin, (ASI_IMG_TYPE)(2) ));

        if(vflg==2) printf("\nset image format %d %d %d %d success", 
            width, height, bin, 2);

        int rwid = -1, rht = -1, rbin = -1, rtype = -1;
	ASIGetROIFormat(CamNum, &rwid, &rht, &rbin, (ASI_IMG_TYPE*)&rtype);
	if(vflg == 2) printf("\nrecovered parameters: %d %d %d %d", 
             width, height, bin, rtype);

        imgSize   = width*height*sizeof(short int);
	
        if(vflg > 0) printf("\nimgSize= %d\n",imgSize); 

        /* set gain (units are cB (!!!) */
	ASISetControlValue(CamNum, ASI_GAIN, gain, ASI_FALSE);
	if(vflg > 0) printf("\nGain set to %d centiBels", gain);
        
        /* set exposure time */
	ASISetControlValue(CamNum, ASI_EXPOSURE, exp_ms*1000, ASI_FALSE);

        /* set bandwidth overload--this matches cam output b/w with host capability */
	ASISetControlValue(CamNum, ASI_BANDWIDTHOVERLOAD, 40, ASI_FALSE); 

        /* start exposure */
        if(vflg > 1)printf("\nBeginning Exposure");

        ASIStartExposure(CamNum, ASI_FALSE);
        usleep(10000); //10ms
        status = ASI_EXP_WORKING;
        /* wait to finish */
        while(status == ASI_EXP_WORKING) ASIGetExpStatus(CamNum, &status); 
        /* if successful, store image in pBuf and write to display */ 
        if(status == ASI_EXP_SUCCESS){
            ASIGetDataAfterExp(CamNum, pBuf, imgSize);
            /* note the pBuf is uchar*, so offset must be given in BYTES */
            /* get some data for quick look at level; */
            ldat =  (unsigned short *)pBuf + width*1760 + width/2 ;
            printf("\nData in Middle: %d %d %d %d \n ", 
                *ldat++, *ldat++, *ldat++, *ldat ++);
                               
        } else {
            printf("Exposure failed, status =%d  ", status);
            return -1;
        }

	ASIStopExposure(CamNum);
	ASICloseCamera(CamNum);
	if(vflg > 1) printf("Camera closed\n");
	return 0 ;
}

