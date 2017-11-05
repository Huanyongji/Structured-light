#include "pgcamera.hpp"

#include <QApplication>
#include <QMetaType>
#include <QTime>
#include <QMap>

#include <stdio.h>
#include <iostream>

PGCamera::PGCamera(QObject  * parent):
QThread(parent),

    _camera_index(-1),
    _init(false),
    _stop(false)
{
    qRegisterMetaType<cv::Mat>("cv::Mat");
    cameraInd = -1;

}

PGCamera::~PGCamera()
{
    stopCamera();
}


unsigned int PGCamera::connectCamera()
{
    Error error;
    BusManager busMgr;
    unsigned int numCameras = -1;
    error = busMgr.GetNumOfCameras(&numCameras);
    if (error != PGRERROR_OK)   //PGRERROR_OK = 0
    {
        error.PrintErrorTrace();
        return 0;
    }
    cout << "Number of cameras detected: " << numCameras << endl;
    return numCameras;
}

void PGCamera::waitForStart(void)
{
    while (isRunning() && !_init)
    {
        QApplication::processEvents();
    }
}


void PGCamera::run()
{
    _init = false;
    _stop = false;

    bool success = startCamera();

    _init = true;

    if (!success)
    {
        return;
    }

    int error = 0;
    int max_error = 10;
    int warmup = 10000;
    QTime timer;
    timer.start();
    while(&cam && !_stop && error<max_error)
    {
        /*IplImage * frame = cvQueryFrame(_pg_camera);
        if (frame)
        {   //ok
            error = 0;
            emit new_image(cv::Mat(frame));
        }
        else
        {   //error
            if (timer.elapsed()>warmup) {error++;}
        }
        */
        cv::Mat frame;
        Image tmpImage;
        perror=cam.RetrieveBuffer(&tmpImage);
        if(perror!=PGRERROR_OK){
           perror.PrintErrorTrace();
        }

        if (tmpImage.GetData())
        {   //ok
             //_num_images++;
             unsigned int rowBytes=tmpImage.GetReceivedDataSize()/tmpImage.GetRows();
             frame = cv::Mat(tmpImage.GetRows(), tmpImage.GetCols(), CV_8UC1,  tmpImage.GetData(),rowBytes);
            emit new_image(cv::Mat(frame));
         }
        else
        {   //error
            if (timer.elapsed()>warmup) {error++;}
        }
    }

    //clean up
    stopCamera();

    QApplication::processEvents();
}

void PGCamera::PrintCameraInfo( CameraInfo* pCamInfo )
{
    cout << endl;
    cout << "*** CAMERA INFORMATION ***" << endl;
    cout << "Serial number -" << pCamInfo->serialNumber << endl;
    cout << "Camera model - " << pCamInfo->modelName << endl;
    cout << "Camera vendor - " << pCamInfo->vendorName << endl;
    cout << "Sensor - " << pCamInfo->sensorInfo << endl;
    cout << "Resolution - " << pCamInfo->sensorResolution << endl;
    cout << "Firmware version - " << pCamInfo->firmwareVersion << endl;
    cout << "Firmware build time - " << pCamInfo->firmwareBuildTime << endl << endl;

}
QStringList PGCamera::list_devices(void)
{
    unsigned int number = connectCamera();
    QStringList list;
    char deviceName[255] = {0};

    for(int i=0;i<number;i++)
    {
        deviceName[i] = (char)i;
        list.append(deviceName);
    }
    return list;

}
bool PGCamera::startCamera(int cameraIndex)
{
    cameraInd = cameraIndex;
    if (cameraInd<0)
    {
        cameraInd = 0;
    }

    Error error;
    BusManager busMgr;
    PGRGuid guid;
    error = busMgr.GetCameraFromIndex(cameraInd, &guid);
    if (error != PGRERROR_OK)
    {
        error.PrintErrorTrace();
        return false;
    }
    error = cam.Connect(&guid);//判断摄像机是否正确连接
    if (error != PGRERROR_OK)
    {
        error.PrintErrorTrace();
        return false;
    }

    // Get the camera information
    error = cam.GetCameraInfo(&camInfo);
    if (error != PGRERROR_OK)
    {
        error.PrintErrorTrace();
        return false;
    }
    // Get the camera configuration
    FC2Config config;
    error = cam.GetConfiguration( &config );
    if (error != PGRERROR_OK)
    {
        error.PrintErrorTrace();
        return false;
    }

    // Set the number of driver buffers used to 10.
    config.numBuffers = 10;

    // Set the camera configuration
    error = cam.SetConfiguration( &config );
    if (error != PGRERROR_OK)
    {
        error.PrintErrorTrace();
        return false;
    }
    error = cam.StartCapture();
    if (error != PGRERROR_OK)
    {
        error.PrintErrorTrace();
        return false;
    }

	Property Exposure_time,Gain;
	Exposure_time.type = FlyCapture2::AUTO_EXPOSURE;
	Exposure_time.autoManualMode = false;
	Exposure_time.absControl = true;
	Exposure_time.absValue =1.30;

	Gain.type = FlyCapture2::GAIN;
	Gain.absControl = true;
	Gain.absValue = 2.0;
	Gain.autoManualMode = false;
	cam.SetProperty(&Exposure_time,true);
	cam.SetProperty(&Gain,true);

    return true;
}


//--------------捕获图片---------------
void PGCamera::captureImage()
{
    // Start capturing images
    Error error;
    Image tmpImage;
    error = cam.RetrieveBuffer( &tmpImage );
    if (error != PGRERROR_OK)
    {
        error.PrintErrorTrace();
        return;
    }
        // Convert the raw image
    error = tmpImage.Convert( PIXEL_FORMAT_RGB8, &imageCamera );
    if (error != PGRERROR_OK)//PGRERROR_OK表示函数执行成功，没有错误返回，该枚举可以表示函数执行的状态
    {
        error.PrintErrorTrace();
        return;
    }

}


//------------停止摄像机的工作-----------------
void PGCamera::stopCamera()
{
    Error error;
    // Stop capturing images
    error = cam.StopCapture();
    if (error != PGRERROR_OK)
    {
        error.PrintErrorTrace();
        return;
    }

    // Disconnect the camera
    error = cam.Disconnect();
    if (error != PGRERROR_OK)
    {
        error.PrintErrorTrace();
        return;
    }
    cameraInd = -1;

}

