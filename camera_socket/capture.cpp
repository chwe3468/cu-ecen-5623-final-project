/*
 *
 *  Example by Berak 
 *  Source: https://answers.opencv.org/question/77638/cvqueryframe-always-return-null/
 *  Most added work done by Chutao
 */

// std related
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

// File related
#include <fcntl.h>
#include <unistd.h>

// Error related
#include <errno.h>
#include <syslog.h>

// Opencv Related
#include "opencv2/opencv.hpp"

// Time related
#include <sys/time.h>
#include <unistd.h>

// name related
#include <sys/utsname.h>

//#define CAPTURE_APP
#define PPM_HEADER_SIZE 3
#define BUF_SIZE 925696
#define NUM_CPU_CORES 1
#define DATE_TIME
#define SEC_MSEC_TIME
#define NAME

#define COMMENT_IN_IMAGE

using namespace cv;
extern "C" int capture_write(int dev, char * filename);
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
    struct tm *tmp ;
    char MY_TIME[128];
    char MY_SUB_TIME[40];
    char MY_NAME_BUF[128];
    struct timeval current_time_val;
    gettimeofday(&current_time_val, (struct timezone *)0);




#ifdef DATE_TIME
    tmp = localtime( &(current_time_val.tv_sec));
    // using strftime to display time
    strftime(MY_TIME, sizeof(MY_TIME), "#timestamp:%a, %d %b %Y %T %z \n", tmp);
    size_t str_size = strlen(MY_TIME);
    putText(frame,MY_TIME,Point(10, 40),FONT_HERSHEY_SIMPLEX,0.8,Scalar(255, 255, 255),2);  
#endif


#ifdef SEC_MSEC_TIME
    // using strftime to display time
    sprintf(MY_SUB_TIME, "# sec=%d, msec=%d\n",(int)current_time_val.tv_sec,(int)current_time_val.tv_usec/1000);
    size_t str_sub_size = strlen(MY_SUB_TIME);
    putText(frame,MY_SUB_TIME,Point(10, 80),FONT_HERSHEY_SIMPLEX,0.8,Scalar(255, 255, 255),2);  
#endif

#ifdef NAME
    struct utsname MY_NAME;
    uname(&MY_NAME);
    sprintf(MY_NAME_BUF, "# %s \n",MY_NAME.nodename);
    size_t str_name_size = strlen(MY_NAME_BUF);
    putText(frame,MY_NAME_BUF,Point(10, 120),FONT_HERSHEY_SIMPLEX,0.8,Scalar(255, 255, 255),2);  
#endif



    // write image to file
    retval = imwrite(filename, frame);// save image to file
    if (retval == false)
    {
        //printf("Save image failed\n");
        return -1;
    }
    //printf("Resized image saved \n");

    /* Add timestamp directly as a comment in image */
             
#ifdef COMMENT_IN_IMAGE
    /* open cap.ppm */
    int fd = open(filename,
            O_RDONLY/*|O_APPEND*/,
            S_IRWXU|S_IRWXG|S_IRWXO);
    if (fd < 0)
    {
        perror("Cannot open file");
    }
    // create a buffer for reading message
    void * local_buf = malloc(BUF_SIZE);
    if (local_buf == NULL)
    {
        perror("cannot malloc this much memory");
    }

    /* Read from /var/tmp/cap.ppm */
    int num_read = 1;
    bool EOF_flag = false;
    ssize_t read_size;
    ssize_t total_read_size;// assume ssize_t never overflow
    while(EOF_flag==false)
    {
        // receive
        read_size = read(fd, (((char *)local_buf)+(num_read-1)*BUF_SIZE), (size_t) BUF_SIZE);
        // update total_size
        total_read_size = ((num_read-1)*BUF_SIZE)+read_size;
        // check if read '\n'
        if(read_size < BUF_SIZE)
        {
            EOF_flag = true;
        }
        else
        {
            num_read++;
            local_buf = realloc(local_buf,num_read*BUF_SIZE);
            if (local_buf == NULL)
            {
                perror("cannot realloc this much memory");
            }
        }
    }
    //local_buf = realloc(local_buf,total_read_size);
    //ERROR_CHECK_NULL(local_buf);
    int error_code = close(fd);
    if (error_code != 0)
    {
        perror("close file error");
    }

    /* open image file at /var/tmp/cap_stamped.ppm */
    fd = open(filename,
            O_WRONLY|O_CREAT/*|O_APPEND*/,
            S_IRWXU|S_IRWXG|S_IRWXO);
    if (fd < 0)
    {
        perror("Cannot open file");
    }

    ssize_t write_size;
    // write to /var/tmp/cap_stamped.ppm
    write_size = write(fd, local_buf, (size_t) PPM_HEADER_SIZE);
    // Check for error
    if (write_size != PPM_HEADER_SIZE)
    {
        // Use errno to print error
        perror("PPM Header write error");
    }
#ifdef DATE_TIME
    // using strftime to display time
    write_size = write(fd, MY_TIME, (size_t) str_size);
    // Check for error
    if (write_size != str_size)
    {
        // Use errno to print error
        perror("timestamp write error");
        //exit(1);
    }
#endif

#ifdef SEC_MSEC_TIME
    // using strftime to display sub time
    write_size = write(fd, MY_SUB_TIME, (size_t) str_sub_size);
    // Check for error
    if (write_size != str_sub_size)
    {
        // Use errno to print error
        perror("timestamp write error");
        //exit(1);
    }
#endif

#ifdef NAME
    // using strftime to display sub time
    write_size = write(fd, MY_NAME_BUF, str_name_size);
    // Check for error
    if (write_size != str_name_size)
    {
        // Use errno to print error
        perror("timestamp write error");
        //exit(1);
    }
#endif


    write_size = write(fd, ((char *)local_buf)+PPM_HEADER_SIZE, (size_t) total_read_size-PPM_HEADER_SIZE);
    // Check for error
    if (write_size < total_read_size-PPM_HEADER_SIZE)
    {
        // Use errno to print error
        perror("ppm write error");
        //exit(1);
    }





    error_code = close(fd);
    if (error_code != 0)
    {
        perror("close file error");
    }
#endif

    // the camera will be deinitialized automatically in VideoCapture destructor
    return 0;
}


void print_scheduler(void);

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

void print_scheduler(void)
{
   int schedType;

   schedType = sched_getscheduler(getpid());

   switch(schedType)
   {
       case SCHED_FIFO:
           printf("Pthread Policy is SCHED_FIFO\n");
           break;
       case SCHED_OTHER:
           printf("Pthread Policy is SCHED_OTHER\n"); exit(-1);
         break;
       case SCHED_RR:
           printf("Pthread Policy is SCHED_RR\n"); exit(-1);
           break;
       default:
           printf("Pthread Policy is UNKNOWN\n"); exit(-1);
   }
}
