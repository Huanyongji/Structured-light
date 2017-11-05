/*


*/
#ifndef PGCAMERA_HPP
#define PGCAMERA_HPP


#include <iostream>
#include "FlyCapture2.h"
#include "FlyCapture2Gui.h"
#include <QThread>
#include <QStringList>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>

using namespace FlyCapture2;
using namespace std;
//image size
#define CAM_WIDTH 1280
#define CAM_HEIGHT 1024


class PGCamera : public QThread
{
    Q_OBJECT

public:

    PGCamera(QObject * parent = 0);
    ~PGCamera();
    inline void stop(void) {_stop = true;}
    inline void set_camera_index(int index) {_camera_index = index;}
    inline int get_camera_index(void) const {return _camera_index;}
   //int get_next_image(cv::Mat& image) ;
    void PrintCameraInfo( CameraInfo* pCamInfo );//
    bool startCamera(int cameraIndex = 0);//
    void stopCamera();
    void captureImage();
    unsigned int connectCamera();   //return camera no, if return 0, means fail 
    void waitForStart(void);
    QStringList list_devices(void);

signals:
    void new_image(cv::Mat image);

protected:
    virtual void run();

public:
    Camera cam;
    CameraInfo camInfo;
    Image imageCamera;   //the image of camera
    int cameraInd;
private:

    //bool start_camera(void);
    //void stop_camera(bool force = false);
    int _camera_index;
    bool _video_capture;
    volatile bool _init;//volatile
    volatile bool _stop;
    Error perror;
    BusManager busMgr;
    PGRGuid guid;
    Property props;
    TriggerMode Mode;
};

#endif // PGCAMERA_HPP

