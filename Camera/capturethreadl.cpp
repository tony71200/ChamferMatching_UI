#include "capturethreadl.h"
#include <atomic>
#include <QDir>
#include <QDateTime>

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

    frame_in_process = false;
}

CaptureThreadL::~CaptureThreadL(){
    // [OPTIMIZED] Stop thread gracefully using condition variable
    mutex.lock();
    stop = true;
    condition.wakeOne();
    mutex.unlock();
    wait(); // Đảm bảo thread đã dừng trước khi hủy
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

// Hàm mới: Lưu ảnh với format capture_<yyyymmdd>_<hhmmss>.jpg vào thư mục mặc định
void CaptureThreadL::CaptureAndSaveImage(const QImage& img) {
    QString saveDir = m_path_folder_save;
    QDir dir(saveDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
    QString filename = QString("capture_%1.jpg").arg(timestamp);
    QString fullPath = dir.filePath(filename);
    if (!img.save(fullPath, "JPG")) {
        qWarning() << "[CaptureThreadL] Failed to save image:" << fullPath;
    } else {
        qDebug() << "[CaptureThreadL] Image saved:" << fullPath;
    }
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
        if (n<=0) {
            is_cam_connected = false;
            err_msg = "Camera connection failed (unknown)";
            if(dm::log) {
                qDebug() << err_msg;
            }
            emit signalCameraLErrorMsg(QString("[Error c006] (camL)%1").arg(err_msg));
            emit signalSetEmptyImg(0);
            emit signalWriteErrorCodeToPlc("c006");
            return;
        }
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
        formatConverter.OutputPixelFormat = Pylon::PixelType_Mono8;
        Pylon::CPylonImage pylonImage;

        cv::Mat openCvImage;
        camera.StartGrabbing(Pylon::GrabStrategy_LatestImageOnly);
        Pylon::CGrabResultPtr ptrGrabResult;
        while (true) {
            mutex.lock();
            if (stop) {
                mutex.unlock();
                break;
            }
            mutex.unlock();
            camera.RetrieveResult(5000, ptrGrabResult, Pylon::TimeoutHandling_ThrowException);
            if(ptrGrabResult->GrabSucceeded()) {
                formatConverter.Convert(pylonImage,ptrGrabResult);
                QImage img = convertToQimage(pylonImage);
                if (!frame_in_process) {
                    frame_in_process = true;
                    emit frameReady(img);
                }
                m_lastFrame = img; // [OPTIMIZED] Lưu frame cuối cùng để UI có thể lưu khi cần
                QThread::msleep(delay);
            }
        }
        camera.StopGrabbing(); // [OPTIMIZED] Make sure grabbing stops when exiting the loop
        camera.Close();        // [OPTIMIZED] Make sure the camera is closed
    }
    catch (Pylon::AccessException access_ex) {
        is_cam_connected = false;
        err_msg = QString::fromStdString( std::string(access_ex.GetDescription()));
        emit signalCameraLErrorMsg(QString("[Error c002] (camL)%1").arg(err_msg));
        emit signalSetEmptyImg(0);
        emit signalWriteErrorCodeToPlc("c002");
    }
    catch (Pylon::BadAllocException alloc_ex) {
        is_cam_connected = false;
        err_msg = QString::fromStdString( std::string(alloc_ex.GetDescription()));
        emit signalCameraLErrorMsg(QString("[Error c003] (camL)%1").arg(err_msg));
        emit signalSetEmptyImg(0);
        emit signalWriteErrorCodeToPlc("c003");
    }
    catch(Pylon::GenericException gen_ex){
        is_cam_connected = false;
        err_msg = QString::fromStdString( std::string(gen_ex.GetDescription()));
        emit signalCameraLErrorMsg(QString("[Error c004] (camL)%1").arg(err_msg));
        emit signalSetEmptyImg(0);
        emit signalWriteErrorCodeToPlc("c004");
    }
    catch(int line){
        is_cam_connected = false;
        err_msg = QString("Camera connection failed, check line: %1").arg(line);
        emit signalCameraLErrorMsg(QString("[Error c005] (camL)%1").arg(err_msg));
        emit signalSetEmptyImg(0);
        emit signalWriteErrorCodeToPlc("c005");
    }
    catch(...){
        is_cam_connected = false;
        err_msg = "Camera connection failed (unknown)";
        if(dm::log) {
            qDebug() << err_msg;
        }
        emit signalCameraLErrorMsg(QString("[Error c006] (camL)%1").arg(err_msg));
        emit signalSetEmptyImg(0);
        emit signalWriteErrorCodeToPlc("c006");
    }
}

void CaptureThreadL::Stop() {
    mutex.lock();
    stop = true;
    condition.wakeOne();
    mutex.unlock();
    quit();
    wait();
}

void CaptureThreadL::Reconnect(Pylon::IPylonDevice *pDevice) {
    if (dm::log)
        qDebug() << "[DEBUG] camera reconnect";
    QThread::msleep(1000);
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

// Slot để UI trigger lưu hình
void CaptureThreadL::saveCurrentFrame() {
    if (!m_lastFrame.isNull()) {
        CaptureAndSaveImage(m_lastFrame);
    } else {
        qWarning() << "[CaptureThreadL] No frame available to save.";
    }
}

