# Name: makefile
# Description: makefile for project 1 for ECEN 5013 AESD
# Toolchain: gcc compiler version 7.4.0


CROSS_COMPILE =# Cross compile option for arm-unknown-linux-gnueabi-

ifeq ($(CC),)
	CC = $(CROSS_COMPILE)gcc
endif

ifeq ($(LD),)
	LD = ld  # Linker
endif

ifeq ($(CCFLAGS),)
	CCFLAGS = -O0 -g -Wall -Werror 
endif

ifeq ($(LDFLAGS),)
	LDFLAGS = -pthread -lrt
endif

DEPS = queue.h # header files
OBJ =  seqgen.o capture.o 
CPPLIBS= -lopencv_core -lopencv_flann -lopencv_video
TARGET = seqgen


all: $(TARGET)


seqgen: $(OBJ)
	$(CC) $(CCFLAGS) -o $@ $^ $(LDFLAGS) -lstdc++ `pkg-config --libs opencv` $(CPPLIBS) 

# %.o: %.c $(DEPS)
# 	$(CC) $(CCFLAGS) -c -o $@ $<  

clean:
	-rm -f seqgen  *.o *.s *.d

#.c.o:
#	$(CC) $(CCFLAGS) -c $<

#.cpp.o:
#	$(CC) $(CCFLAGS) -lstdc++ -c $<
