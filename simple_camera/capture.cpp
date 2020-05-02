/*
 *
 *  Example by Berak
 *  Source: https://answers.opencv.org/question/77638/cvqueryframe-always-return-null/
 *
 */

// std related
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

// Opencv Related
#include "opencv2/opencv.hpp"

// Time related
#include <time.h>
#include <unistd.h>


#define CAPTURE_APP

using namespace cv;
//extern "C" int capture_write(int);
int capture_write(int dev, char * filename)
{
    VideoCapture cap(dev); // open the default camera
    if(!cap.isOpened())  // check if we succeeded
    {
        printf("Device is not opened\n");
        return -1;
    }

    Mat frame;
    Mat frame_resized;
    cap >> frame; // get a new frame from camera


    bool retval = false; 
    // resize image down to 320x240
    // resize(frame, frame_resized, Size(320,240), 0.5, 0.5,INTER_LINEAR);


    /* Add timestamp directly in image */
    time_t t ;
    struct tm *tmp ;
    char MY_TIME[100];
    time( &t );
    tmp = localtime( &t );
    // using strftime to display time
    strftime(MY_TIME, sizeof(MY_TIME), "#timestamp:%a, %d %b %Y %T %z \n", tmp);
    size_t str_size = strlen(MY_TIME);
    putText(frame,MY_TIME,Point(10, 40),FONT_HERSHEY_SIMPLEX,1,Scalar(255, 255, 255),2);  


    /* Add timestamp directly as a comment in image */
    //write_size = write(fd, MY_TIME, (size_t) str_size);
             




    // write image to file
    retval = imwrite(filename, frame);// save image to file
    if (retval == false)
    {
        //printf("Save image failed\n");
        return -1;
    }
    //printf("Resized image saved \n");
    // the camera will be deinitialized automatically in VideoCapture destructor
    return 0;
}

#ifdef CAPTURE_APP
int main( int argc, char** argv )
{
    int dev=0;

    if(argc > 1)
    {
        // use /dev/video<#>
        sscanf(argv[1], "%d", &dev);
        printf("using %s\n", argv[1]);
    }
    else if(argc == 1)
        // use default /dev/video0
        printf("using default\n");

    else
    {
        // specific usage
        printf("usage: capture [dev]\n");
        exit(-1);
    }
    printf("Start Capture and write\n");
    int retval = -1;
    char filename[] = "cap.ppm";
    retval = capture_write(dev, filename);
    if (retval < 0)
    {
        printf("error in capture_write function\n");
        return -1;
    }
    // the camera will be deinitialized automatically in VideoCapture destructor
    return 0;
}
#endif