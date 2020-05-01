/*
 *
 *  Example by Berak
 *  Source: https://answers.opencv.org/question/77638/cvqueryframe-always-return-null/
 *
 */
#include "opencv2/opencv.hpp"
            
using namespace cv;

int main(int, char**)
{
    VideoCapture cap(0); // open the default camera
    if(!cap.isOpened())  // check if we succeeded
    {
        printf("device not opend\n");
        return -1;
    }
        

    for(;;)
    {
        Mat frame;
        cap >> frame; // get a new frame from camera

        imshow("VideoCapture Basic Demo", frame);

        if(waitKey(30) == 'q') break;
    }
    // the camera will be deinitialized automatically in VideoCapture destructor
    return 0;
}
