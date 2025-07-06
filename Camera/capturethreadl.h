#ifndef CAPTURETHREADL_H
#define CAPTURETHREADL_H
#define SAVEIMG 1
#define RECVID  1

#include <iostream>
#include <cstdlib>
#include <fstream>

#include <QThread>
#include <QDialog>
#include <QObject>
#include <QImage>
#include <QMutex>
#include <QWaitCondition>
#include <QDebug>

#include <opencv2/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>

#include <pylon/PylonIncludes.h>
#include <pylon/gige/BaslerGigECamera.h>
#include "globalparms.h"

#ifdef SPINNAKER_CAM_API
#ifdef PYLON_WIN_BUILD
#include <pylon/PylonGUI.h>
#endif
#endif
#include <QElapsedTimer>

class ImageProcSettingDialog;

class CaptureThreadL: public QThread
{
    Q_OBJECT

private:
    bool stop;
    QMutex mutex;
    QWaitCondition condition;
    int frame_rate = 0;
    QImage last_image;

    // Opencv Object
    CvCapture *cam;
    //cv::Mat frame;
    cv::Mat current_img;

    int64_t videoFormat_w = 0;// add by garly, for adjust video format
    int64_t videoFormat_h = 0;// add by garly, for adjust video format
    int64_t videoFormat_offset_x = 0;// add by garly, for adjust video format
    int64_t videoFormat_offset_y = 0;// add by garly, for adjust video format
    bool videoFormat_flip = false;

    _CAMERA_SDK_ cam_model;

#ifdef SPINNAKER_CAM_API
    // Spinnaker
    SpinnakerCamera *spin_cam;
#endif
    int camera_rotation;   // -90, 0, 90, 180
    bool is_cam_connected;
    float exposure_time_;
    float gain_;
    bool use_offline_img = false;
    int systemType = 0;	//garly_20220310, for offline test
    bool is_capture_ready_ = true;  // Jang 20220408

    int device_idx_;    // 0 or 1
    int cam_fail_timeout_ = 10; // s
    struct timespec time_start, time_end;

    double clock_diff (struct timespec *t1, struct timespec *t2)
    { return t2->tv_sec - t1->tv_sec + (t2->tv_nsec - t1->tv_nsec) / 1e9; }

protected:
    void run();
    void msleep(int ms);
    // For reconnection
    static void Reconnect(Pylon::IPylonDevice* pDevice);

public:
    CaptureThreadL(QObject *parent = 0);
    ~CaptureThreadL();

    void Play();
    void Stop();
    bool isStopped() const;
    bool IsConnected();
    cv::Mat GetCurImg() { return current_img; }

    void setVideoFormat(int64_t offset_x, int64_t offset_y, int64_t w, int64_t h, bool flip);
    void setSourceMode(int src);
    // Jang 20220106
    void UseOfflineImg(bool use_offline);
    bool GetOfflineMode(){return use_offline_img;}
    void setOfflineSystemType(int type) {systemType = type;}	//garly_20220310, for offline test

    QMutex *mutex_shared;
    ImageProcSettingDialog * imageprocSettingDialog;
    void SetCameraModel(_CAMERA_SDK_ cam_model);
    bool using_default_parameter = false;
#ifdef SPINNAKER_CAM_API
    void SetSpinnakerPtr(SpinnakerCamera *spin_cam);
#endif

    // Jang modified 211125
    void SetCameraRotation(int rot){
        assert(rot == 0 || rot == 180 || rot == -90 || rot == 90);
        camera_rotation = rot;
    }
    void SetExposureGain(float exposure, float gain){
        this->exposure_time_ = exposure;
        this->gain_ = gain;
    }
    void SelectCam(int idx){
        device_idx_ = idx;
    }

signals:
    void ProcessedImgL(cv::Mat);
    void signalSendImgOnce(cv::Mat);

    void signalCameraLErrorMsg(QString);
    void signalMemChek();
    void signalSetEmptyImg(int cam_index);
    void signalCameraConnectionTimeout(QString);
    void signalWriteErrorCodeToPlc(QString);

    void frameReady(const QImage &img);
public slots:
    void CaptureReady();	// Jang 20220408
private:
    bool m_running = false;
    Pylon::CInstantCamera m_camera;
    QImage convertToQimage(const Pylon::CPylonImage &pylonImg);
};

#endif // CAPTURETHREADL_H
