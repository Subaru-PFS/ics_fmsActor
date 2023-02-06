/* this is a general-purpose cli code to run Zwo ASI1600 monochrome
 * cameras. See the Makefile for dependencies. For it to work, you
 * must have a large enough USB buffer and, for non-root users, a
 * suitable entry in /etc/udev/rules.d. See README
 */

/*NB!! _OPENCV is defined in the Makefile (or not) */

#include <stdio.h>
#include "ASICamera2.h"
#include <unistd.h>
#include <math.h>
#include <stdlib.h>
#include <argp.h>
#include "csimfitshdr.h"  /* defines u_short, u_char */
#ifdef _OPENCV
#include "opencv2/highgui/highgui_c.h"
#endif

#define MAXCAM 8          /* maximum number of cameras */
static int vflg = 0;      /* verbosity level 0-2 */

extern int asnap(int CamNum, int exp_ms, int gain, u_char *pBuf, int vflg);
extern int writeasifits(int width,int height,int camnum, int exp_ms,
    int asi_gain, u_short *buf, char *fitsname);
extern int writeasipgm(int width,int height,u_short *buf,int pfac,char* name);

/* use argp to process args for asi_snap */

const char *argp_program_version = "asi_snap 0.5";
const char *argp_program_bug_address = "";

/* 
 * args are exptime(ms) and gain(cB), integers.
 * options are 
 * -v ( -vv, verbosity )
 * -f ( writes snapN.fits or snapNAMW.fits )
 * -F ( writes FITS file to the given filename )
 * -d ( default 0; !=0 enables display and sets reduction factor img->screen )
 * -c ( camera index id; default 0)
 * -p ( default 0; !=0 enables PGM output and sets reduction factor img->file )
 * -n camname ( uses optional nonvolatile camera name instead of camera index )
 * -s camname  (if invoked, allows setting nonvolatile camera id name and exits)
 */

/* This structure is used by main to communicate with parse_opt. */
/* NB!!!! if you declare POINTERS here, as you probably will, there
 * is no STORAGE associated, so be careful when you initialize before
 * calling the parser. eg arguments.camsetname = "junk.fits" is OK, but
 * strcpy(arguments.camsetname,"foobar") will cause a segfault.
 */
struct arguments {
  char *args[2];            /* exptime and gain */
  int vlevel;               /* The int (012)associated with the  -v flag */
  char *fitsname;           /* Argument for -o */
  int fitsflg;              /* flag requesting fitsfile, set by -f */
  char *disp;               /* Argument for -d (string) */
  char *camid;              /* camera ID (string) */
  char *pgmstr;             /* Argument for -p (string) */
  char *camsetname;         /* Nonvolatile Camera name to set */
  int setnameflg;           /* flag requesting name setting, by -n string */
  char *camname;            /* input identifying camname */
  int bynameflg;            /* flag for camera id by name instead of index*/
};
        
/* OPTIONS.  Field 1 in ARGP. Order of fields: {NAME, KEY, ARG, FLAGS, DOC}.*/
static struct argp_option options[] =
{
  {"verbose",   'v', 0,           0, "Produce verbose output; -vv for more"},
  {"disp",      'd', "DisRedFac", 0, "Enable display & set reduction factor"}, 
  {"foutput",   'f', 0,           0, "Write output to simple FITS file"},
  {"fitsname",  'F', "FITSname",  0, "FITS output filename"},
  {"camid",     'c', "CameraID",  0, "Which Camera? (index, or use -n name)"},
  {"pgmstr",    'p', "PGMRedfac", 0, "Write PGM prevu file snap.pgm, reduced"},
  {"camsetname",'s', "CamSetName",0, "Set nonvolatile CamName and exit"},
  {"camname",   'n', "CamName",   0, "Input camera ID name"},
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
        arguments->fitsflg = 1;
        break;
    case 'F':
        arguments->fitsflg = 1;
        arguments->fitsname = arg;
        break;
    case 'p':
        arguments->pgmstr = arg;
        break;
    case 's':
    	arguments->camsetname = arg;
    	arguments->setnameflg = 1;
    	break;	        
    case 'n':
    	arguments->camname = arg;
    	arguments->bynameflg   = 1;
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
        save output to a simple fits file or .pgm file .";

/* The ARGP structure itself.*/
static struct argp argp = {options, parse_opt, args_doc, doc};
         
int main (int argc, char **argv)
{
        struct arguments arguments;
        int exp_ms = 0;
        int asi_gain = 0;
        int Dfac   = 0;
        int CamNum = -1;
        int vflg = 0;
        char ffname[128];
        char pgmname[128];
        char jpgname[128];
        int foutflg = 0;  /* FITS output */
        int pfac    = 0;  /* PGM output to snap.pgm */
        int bDisplay = 0; /* display OpenCV display loop is running */
        char nostr[] = "";
        char zstr[] = "0";
        char camsetnameid[8]; 
        int setname = 0;
        char camnameid[8];
        char gotcamname[8];
        int byname = 0;
    
        arguments.vlevel       =  0 ;
        arguments.fitsflg      =  0 ; 
        arguments.setnameflg   =  0 ;
        arguments.bynameflg    =  0 ;
        arguments.camid        = (char *)"*";
        arguments.disp         = (char *)"0";
        arguments.pgmstr       = (char *)"0";
        arguments.camsetname   = (char *)"" ;
        arguments.camname      = (char *)"" ;
        arguments.fitsname     = (char *)"" ;

        argp_parse (&argp, argc, argv, 0, 0, &arguments);
        
        exp_ms   = atoi(arguments.args[0]);
        asi_gain = atoi(arguments.args[1]);
        Dfac     = atoi(arguments.disp);
        if(arguments.camid != (char *)"*")
            CamNum   = atoi(arguments.camid);
        vflg     = arguments.vlevel;
        pfac     = atoi(arguments.pgmstr);
        strncpy(camsetnameid,arguments.camsetname,8);
        strncpy(camnameid, arguments.camname, 8);
        strncpy(ffname, arguments.fitsname, 128);
        
        /*printf("\nCamNum = %d, Dfac = %d\n",CamNum, Dfac);*/

        if(arguments.fitsflg != 0){
            foutflg = 1;
        }
        if(arguments.setnameflg != 0){
            setname = 1;  /* setting name */
            vflg = 2;     /* turn on max verbosity */
        }		
      	if(arguments.bynameflg != 0){
            byname = 1;
        }	
      
        if(vflg > 1){
            printf("INPUT PARAMETERS");
            printf("\nExptime = %d ms", exp_ms);
            printf("\nGain = %d centiBels",asi_gain);
            printf("\nfoutflg = %d",foutflg);
            if(strlen(ffname) > 0){
                printf("\nfitsname = %s",ffname);
            }
            printf("\nverbosity = %d",vflg);
            printf("\ndisp Red Fac = %d",Dfac);
            if(byname){
                printf("\nCamNameID = %s",camnameid);
            }else{
                printf("\nCamID = %d", CamNum);
            }
            printf("\nPGMout = %d",pfac);
            if(setname) 
            	printf("\nSetting Camera %d ID to %s",CamNum,camsetnameid);
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
	int camid;
	char asinamestr[64];

        u_char *pCv8bit; /* display data line pointer */
        
	ASI_CAMERA_INFO ASICameraInfo;
	ASI_ID camnamear[MAXCAM];  /* don't need */
	ASI_ID setid;
        
        /* find out about camera(s) */
	int numDevices = ASIGetNumOfConnectedCameras();
	int neednum = 0;
        if(numDevices > 1 && CamNum == -1 && byname == 0){
            printf(
"There are %d cameras connected; you MUST specify a name or index\n",numDevices);
            neednum = 1;
        }
	if(numDevices <= 0){
            printf("\nNo camera connected. Check connection or connect camera\n");
            exit(-1);
	}else{
            if(vflg > 1) printf("attached cameras:");
	}

	if( CamNum > numDevices - 1){    /* no Camera, outside range */
	    printf("\nThere is no such connected camera as %d\n",CamNum);
	    exit(-1);
	}
	    

        /* name setting code */
        if( setname ){
            if(CamNum < 0){
                printf("You MUST give a valid camera number to set the name");
                return -2;
            }
            if(ASIOpenCamera(CamNum) != ASI_SUCCESS){
	        printf("Cannot open camera %d\n", CamNum);
	        printf("Camera %d is not connected, or you have not set udev permissions\n");
	        printf("if the latter, you must be root to run the camera\n");
                return -2;
            }
            strncpy((char *)(setid.id), camsetnameid, 8);
            setid.id[7] = 0;
            ASISetID(CamNum, setid);
            ASICloseCamera(CamNum);
        }
        
        
        /*if no camera number was set and there is only one camera, CamNum=0**/
                   
	if(numDevices == 1) CamNum = 0;  /* only one camera */
	
        char *cp, *cp2;
        int gotname = 0 ; /* success flag */
        int hitname ;     /* success on THIS iteration */
   
	if(numDevices > 1){
            for(i = 0; i < numDevices; i++){          
                ASIGetCameraProperty(&ASICameraInfo, i);
                width = ASICameraInfo.MaxWidth;
                height = ASICameraInfo.MaxHeight;
                camid = ASICameraInfo.CameraID ;
                /* if we have a name, check with cameras as they go by */
                if(byname){
                    hitname = 0;
                    strncpy(asinamestr, ASICameraInfo.Name,64);
                    cp = strchr(asinamestr, '(' );
                    cp2 = strchr(asinamestr, ')' );
                    if(cp == (char *)0 || cp2 == (char *)0 || cp>cp2){
                        printf("\nCamera %d has no valid name set", i);
                        return -2;
                    }
                    cp++;
                    *cp2 = 0; /* terminate name, which is pointed to by cp */
                    if(strcmp(cp,camnameid) == 0 ){
                        CamNum = i;
                        hitname = 1;
                        gotname = 1;
                        strcpy(gotcamname,cp); /*gotcamname is short id string*/
                    }
                }
                if(vflg > 0){
                    printf("\n%1d %s %dX%d %d",
                        i, ASICameraInfo.Name, width, height, camid);
                    if(byname) printf("  name:%s",cp);	
                    if(byname && hitname) 
                        printf(" got name, Camnum= %d", CamNum);
                }

            }
            if(byname && gotname == 0){
                printf("\nNo camera named %s, sorry\n",camnameid);
                return -2;
            }
            if(vflg) printf("\n");

	}

        if(neednum){
            return -2;
        } 
  
        if(vflg) 
            printf("Camnum = %d  #connected cameras = %d\n",CamNum,numDevices);	
        ASIGetCameraProperty( &ASICameraInfo, CamNum );
        width = ASICameraInfo.MaxWidth;
        height = ASICameraInfo.MaxHeight; 
    
        imgSiz = width * height * sizeof(unsigned short);
        
        u_char* imgBuf = (unsigned char *)malloc(imgSiz);
        u_short *simgBuf = (u_short *)imgBuf;

        /* take picture !!!! */
        ret=asnap(CamNum, exp_ms, asi_gain, imgBuf, vflg);
        
        /* write fits file if requested */
       if(foutflg){
            if(strlen(ffname) > 0){
              ;
            }else if(byname == 0){
                sprintf(ffname,"/tmp/snap%d.fits",CamNum);
            }else{
                sprintf(ffname,"/tmp/snap%s.fits",gotcamname);                
            }
            if(vflg) printf("\nWriting data to %s", ffname);
            (void)writeasifits(
                width,height,CamNum,exp_ms,asi_gain,simgBuf,ffname);
        }
        /* write PGM file if specified*/
        if(pfac > 0){
            if(byname == 0){
                sprintf(pgmname,"/tmp/snap%d.pgm",CamNum);
            }else{
                sprintf(pgmname,"/tmp/snap%s.pgm",gotcamname);
            }
            if(vflg){ 
                int pwid=8*((width/pfac)/8);
                printf("\nWriting %dx%d preview to %s, red fac = %d",
                pgmname,pwid,height/pfac,pfac);
            }
            (void)writeasipgm(width,height,simgBuf,pfac,pgmname);
        }        

        /* now make display in main if requested -- uses imgBuf, height, wid */
        /* think about this--want to introduce -p -> Prefac for production of
         * snapshot .pgm file. Want #define OPENCV for presence of library, but want
         * things to work with or without. Maybe just duplicate this code with another
         * pointer, which we will have to malloc storage for. Might be best. Yes. Then
         * can do anything.
         */
#ifdef _OPENCV        
        if(Dfac > 0){
            /* set display parameters */
            int displayWid = width/Dfac;
            int displayHei = height/Dfac;
            displayWid = (displayWid/8) * 8 ;  
            displayHei = (displayHei/2) * 2 ;
                /* displayWid must be a multiple of 8; cv must use bitplanes;
                 * height must be even */
        
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
                pCv8bit += displayWid;
                pImg16bit   += Dfac*width;
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
#else
        if (Dfac != 0){
            printf(
            "\nThe OpenCV libraries are not available, so I cannot display; SORRY!\n\n");
        }
#endif            
        
        /* clean up */
	if(imgBuf) free(imgBuf);
	return 0;
}

/*********************** WRITEASIFITS() *************************************/

int
writeasifits(int width,int height,int camnum, int exp_ms,int asi_gain, 
    u_short *buf, char *fitsname )
{
    fhead *fh;
    FILE *outf;

    char a;
    char b;
    u_char *pix;

    outf = fopen(fitsname,"w");
    fh = newhead();
    newcard(fh,(char *)"SIMPLE",fh_Log,(char *)"T",(char *)0);    
    newcard(fh,(char *)"BITPIX",fh_Int,itoa(16), (char *)0);
    newcard(fh,(char *)"NAXIS" ,fh_Int, itoa(2), (char *)0);
    newcard(fh,(char *)"NAXIS1",fh_Int, itoa(width), 0);
    newcard(fh,(char *)"NAXIS2",fh_Int, itoa(height),0);
    newcard(fh,(char *)"CAMNUM",fh_Int, itoa(camnum),(char *)"ASI camera number (0-3)");
    newcard(fh,(char *)"EXP_MS",fh_Int, itoa(exp_ms),(char *)"Exptime in Milliseconds");
    newcard(fh,(char *)"GAIN_CB",fh_Int,itoa(asi_gain),(char *)"Gain in cB");
    timecard(fh);
    endcard(fh);

    printf("\nWRITING %s\n",fitsname);          

    fwrite((char *)fh->header,1,2880,outf);

    if(vflg > 1){ 
        /* print the header */
        char *cp = (char *)fh->header;
        printf("\n\n");
        for(int i=0;i<36;i++){
            if(*cp == ' ') break;   
            putchar('\n');
            for(int j=0; j<80; j++) putchar(*cp++);
        }
        printf("\n\n");    
        printf("%s\n", "Swapping the bytes");    

    }

    /* swap the bytes in place */        
    pix = (u_char *)buf;       
    for(int i = 0; i < width*height; i++){
        a = *pix;
        b = *(pix + 1);
        *pix++ = b;
        *pix++ = a;
    }

   
    if(vflg) printf("Writing the array\n");
    fwrite((char *)buf,1,width*height*2,outf);

    if(vflg > 1) printf("Swapping back\n");    
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
writeasipgm(int width,int height,unsigned short int *buf,int pfac,char *name)
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
    pgmout = fopen(name,"w");

    printf("\nWRITING PGM file %s\n",name);
    fwrite(header,1,strlen(header),pgmout);
    fwrite(pgmbuf,1,pgmWid*pgmHei,pgmout);
    fclose(pgmout);

    free(pgmbuf);
    return 0;
}

/*************************** WRITEASIJPG *************************************/
/* This file requires the libopencv_imageprocessing library */
int 
writeasijpg(int width,int height,unsigned short int *buf,int jfac, char *name)
{
    int jpgWid = width/jfac;
    int jpgHei = height/jfac;
    jpgWid = (jpgWid/8) * 8;  /* multiple of 8, JFTHOI */
    FILE *jpgout;
    char header[64];
            
    /* create uchar  array and clear it */
    int jpgSize = jpgWid*jpgHei;
    u_char *jpgbuf=(u_char *)malloc(jpgSize);
    memset(jpgbuf, 0, jpgSize);
    
    /* populate file buffer with scaled and subsampled image*/

    u_char *p8bit = jpgbuf;
    u_short *p16bit = buf;
    for(int y = 0; y < jpgHei; y++){
        for( int j=0; j < jpgWid; j++){
            p8bit[j] = (p16bit[jfac*j]>>8);
        }
        p8bit+=jpgWid;
        p16bit+=jfac*width;
    }
    printf("\nWRITING JPEG file %s", name);
    cvSaveImage(name, jpgbuf);    
    free(jpgbuf);
    return 0;
}

/********************************************************************************************/

