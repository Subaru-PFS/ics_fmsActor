/* header for simple C fits header code csimfitshdr.c */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>

#define TEST

/* csimfitshdr.h */
#ifdef u_char
#undef u_char
#endif

#define u_char unsigned char

#ifdef u_short
#undef u_short
#endif

#define u_short unsigned short int

#define min(a,b) ((a) < (b) ? (a) : (b))

typedef struct fhead{
    int cno;
    u_char header[2880];
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

extern fhead * newhead();
extern char *itoa(int i);
extern char *ftoa(double f);
extern void newcard(fhead *hdr, char *key, int type, char *valp, char *comment);
extern char *fitsdatestr();
extern char *fitstimestr();
extern void timecard(fhead *hdr);
extern void endcard(fhead *hdr);





