/* this is a prototype version which includes a skeleton main program */

#include <stdio.h>
#include "opencv2/highgui/highgui_c.h"
#include "ASICamera2.h"
#include <unistd.h>
#include <math.h>
#include <stdlib.h>
#include <argp.h>
#include "csimfitshdr.h"  /* defines u_short, u_char */

static int bDisplay = 0;

extern int asnap(int CamNum, int exp_ms, int gain, u_char *pBuf, int vflg);
extern int writeasifits(int width,int height,int exp_ms,int asi_gain, u_short *buf, char *fitsname);
extern int writeasipgm(int width,int height,u_short *buf,int pfac);

/* use argp to process args for asi_snap */

const char *argp_program_version = "asi_snap 0.5";
const char *argp_program_bug_address = "";

/* 
 * args are exptime(ms) and gain(cB), integers.
 * options are 
 * -v ( -vv, verbosity )
 * -f fitsfilename
 * -d ( default 0; !=0 enables display and sets reduction factor img->screen )
 * -c ( camid; default 0)
 * -p ( default 0; !=0 enables PGM output and sets reduction factor img->file )
 */

/* This structure is used by main to communicate with parse_opt. */
struct arguments {
      char *args[2];            /* exptime and gain */
      int vlevel;               /* The int (012)associated with the  -v flag */
      char *fitsname;           /* Argument for -o */
      char *disp;               /* Argument for -d (string) */
      char *camid;              /* camera ID (string) */
      char *pgmstr;             /* Argument for -p (string) */
};
        
/* OPTIONS.  Field 1 in ARGP. Order of fields: {NAME, KEY, ARG, FLAGS, DOC}.*/
static struct argp_option options[] =
{
    {"verbose", 'v', 0,           0, "Produce verbose output; -vv for more"},
    {"disp",    'd', "DisRedFac", 0, "Enable display & set reduction factor"}, 
    {"foutput", 'f', "Outfile",   0, "Write output to simple FITS file"},
    {"camid",   'c', "CameraID",  0, "Which Camera?"},
    {"pgmstr",  'p', "PGMRedfac", 0, "Write PGM preview file snap.pgm, reduced"},
    {0}
};
                                
                             
/*
 * PARSER. Field 2 in ARGP.
 * Order of parameters: KEY, ARG, STATE.
 * IMHO it is better to keep this simple; string args whenever
 * possible
 */
static error_t
parse_opt (int key, char *arg, struct argp_state *state)
{
    struct arguments *arguments = (struct arguments *)state->input;
                                          
    switch (key)
    {
    case 'v':
        arguments->vlevel ++;
        break;
    case 'c':
        arguments->camid = arg;
        break;        
    case 'd':
        arguments->disp = arg;
        break;
    case 'f':
        arguments->fitsname = arg;
printf("\n passed fitsname = %s\n", arguments->fitsname);
        break;
    case 'p':
        arguments->pgmstr = arg;
        break;
    case ARGP_KEY_ARG:
        if (state->arg_num >= 2)
        {
           argp_usage(state);
        }
        arguments->args[state->arg_num] = arg;
        break;
    case ARGP_KEY_END:
        if (state->arg_num < 2)
        {
          argp_usage (state);
        }
        break;
    default:
        return ARGP_ERR_UNKNOWN;
    }
    return 0;
}
      
/* ARGS_DOC. Field 3 in ARGP. A description of the non-option 
 * command-line arguments that we accept. */
static char args_doc[] = "Exptime(ms) Gain(cB)";
          
/* DOC.  Field 4 in ARGP.  Program documentation. */
static char doc[] =
    "asi_snap -- A program to operate the ASI1600 camera and display and/or \n\
        save output to a simple fits file.";

/* The ARGP structure itself.*/
static struct argp argp = {options, parse_opt, args_doc, doc};
         
int main (int argc, char **argv)
{
        struct arguments arguments;
        int exp_ms = 0;
        int asi_gain = 0;
        int Dfac   = 0;
        int CamNum = 0;
        int vflg;
        unsigned char ffname[128];
        int foutflg = 0;  /* FITS output */
        int pfac    = 0;  /* PGM output to snap.pgm */
    
        arguments.vlevel = 0;
        arguments.disp = (char *)"";
        arguments.fitsname = (char *)"";
        arguments.pgmstr = (char *)"";

        argp_parse (&argp, argc, argv, 0, 0, &arguments);

        exp_ms   = atoi(arguments.args[0]);
        asi_gain = atoi(arguments.args[1]);
        Dfac     = atoi(arguments.disp);
        CamNum   = atoi(arguments.camid);
        vflg     = arguments.vlevel;
        pfac     = atoi(arguments.pgmstr);

        if(arguments.fitsname[0] != 0){
            foutflg = 1;
            strcpy(ffname,arguments.fitsname);
        }
       
        if(vflg > 1){
            printf("INPUT PARAMETERS");
            printf("\nExptime = %d ms", exp_ms);
            printf("\nGain = %d centiBels",asi_gain);
            printf("\noutfile = %s",ffname);
            printf("\nfoutflg = %d",foutflg);
            printf("\nverbosity = %d",vflg);
            printf("\ndisp Red Fac = %d",Dfac);
            printf("\nCamID = %d", CamNum);
            printf("\nPGMout = %d",pfac);
            printf("\n\n");
        }
        
        /* compute ASI gain as multiplier: */
        double asi_fgain = ((double)asi_gain/200.)/exp(10.);
        
	int width;
	int height;
	int i, j;
	char c;
        int bin = 1;
        int Image_type = 2; /* unsigned short, ~32MB */
	int ret;
	int imgSiz;


        u_char *pCv8bit; /* display data line pointer */
        
        /* find out about camera */
	int numDevices = ASIGetNumOfConnectedCameras();
	if(numDevices <= 0){
            printf("no camera connected, press any key to exit\n");
            getchar();
            return -1;
	}else{
            if(vflg > 1) printf("attached cameras:\n");
	}

	ASI_CAMERA_INFO ASICameraInfo;
	if(numDevices > 1 && vflg > 1){
            for(i = 0; i < numDevices; i++){          
                ASIGetCameraProperty(&ASICameraInfo, i);
                width = ASICameraInfo.MaxWidth;
                height = ASICameraInfo.MaxHeight;
                if(vflg > 0)
                printf("%d %s %dX%d\n",i, ASICameraInfo.Name, width, height);
            }
	}
	
        ASIGetCameraProperty(&ASICameraInfo, CamNum);
        width = ASICameraInfo.MaxWidth;
        height = ASICameraInfo.MaxHeight;
    
        imgSiz = width * height * sizeof(unsigned short);
        
        u_char* imgBuf = (unsigned char *)malloc(imgSiz);
        u_short *simgBuf = (u_short *)imgBuf;

        /*return -1;*/  /* debug */

        /* take picture !!!! */
        ret=asnap(CamNum, exp_ms, asi_gain, imgBuf, vflg);
        
        /* write fits file */
        if(foutflg){
            if(vflg) printf("\nWriting data to %s", ffname);
            (void)writeasifits(width,height,exp_ms,asi_gain,simgBuf,ffname);
        }
        /* write PGM file */
        if(pfac){
            if(vflg){ 
                int pwid=8*((width/pfac)/8);
                printf("\nWriting %dx%d preview to snap.pgm, red fac = %d",
                pwid,height/pfac,pfac);
            }
            (void)writeasipgm(width,height,simgBuf,pfac);
        }        

        /* now make display in main if requested -- uses imgBuf, height, wid */
        /* think about this--want to introduce -p -> Prefac for production of
         * snapshot .pgm file. Want #define OPENCV for presence of library, but want
         * things to work with or without. Maybe just duplicate this code with another
         * pointer, which we will have to malloc storage for. Might be best. Yes. Then
         * can do anything.
         */
        
        if(Dfac > 0){
            /* set display parameters */
            int displayWid = width/Dfac;
            int displayHei = height/Dfac;
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
                    pCv8bit[j] = (pImg16bit[Dfac*j]>>8);
                }
                pCv8bit+=displayWid;
                pImg16bit+=Dfac*width;
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
            cvReleaseImage(&pRgb);
        }
        
        /* write fits file if requested
        if(arguments.fitsname[0]){
            
           
        }
        /* clean up */
	if(imgBuf) free(imgBuf);
	return 1;
}


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
            printf("\nData in Middle: %d %d %d %d \n ", 
                *ldat++, *ldat++, *ldat++, *ldat ++);
                               
        } else {
            printf("Exposure failed, status = %d  ", status);
            return -1;
        }

	ASIStopExposure(CamNum);
	ASICloseCamera(CamNum);
	if(vflg > 1) printf("Camera closed\n");
	return 0 ;
}

/*********************** WRITEASIFITS() *************************************/

int
writeasifits(int width,int height,int exp_ms,int asi_gain, u_short *buf, char *fitsname )
{
    fhead *fh;
    FILE *outf;

    char a;
    char b;
    u_char *pix;

printf("\n%s","WRITEASIFITS: composing the header\n");
    outf = fopen(fitsname,"w");
    fh = newhead();
    newcard(fh,"SIMPLE",fh_Log,"T",0);    
    newcard(fh,"BITPIX",fh_Int,itoa(16), 0);
    newcard(fh,"NAXIS",fh_Int, "2", 0);
    newcard(fh,"NAXIS1",fh_Int, itoa(width), 0);
    newcard(fh,"NAXIS2",fh_Int, itoa(height),0);
    newcard(fh,"EXP_MS",fh_Int, itoa(exp_ms),"Exptime in Milliseconds");
    newcard(fh,"GAIN_CB",fh_Int,itoa(asi_gain),"Gain in cB");
    timecard(fh);
      
printf("Writing the header\n");
    printf("\nWRITING %s",fitsname);    
    fwrite((char *)fh->header,1,2880,outf);

char *cp = fh->header;
printf("\n\n");
for(int i=0;i<36;i++){
    if(*cp == ' ') break;   
    putchar('\n');
    for(int j=0; j<80; j++) putchar(*cp++);
}
printf("\n\n");    

printf("%s\n", "Swapping the bytes");    
    /* swap the bytes in place */        
    pix = (u_char *)buf;       
    for(int i = 0; i < width*height; i++){
        a = *pix;
        b = *(pix + 1);
        *pix++ = b;
        *pix++ = a;
    }
   
printf("Writing the array\n");
    fwrite((char *)buf,1,width*height*2,outf);

printf("Swapping back\n");    
    /* swap the bytes in place */        
    pix = (u_char *)buf;       
    for(int i = 0; i < width*height; i++){
        a = *pix;
        b = *(pix + 1);
        *pix++ = b;
        *pix++ = a;
    }

    fclose(outf);
    
    return 0;
}

/*************************** WRITEASIPGM *************************************/

int 
writeasipgm(int width,int height,unsigned short int *buf,int pfac)
{
    int pgmWid = width/pfac;
    int pgmHei = height/pfac;
    pgmWid = (pgmWid/8) * 8;  /* multiple of 8, JFTHOI */
    FILE *pgmout;
    char header[64];
            
    /* create uchar  array and clear it */
    int pgmSize = pgmWid*pgmHei;
    u_char *pgmbuf=(u_char *)malloc(pgmSize);
    memset(pgmbuf, 0, pgmSize);
    
    /* populate file buffer with scaled and subsampled image*/

    u_char *p8bit = pgmbuf;
    u_short *p16bit = buf;
    for(int y = 0; y < pgmHei; y++){
        for( int j=0; j < pgmWid; j++){
            p8bit[j] = (p16bit[pfac*j]>>8);
        }
        p8bit+=pgmWid;
        p16bit+=pfac*width;
    }
    sprintf(header,"P5 %d %d 255\n",pgmWid,pgmHei);
    pgmout = fopen("snap.pgm","w");

    printf("\nWRITING PGM file snap.pgm");
    fwrite(header,1,strlen(header),pgmout);
    fwrite(pgmbuf,1,pgmWid*pgmHei,pgmout);
    fclose(pgmout);

    free(pgmbuf);
    return 0;
}


/*****************************************************************************/
