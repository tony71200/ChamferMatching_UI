#include "capturethreadl.h"

static const uint32_t c_countOfImagesToGrab = 10;

CaptureThreadL::CaptureThreadL(QObject *parent) : QThread(parent)
{
    cam_model = _CAMERA_SDK_::BASLER_PYLON;
    stop = true;
    is_cam_connected = false;
    imageprocSettingDialog = nullptr;

    device_idx_ = 1;
    exposure_time_ = 150000;
    gain_ = 0;

    std::atomic<bool> frame_in_process = false;
}

CaptureThreadL::~CaptureThreadL(){
    mutex.lock();
    stop = true;
    condition.wakeOne();
    mutex.unlock();
}

void CaptureThreadL::Play() {
    if (!isRunning()) {
        if (isStopped()) {
            stop = false;
        }
        start(LowestPriority);
    }
}

// Jang 20220408
void CaptureThreadL::CaptureReady(){
    is_capture_ready_ = true;
}

QImage CaptureThreadL::convertToQimage(const Pylon::CPylonImage &pylonImg)
{
    return QImage(static_cast<const uchar *>(pylonImg.GetBuffer()),
                  pylonImg.GetWidth(),
                  pylonImg.GetHeight(),
                  QImage::Format_Grayscale8).copy();
}

void CaptureThreadL::run() {
    frame_rate = 4000;
    int delay = (1000 / frame_rate);
    bool isDoneSendOnce = false;

    QString err_msg;
    //================================ Pylon =====================================
    try {
        //Pylon::PylonAutoInitTerm auto_init_term;

        Pylon::DeviceInfoList_t devices;
        Pylon::CTlFactory &tlFactory = Pylon::CTlFactory::GetInstance();
        int n = tlFactory.EnumerateDevices(devices);
        Pylon::IPylonDevice* pDev = tlFactory.CreateDevice(devices[0]);
        Pylon::CInstantCamera camera(pDev);
        //================================
        camera.Open();
        //================================

        //==============Setup Default Parameters
        // Tony 250626
        if (using_default_parameter) {
            GenApi::INodeMap& nodeMap = camera.GetNodeMap();
            if ((GenApi::IsReadable(nodeMap.GetNode("AcquisitionFrameRate"))) &&
                    (GenApi::IsWritable(nodeMap.GetNode("AcquisitionFrameRate")))) {
                // Set AcquisitionFrameRate = 10;
                GenApi::CBooleanPtr(nodeMap.GetNode("AcquisitionFrameRateEnable"))->SetValue(true);
                GenApi::CFloatPtr(nodeMap.GetNode("AcquisitionFrameRate"))->SetValue(10.0);
            }

            if ((GenApi::IsReadable(nodeMap.GetNode("ExposureTime"))) && (GenApi::IsWritable(nodeMap.GetNode("ExposureTime")))) {
                // Set ExposureTime = 90000
                GenApi::CFloatPtr(nodeMap.GetNode("ExposureTime"))->SetValue(90000);
            }

            if ((GenApi::IsReadable(nodeMap.GetNode("ReverseX"))) && (GenApi::IsWritable(nodeMap.GetNode("ReverseX")))) {
                GenApi::CBooleanPtr(nodeMap.GetNode("ReverseX"))->SetValue(false);
            }

            if ((GenApi::IsReadable(nodeMap.GetNode("ReverseY"))) && (GenApi::IsWritable(nodeMap.GetNode("ReverseY")))) {
                GenApi::CBooleanPtr(nodeMap.GetNode("ReverseY"))->SetValue(false);
            }
        }
        //=======================================

        camera.MaxNumBuffer = 1;
        Pylon::CImageFormatConverter formatConverter;
//        formatConverter.OutputPixelFormat = Pylon::PixelType_BGR8packed;
        formatConverter.OutputPixelFormat = Pylon::PixelType_Mono8;
        Pylon::CPylonImage pylonImage;

        cv::Mat openCvImage;
        camera.StartGrabbing(Pylon::GrabStrategy_LatestImageOnly);
        Pylon::CGrabResultPtr ptrGrabResult;

        while (!stop) {
            camera.RetrieveResult(5000, ptrGrabResult, Pylon::TimeoutHandling_ThrowException);
            if(ptrGrabResult->GrabSucceeded()) {
                formatConverter.Convert(pylonImage,ptrGrabResult);
                QImage img = convertToQimage(pylonImage);
                //openCvImage = cv::Mat(ptrGrabResult->GetHeight(),ptrGrabResult->GetWidth(),CV_8UC3,(uint8_t *)pylonImage.GetBuffer());

                //emit ProcessedImgL(openCvImage);
                {
                    QMutexLocker locker(&mutex_shared);
                    last_image = img;
                }
                if (!frame_in_process) {
                    frame_in_process = true;
                    emit frameReady(img);
                }
                
                QThread::msleep(delay);
            }
        }
    }
    catch (Pylon::AccessException access_ex) {
        is_cam_connected = false;
        err_msg = QString::fromStdString( std::string(access_ex.GetDescription()));
        emit signalCameraLErrorMsg(QString("[Error c002] (camL)%1").arg(err_msg));
        emit signalSetEmptyImg(0); // Set AlignSys::frameL empty
        emit signalWriteErrorCodeToPlc("c002");
    }
    catch (Pylon::BadAllocException alloc_ex) {
        is_cam_connected = false;
        err_msg = QString::fromStdString( std::string(alloc_ex.GetDescription()));
        emit signalCameraLErrorMsg(QString("[Error c003] (camL)%1").arg(err_msg));
        emit signalSetEmptyImg(0); // Set AlignSys::frameL empty
        emit signalWriteErrorCodeToPlc("c003");
    }
    catch(Pylon::GenericException gen_ex){
        is_cam_connected = false;
        err_msg = QString::fromStdString( std::string(gen_ex.GetDescription()));
        emit signalCameraLErrorMsg(QString("[Error c004] (camL)%1").arg(err_msg));
        emit signalSetEmptyImg(0); // Set AlignSys::frameL empty
        emit signalWriteErrorCodeToPlc("c004");
    }
    catch(int line){
        is_cam_connected = false;
        err_msg = QString("Camera connection failed, check line: %1").arg(line);
        emit signalCameraLErrorMsg(QString("[Error c005] (camL)%1").arg(err_msg));
        emit signalSetEmptyImg(0); // Set AlignSys::frameL empty
        emit signalWriteErrorCodeToPlc("c005");
    }
    catch(...){
        is_cam_connected = false;
        err_msg = "Camera connection failed (unknown)";
        if(dm::log) {
            qDebug() << err_msg;
        }
        emit signalCameraLErrorMsg(QString("[Error c006] (camL)%1").arg(err_msg));
        emit signalSetEmptyImg(0); // Set AlignSys::frameL empty
        emit signalWriteErrorCodeToPlc("c006");
    }
}

void CaptureThreadL::Stop() {
    stop = true;
    quit();
    wait();
}

void CaptureThreadL::msleep(int ms){
    struct timespec ts = { ms/1000, (ms%1000)*1000*1000 };
    // nanosleep(&ts, NULL);
}

void CaptureThreadL::Reconnect(Pylon::IPylonDevice *pDevice) {
    if (dm::log)
        qDebug() << "[DEBUG] camera reconnect";
    usleep(1000000);
}

bool CaptureThreadL::isStopped() const {
    return this->stop;
}

bool CaptureThreadL::IsConnected() {
    return this->is_cam_connected;
}

void CaptureThreadL::setVideoFormat(int64_t offset_x, int64_t offset_y, int64_t w, int64_t h, bool flip)   // add by garly
{
    videoFormat_w = w;
    videoFormat_h = h;
    videoFormat_offset_x = offset_x;
    videoFormat_offset_y = offset_y;
    videoFormat_flip = flip;
}

void CaptureThreadL::SetCameraModel(_CAMERA_SDK_ cam_model){
    this->cam_model = cam_model;
}

#ifdef SPINNAKER_CAM_API
void CaptureThreadL::SetSpinnakerPtr(SpinnakerCamera *spin_cam){
    this->cam_model = _CAMERA_SDK_::FLIR_SPINNAKER;
    this->spin_cam = spin_cam;
}
#endif

void CaptureThreadL::setSourceMode(int src)
{
    // currentMode = src;
}

// Jang 20220106
void CaptureThreadL::UseOfflineImg(bool use_offline){
    this->use_offline_img = use_offline;
}

