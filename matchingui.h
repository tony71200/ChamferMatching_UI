#ifndef MATCHINGUI_H
#define MATCHINGUI_H

// C++ Standard Library
#include <chrono>
#include <cmath>

// Qt
#include <QMainWindow>
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
#include <QActionGroup>
#include <QMutex>
#include <QThread>

// OpenCV
#include <opencv2/opencv.hpp>
#include <opencv2/core.hpp>
#include <opencv2/highgui/highgui.hpp>

// Project-specific
#include "PLCComm/plcconnector.h"
#include "custompicturebox.h"
#include "Camera/capturethreadl.h"
#include "Chamfer/chamfermatch.h"
#include "Chamfer/matchingcontainer.h"
#include "globalparams.h"
#include "Log/timelog.h"
#include "Camera/Calibration.h"
#include "robotposetrans.h"
#include "custompicturepanel.h"
#include "Utils/initsetup.h"

namespace Ui {
class MatchingUI;
}

/**
 * @brief The main window for the Chamfer Matching application.
 *
 * This class manages the user interface, orchestrates the interactions between
 * the camera, PLC, matching algorithms, and handles user input.
 */
class MatchingUI : public QMainWindow
{
    Q_OBJECT

public:
    explicit MatchingUI(QWidget *parent = 0);
    ~MatchingUI();
protected:
    void resizeEvent(QResizeEvent *event) override;

private slots:
    // --- UI Action Slots ---
    void onModeChanged(QAction *action);
    void onMethodChanged(QAction *action);
    void on_btn_connect_clicked();
    void on_btn_connect_plc_clicked(bool checked);
    void on_btn_train_data_clicked();
    void on_btn_run_clicked();
    void on_btn_capture_clicked();
    void on_btn_loadImage_clicked();

    // --- Custom Widget Slots ---
    void onTemplatesChanged(const QMap<int, CustomPictureBox::TemplateData> &templates);
    void onRoisChanged(const QMap<int, CustomPictureBox::TemplateData> &rois);

    // --- Camera Slots ---
    void update_QImage(QImage img);

    // --- PLC Slots ---
    void UpdatePlcStatus(uint16_t st);
    void UpdatePLCState(bool state);
    void PrintPlcLogs(QString msg);
    void ActionAlign();
    void StopPlcThread();
    void FinishPlcCmd();


signals:
    // --- PLC Signals ---
    void signalPlcInit();
    void signalPlcSetPath(QString, QString);
    void signalPlcDisconnect(bool);
    void signalPlcLoadDefaultSetting(std::string, std::string);
    void signalPlcLoadSetting();
    void signalPlcWriteSetting();
    void signalPlcSetCCDTrig();
    void signalPlcSetWaitingPlcAck(bool);
    void signalPlcSetStatus(uint16_t);
    void signalPlcSetCurrentRecipeNum(uint16_t);
    void signalPlcWriteResult(int32_t, int32_t, int32_t, int32_t, PlcWriteMode);
    void signalPlcWriteCurrentRecipeNum();
    void signalPlcWriteCurrentErrorCodeNum(QString);
    void signalPlcGetStatus();

private:
    // --- Initialization Functions ---
    void setupUI();
    void setupActionsAndConnections();
    void setupPLC();
    void setupCamera();
    void setupMatching();
    void loadAppSettings();
    void updateUIFromSettings();

    // --- Core Logic Functions ---
    void saveRois(const QMap<int, CustomPictureBox::TemplateData> &rois);
    void loadRois(const QSize &imageSize);
    void saveTemplatesInfo();
    void loadTemplatesInfo();
    void setTrainMode(bool enabled);
    void setSettingMode(bool enabled);
    void drawResult(cv::Mat &image, const std::vector<cv::Point> &points, const QRect &roi);
    void SleepByQtimer(unsigned int timeout_ms);

    // --- UI Components ---
    Ui::MatchingUI *ui;
    CustomPicturePanel *mainPicturePanel;
    QActionGroup *modeActionGroup;
    QActionGroup *methodActionGroup;
    CustomPictureBox* getPictureBox() const { return mainPicturePanel ? mainPicturePanel->getPictureBox() : nullptr; }

    // --- Application Logic & State ---
    InitSetup *m_initSetup;
    CustomPictureBox::Mode current_mode;
    RobotPoseTrans *transformer;

    // --- PLC Communication ---
    PlcConnector *connector_plc_;
    QThread *thread_plc_;
    uint16_t plc_status_;
    bool isconnectPLC;

    // --- Camera & Image Data ---
    CaptureThreadL *camL;
    QPixmap lastpixmap;
    Calibration *calibration_;
    double mmPerPixel;
    QMutex *mutex_shared;

    // --- Chamfer Matching ---
    float focusx, focusy, a; // 'a' is a distance in mm
    MatchingContainer m_container1;
    MatchingContainer m_container2;

    // Template data
    QPixmap m_template_pixmap1, m_template_pixmap2;
    cv::Mat m_template_mat1, m_template_mat2;
    QRect template_roi_1, template_roi_2;
    QRect m_trained_template_roi1, m_trained_template_roi2;
    float m_trained_angle;

    // ROI data
    QRect m_roi_rect1, m_roi_rect2;
};

#endif // MATCHINGUI_H
