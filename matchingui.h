#ifndef MATCHINGUI_H
#define MATCHINGUI_H

#include <QMainWindow>
#include <opencv2/opencv.hpp>
#include <opencv2/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <QString>
#include <QDebug>
#include <QImage>
#include <QLabel>
#include <QRadioButton>
#include <QPushButton>
#include <QTextEdit>
#include <QSpinBox>
#include <QGroupBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QFileDialog>
#include <chrono>
#include <QActionGroup>

#include "PLCComm/plcconnector.h"
#include "custompicturebox.h"
#include "Camera/capturethreadl.h"
#include "Chamfer/chamfermatch.h"
#include "Chamfer/matchingcontainer.h"
#include "globalparams.h"
#include "Log/timelog.h"


namespace Ui {
class MatchingUI;
}

class MatchingUI : public QMainWindow
{
    Q_OBJECT

public:
    explicit MatchingUI(QWidget *parent = 0);
    ~MatchingUI();

private slots:
    void on_btn_load_img_clicked();

    void onModeChanged(QAction *action);
    void onMethodChanged(QAction *action);

    // Slots for signals from CustomPictureBox
    void onTemplatesChanged(const QMap<int, CustomPictureBox::TemplateData> &templates);
    void onRoisChanged(const QMap<int, CustomPictureBox::TemplateData> &rois);

    void on_btn_connect_clicked();
    void update_image_mat(cv::Mat img);
    void update_QImage(QImage img);

    void on_btn_train_data_clicked();

    void on_btn_run_clicked();

    void on_btn_connect_plc_clicked();

    void on_btn_send_clicked();

private:
    void setupConnections();
    void updateModeLabel(const QString& mode);
    void updateMethodLabel(const QString& method);
    void saveRois(const QMap<int, CustomPictureBox::TemplateData> &rois);
    void loadRois(const QSize &imageSize);
    void saveTemplatesInfo();
    void loadTemplatesInfo();
    void setTrain(const bool& train);
    void setSetting(const bool& setting);
    void setCapture(const bool& capture);
    void drawResult(cv::Mat &image, const std::vector<cv::Point> &points, const QRect &roi);
//    void processAndDisplayResults(const CoorData& result1, const QRect& roi1, const CoorData& result2, const QRect& roi2);
    void SleepByQtimer(unsigned int timeout_ms);
    bool isconnectPLC = false;

    Ui::MatchingUI *ui;
    CustomPictureBox *mainPictureBox;
    QActionGroup *modeActionGroup;
    QActionGroup *methodActionGroup;

    // ================PLC Communication===================================
    PlcConnector *connector_plc_;
    QThread *thread_plc_;
    uint16_t plc_status_;


    //=====================================================================

    // ================Camera Pylon===================================
    CaptureThreadL *camL;
    QPixmap lastpixmap;
    QPixmap template1, template2;
    QRect template_roi_1, template_roi_2;
    // Store initial trained template positions
    QRect m_trained_template_roi1, m_trained_template_roi2;
    float m_trained_angle;
    QPixmap ROIpx1, ROIpx2;
    QRect roi_rect_1, roi_rect_2;
    QMutex *mutex_shared;
    // ================Chamfer Matching===============================
    float focusx, focusy, a;
    MatchingContainer m_container1;
    MatchingContainer m_container2;
    // Store template data as members
    QPixmap m_template_pixmap1, m_template_pixmap2;
    cv::Mat m_template_mat1, m_template_mat2;


    // Store ROI data for tesing
    QRect m_roi_rect1, m_roi_rect2;
signals:    // signal for PLC thread. Jimmy.
    void signalPlcInit();
    void signalPlcSetPath(QString, QString);
    void signalPlcDisconnect(bool);
    void signalPlcLoadDefaultSetting(std::string, std::string);
    void signalPlcLoadSetting();
    void signalPlcWriteSetting();
    void signalPlcSetCCDTrig();
    void signalPlcSetWaitPlcAckSuccess(bool);
    void signalPlcSetWaitingPlcAck(bool);
    void signalPlcSetStatus(uint16_t);
    void signalPlcSetCurrentRecipeNum(uint16_t);
    void signalPlcWriteResult(int32_t, int32_t, int32_t, int32_t, PlcWriteMode); //result=> 0: continue; 1: finish; 2: NG
    void signalPlcWriteCurrentRecipeNum();
    void signalPlcWriteCurrentErrorCodeNum(QString);
    void signalPlcGetStatus();

private slots:
    void UpdatePlcStatus(uint16_t st) { plc_status_ = st; }
    void UpdatePLCState(bool state);
    void PrintPlcLogs(QString msg) {  }
    void FinishPlcCmd(){
        emit signalPlcSetCCDTrig();
        emit signalPlcSetStatus(_PLC_IDLE_);
    }
    void StopPlcThread(){
        thread_plc_->quit();
        thread_plc_->wait();
    }
    void ActionAlign();
};

#endif // MATCHINGUI_H
