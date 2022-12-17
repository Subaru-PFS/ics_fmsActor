ver = debug
platform = x86
#platform = armv7
graph = opencv
#graph = none

#CC = g++
CC = gcc
#INCLIB = /usr/local/include
#LDLIB = /usr/local/lib
ifeq ($(graph), opencv)
OPENCV = -lopencv_core -lopencv_highgui -lopencv_imgproc#$(shell pkg-config --cflags opencv) $(shell pkg-config --libs opencv) -I/usr/include/opencv2
endif
#USB =  -I../libusb/include  -L../libusb/$(platform) -lusb-1.0  


LIBSPATH = -L../lib/$(platform) -I../include
                                                   
ifeq ($(ver), debug)
DEFS = -D_LIN -D_DEBUG 
#CFLAGS = -g  -I $(INCLIB) -L $(LDLIB) $(DEFS) $(COMMON) $(LIBSPATH)  -lpthread  -DGLIBC_20 -lm
CFLAGS = -g  -I $(INCLIB) -L $(LDLIB) $(DEFS) $(COMMON) $(LIBSPATH) -DGLIBC_20 -lm
else
DEFS = -D_LIN 
#CFLAGS =  -O3 -I $(INCLIB) -L $(LDLIB) $(DEFS) $(COMMON) $(LIBSPATH)  -lpthread  -DGLIBC_20
CFLAGS =  -O3 -I $(INCLIB) -L $(LDLIB) $(DEFS) $(COMMON) $(LIBSPATH) -DGLIBC_20
endif
ifeq ($(graph),opencv)
    DEFS += -D_OPENCV
endif

ifeq ($(platform), mac32)
CC = g++
CFLAGS += -D_MAC -m32
ifeq ($(graph), opencv)
OPENCV = -lopencv_calib3d -lopencv_contrib -lopencv_core -lopencv_features2d -lopencv_flann -lopencv_highgui -lopencv_imgproc -lopencv_legacy -lopencv_ml -lopencv_objdetect -lopencv_photo -lopencv_stitching -lopencv_ts -lopencv_video -lopencv_videostab -I/usr/local/include/opencv
endif
endif

ifeq ($(platform), mac64)
CC = g++
CFLAGS += -D_MAC -m64
ifeq ($(graph), opencv)
OPENCV = -lopencv_calib3d -lopencv_contrib -lopencv_core -lopencv_features2d -lopencv_flann -lopencv_highgui -lopencv_imgproc -lopencv_legacy -lopencv_ml -lopencv_objdetect -lopencv_photo -lopencv_stitching -lopencv_ts -lopencv_video -lopencv_videostab -I/usr/local/include/opencv
endif
endif

ifeq ($(platform), mac)
CC = g++
CFLAGS += -D_MAC -arch i386 -arch x86_64
ifeq ($(graph), opencv)
OPENCV = -lopencv_calib3d -lopencv_contrib -lopencv_core -lopencv_features2d -lopencv_flann -lopencv_highgui -lopencv_imgproc -lopencv_legacy -lopencv_ml -lopencv_objdetect -lopencv_photo -lopencv_stitching -lopencv_ts -lopencv_video -lopencv_videostab -I/usr/local/include/opencv
endif
endif

ifeq ($(platform), x86)
CFLAGS += -m32
CFLAGS += -lrt
endif


ifeq ($(platform), x64)
CFLAGS += -m64
CFLAGS += -lrt
endif

ifeq ($(platform), armv7)
CC = arm-linux-gnueabihf-g++
AR= arm-linux-gnueabihf-ar
CFLAGS += -march=armv7 -mthumb
endif

OFILES=	snap.o csimfitshdr.o snapfun.o

HFILES=	ASICamera2.h csimfitshdr.h


snap:   $(OFILES)
	$(CC)  $(CFLAGS) $(OFILES) $(OPENCV) -lASICamera2 -o snap
	cp snap ./bin/$(platform)/

fits:	csimfitshdr.c csimfitshdr.h
	$(CC) csimfitshdr.c -D FITSTEST -o fits

.PHONY:	
clean:
	rm -f *.o snap fits

