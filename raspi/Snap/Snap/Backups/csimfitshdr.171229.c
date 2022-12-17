/* code for simple (one-block) fits headers and files */
#include "csimfitshdr.h"

#if 0  /* in header */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <ctype.h>

/* csimfitshdr.h */
#ifdef u_char
#undef u_char
#endif

#define u_char unsigned char
#define min(a,b) ((a) < (b) ? (a) : (b))

typedef struct fhead{
    int cno;
    char header[2880];
} fhead;

enum ctype{
    fh_Log,
    fh_Str,
    fh_Int,
    fh_Flt,
    fh_Cmt,
    fh_His,
    fh_End,
};

#endif

/* for debugging, brings in main() test program */

#if 0
#define FITSTEST
#endif

/********** NEWHEAD() **************************************************************/
/* Allocates and sets up a new fhead structure */

fhead * 
newhead()
{
    fhead *ptr;    
    ptr = (fhead *)malloc(sizeof(fhead));
    ptr->cno = 0;
    memset(ptr->header,' ',2880);

#ifdef xTEST 
    printf("\nptr=%u, ptr->header=%u, ptr->cno=%u\n",ptr,ptr->header,ptr->cno);
    *(ptr->header) = '*';
    printf("\n%s"," 0:"); 
    /*for(int i = 0; i < 80; i++) putchar(*((ptr->header) + i));*/
#endif

    return ptr;
}

/**************** ITOA(), FTOA() ***************************************************/
/* convert ints and floats(doubles) to strings for feeding newcard() valp */

char *itoa(int i)
{
    char buf[64];
    static char *ibuf = (char *)0;
    
    sprintf(buf,"%d",i);
    int len = strlen(buf);
    if(ibuf) free(ibuf);
    ibuf = (char *)malloc(len);
    strcpy(ibuf,buf);
    return ibuf;
}    

char * ftoa(double f)
{
    char buf[64];
    static char *fbuf = (char *)0;
    
    sprintf(buf,"%f",f);
    int len = strlen(buf);
    if(fbuf) free(fbuf);
    fbuf = (char *)malloc(len);
    strcpy(fbuf,buf);
    return fbuf;
}

/************************ NEWCARD() *************************************************/
/* makes a new card and inserts it in next open slot in fhead */    

extern int isprint(int c);

void
newcard(fhead *hdr, char *key, int type, char *valp, char *comment)
{
    int card = hdr->cno;
    int klen ;
    char *cp = hdr->header + 80*card;
    int nk ;
    char num[80];
    int comflg = (comment != 0);
    char *cmp = 0;  
    int n;
    float f;
    double d;
    int len;

    klen = strlen(key);
    nk = min(klen,8);
    strncpy(cp, key, nk);
    /*printf("\nkey=%s nk=%d card=%d cp=%u",key,nk,card,(unsigned long)cp); */
    if(type < fh_Cmt) strncpy(cp + 8,"= ",2);    
    /*printf("\nType, key, valp, card = %d %s %s %d\n", type, key, valp, card);*/

    switch(type){
    case fh_Log:
        if(strcmp(valp,"T") != 0 && strcmp(valp,"F") != 0){
            printf("\nBooleans must be 'T' or 'F'");
            exit(-1);
        };
        strncpy(cp + 29,valp,1);
        cmp = cp + 31;
        break;
    case fh_Str:
        *(cp+10) = '\'';  /* col 11 */
        len = min(strlen(valp),67);
        strncpy(cp + 11, (char *)valp, len);
        if(len<8) len = 8;
        *(cp + 11 + len) = '\'';
        cmp = cp + len + 11 + 2;
        break;
    case fh_Int:
        n = atoi(valp);
        sprintf(num,"%d",n);
        len = strlen(num);
        strncpy(cp + 30 - len, num, len);
        cmp = cp + 31;
        break;
    case fh_Flt:
        f = atof(valp);
        sprintf(num,"%8.8E",f);
        len = strlen(num);
        strncpy(cp + 30 - len, num, len);
        cmp = cp + 31;
        break;
    case fh_Cmt:
    case fh_His:
        len = min(strlen(valp),72);
        strncpy(cp+8,valp,len);
        if(comflg){
            printf("\nNo inline comments on COMMENT or HISTORY cards");
            cmp = 0;
        }
        break;
    case fh_End:
    break;        
    default:
        printf("\nUnknown type %d",type);         
    }
    /* deal with comments */    
    /* printf("\ncmp,cp=%u %u",cmp,cp); */
    if(cmp && (comflg != 0) ){
        *cmp++ = '/';
        len = min(strlen(comment),(int)(cp + 80 - cmp));
        /* printf(" len=%d, com=%s",len,comment); */        
        if(len > 0) strncpy(cmp,comment,len);
    }
    /* check integrity */
    for(int i = 0; i<80; i++){
        if(!isprint(cp[i])){
            printf("\nCharacter %d in Card %d is illegal; value=%d",i,hdr->cno,cp[i]);
        }
    }
    (hdr->cno)++;
    return;
}

/*************** TIME/DATE STUFF ****************************************/
/* Borrowed from mirella:mirunix.c */

/* creates FITS CURRENT datestring for timestamping yyyy-mm-dd.  */
char *fitsdatestr()
{
    struct timeval times;
    long clock;
    static char datestr[30];

    gettimeofday(&times,NULL);
    clock = times.tv_sec ;
    struct tm m_ltime = *((struct tm *)gmtime(&clock));
    sprintf(datestr,"%04d-%02d-%02d",m_ltime.tm_year+1900,m_ltime.tm_mon + 1,
        m_ltime.tm_mday);
    return datestr;
}


/* creates FITS Current (UT) date/timestring yyyy-mm-ddThh:mm:ss */ 
char *fitstimestr()
{
    struct timeval times;
    long clock;
    static char datestr[30];

    gettimeofday(&times,NULL);
    clock = times.tv_sec ;
    struct tm m_ltime = *((struct tm *)gmtime(&clock));
    sprintf(datestr,"%04d-%02d-%02dT%02d:%02d:%02d",m_ltime.tm_year+1900,
        m_ltime.tm_mon + 1,m_ltime.tm_mday,
        m_ltime.tm_hour,m_ltime.tm_min, m_ltime.tm_sec);
    return datestr;
}

void
timecard(fhead *hdr)
{
    newcard(hdr, "DATE", fh_Str, fitstimestr(), "UT");
}

void
endcard(fhead *hdr)
{
    newcard(hdr, "END", fh_End, 0, 0);
}

#ifdef FITSTEST
/******************** TEST STUFF *********************************************/

static char gauge[81] =
"123456789A123456789A123456789A123456789A123456789A123456789A123456789A123456789A";

/* prints cardno(2):cardimage; set screen to at least 83 wide */
static void 
pcard(int n, char *card)
{
    int i;
    if(n < 0) printf("\n  ");
    else printf("\n%2d",n);
    putchar(':');
    for(i=0; i<80; i++) putchar(*(card+i));
}

/*newcard(fhead *hdr, char *key, int type, char *valp, char *comment)*/


int main()
{
    fhead *fh;
    int i;

    fh = newhead();
    printf("\n         HEADER:");

    pcard(-1,gauge);

    newcard(fh,"SIMPLE",fh_Log,"T","Natch");
    pcard(fh->cno - 1,fh->header);    

    newcard(fh,"BITPIX",fh_Int,itoa(16), 0);
    pcard(fh->cno - 1,fh->header+80*((fh->cno) - 1));    

    newcard(fh,"NAXIS",fh_Int, "2", "Its an IMAGE, NO????");
    pcard(fh->cno - 1,fh->header+80*((fh->cno) - 1));    

    newcard(fh,"NAXIS1",fh_Int, "4656", 0);
    pcard(fh->cno - 1,fh->header+80*((fh->cno) - 1));    

    newcard(fh,"NAXIS2",fh_Int,itoa(3520), "The Ysize deserves a comment which is almost certainly too long for this card");
    pcard(fh->cno - 1,fh->header+80*((fh->cno) - 1));    

    printf("\n  :%80s",gauge);

    newcard(fh,"FLOATPARAM",fh_Flt,ftoa(123456.7890), "More Digits?");
    pcard(fh->cno - 1,fh->header+80*((fh->cno) - 1));    

    newcard(fh,"COMMENT",fh_Cmt,"All of this is shit", 0);
    pcard(fh->cno - 1,fh->header+80*((fh->cno) - 1));    

    newcard(fh,"STRING",fh_Str,"ObservatoryInJungleInBrazil", "No Comment LOL");
    pcard(fh->cno - 1,fh->header+80*((fh->cno) - 1));    

    newcard(fh,"SHORTSTRING",fh_Str,"Obser", "Short Name, No??");
    pcard(fh->cno - 1,fh->header+80*((fh->cno) - 1));    
    
    timecard(fh);
    pcard(fh->cno - 1,fh->header+80*((fh->cno) - 1));    

    endcard(fh);
    pcard(fh->cno - 1,fh->header+80*((fh->cno) - 1));    
    printf("\n  :%80s",gauge);
    
    printf("\n\n\n");

    for(i=0; i<36; i++){
      /*  if(i%4 == 0){
            putchar('\n');   
            fflush(stdout);
            printf("  :%s",gauge);
        } */
        pcard(i,fh->header+80*i);
    }
    printf("\n\n");
    return 0;
}


#endif /*FITSTEST*/

