#include <opencv2/objdetect/objdetect.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#include <iostream>
#include <stdio.h>

using namespace std;
using namespace cv;


int main( int argc, const char** argv )
{
    CvCapture* capture = 0;
    Mat frame, frameCopy, image;

    capture = cvCreateCameraCapture( 0 ); //0=default, -1=any camera, 1..99=your camera
    if(!capture) cout << "No camera detected" << endl;

    if( capture )
    {
        for(;;)
        {
            IplImage* img = cvQueryFrame( capture );
            frame = Mat(img);
            if( frame.empty() )
                break;
            if( iplImg->origin == IPL_ORIGIN_TL )
                frame.copyTo( frameCopy );
            else
                flip( frame, frameCopy, 0 );

            if( waitKey( 10 ) >= 0 )
                cvReleaseCapture( &capture );
        }
    return 0;
    }
}

