#include "stdio.h"
#include "ASICamera2.h"
#include <unistd.h>
#include <stdlib.h>

/********************** ASNAP() **********************************************
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
asnap(int CamNum, int exp_ms, int gain, unsigned char *pBuf, int vflg)
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

	if(vflg > 0) printf("\n%s information\n",ASICameraInfo.Name);

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
	if(vflg>0) printf("sensor temperature:%5.2f", (float)ltemp/10.0);

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
	
        if(vflg > 0) printf("\nimgSize= %d",imgSize); 

        /* set gain (units are cB (!!!) */
	ASISetControlValue(CamNum, ASI_GAIN, gain, ASI_FALSE);
	if(vflg > 0) printf("\nGain set to %d centiBels", gain);
        
        /* set exposure time */
	ASISetControlValue(CamNum, ASI_EXPOSURE, exp_ms*1000, ASI_FALSE);

        /* set bandwidth overload--this matches cam output b/w with host capability */
	ASISetControlValue(CamNum, ASI_BANDWIDTHOVERLOAD, 40, ASI_FALSE); 

        /* start exposure */
        if(vflg > 1)printf("\n\nBeginning Exposure\n");

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
            if(vflg) printf("\nData in Middle: %d %d %d %d \n ", 
                *ldat++, *ldat++, *ldat++, *ldat ++);
                               
        } else {
            printf("Exposure failed, status = %d  ", status);
        }

	ASIStopExposure(CamNum);
	ASICloseCamera(CamNum);
	if(vflg > 1){
		printf("\nCamera closed\n");
	}
	if(status == ASI_EXP_SUCCESS) return 0 ;
	else return -1;
}

