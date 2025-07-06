#ifndef ALIGNSYS_H
#define ALIGNSYS_H

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <algorithm>
#include <cmath>
#include <ctime>
#include <cstring>
#include <string>
#include <unistd.h>
#include <memory>

#include <QString>
#include <QRegExp>
#include <QDebug>
#include <QImage>
#include <QTimer>
#include <QTime>
#include <QDateTime>
#include <QDate>
#include <QThread>
#include <QTransform>

#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <QDirIterator>
#include <QMessageBox>

#include <QScrollBar>
#include <QMainWindow>
#include <QImageReader>
#include <QCloseEvent>
#include <QMouseEvent>  //garly_20211217
#include <QKeyEvent>
#include <QNetworkInterface>

#include "globalparms.h"
#include "Log/timelog.h"

#include "capturethreadl.h"
#include "capturethreadr.h"
#include "processthread.h"
#include "testprocess.h"
#include "alignprocess.h"
#include "plcconnector.h"
#include "markersettingdialog.h"
#include "debugdialog.h"
#include "recipechange.h"
#include "directionsetting.h"

#include "msgboxdialog.h"
#include "shapesettingdialog.h"
#include "imageprocsettingdialog.h"
#include "camerasettingdialog.h"
#include "BrightnessAdjustment/optconnector.h"  // Jimmy. 20220119
#include "systemsettingdialog.h"
#include "croproidialog.h"      // Add the crop ROI dialog. Jimmy 20211223
#include "changecustomerdialog.h"
#include "changeauthoritydialog.h"
#include "configlightsourceipdialog.h"
#include "barcodedialog.h"      // Add for Barcode detection. Jimmy. 20220301.
#include "othersettingdialog.h" // Jang 20220411
#include "diskspacemanager.h"   // Jimmy 20220804

#include "logindialog.h"
#include "warningmsgdialog.h"

#ifdef SPINNAKER_CAM_API
#include "Spinnaker/spinnakercamera.h"
#endif
#ifndef MC_PROTOCOL_PLC
#include "communication.h"
#endif
#include "ShapeAlignment/shapealignment.h"
#include "FileIO/fileiomanager.h"
#include "FileIO/fileio.h"

/** Definition **/
#define PPR 1600

#define VIDEO_FORMAT_WIDTH 1280     // add by garly, for adjust video format
#define VIDEO_FORMAT_HEIGHT 800     // add by garly, for adjust video format

#define DBLOG(x)    do { if (dm::log) qDebug() << x ; } while (0)

/** Namespace **/
using namespace std;
namespace fm = file_io_manager;
namespace fio = file_io;
namespace Ui {
class AlignSys;
}

/** Main Class - AlignSys **/
class AlignSys : public QMainWindow
{
    Q_OBJECT

    const double kPi = 3.14159265358979323846;
public:
    explicit AlignSys(QWidget *parent = 0);
    ~AlignSys();

    /// Public Functions
    void InitializeUI();
    void SetMainMenuBtnEnable(bool enabled, QPushButton* selected_button = nullptr);
    void SetBtnStyle(QString styleSheet = "" ,QPushButton* selected_button = nullptr);
    void ShowCurrentSystemStatus();
#ifdef MC_PROTOCOL_PLC	// add by garly
    // unit: pulse
    void SetPLCValues(double*, double, double, double);
#else
    void SetPLCValues(double*, int, int, int);
#endif
    void OpenImage(QImage *);
    bool LoadImage(QImage *, const QString &);
    void OpenCamera();
    void StopCamera();
    void CloseCamera();
#ifdef SPINNAKER_CAM_API
    bool SaveCameraParams(CamParams l_cam_params, CamParams r_cam_params);
    bool LoadCameraParams(CamParams& l_cam_params, CamParams& r_cam_params);
#endif
    cv::Mat DrawResult(const cv::Mat &, int, uint32_t c);
    void RunOneRST();
    bool RunCalRST();
#ifdef MC_PROTOCOL_PLC	// add by garly
    QString ControlPLC(QString, double *);
#else
    QString ControlPLC(Communication *, QString, double *);
#endif
    QString ControlPLCPulse(QString mode, double* vals);
    QString ControlPLCMMDeg(QString mode, double* vals);
    void DoAlign();
    void DoPreDetect(); // Jimmy 20221006.

    // RST Template Matching
    void RST_Train(bool is_re_train);
    void RST_Test(int);
    void RST_Show(int c);

    void AutoTurnOnCamera();    // add by garly
    void SystemLog(const QString& str);
    void SystemLogfWithCodeLoc(const QString& msg, const QString& file, const QString& func);
    void SaveCalibrationImage(const cv::Mat& img, int index, int lr);     // add by garly
    void closeEvent(QCloseEvent *event);

    void DisconnectCameras();
    bool eventFilter(QObject *obj, QEvent *event);  //garly_20211217
    void mouseDoubleClickEvent(QMouseEvent *e) override;
    void WriteErrorCodeToPlc(QString error_code);
private:
    /// Private Variables
    // UI
    Ui::AlignSys *ui;
    // Dialog classes
    MarkerSettingDialog *markerSettingDialog = nullptr;
    DebugDialog *debugDialog = nullptr;
    RecipeChange *recipeChangeDialog = nullptr;
    MsgBoxDialog *msgDialog = nullptr;
#ifdef ORIGINAL_SHAPE_ALIGN
    ShapeSettingDialog *shapeSettingDialog;
#endif
    CameraSettingDialog *cameraSettingDialog = nullptr;
    SystemSettingDialog *systemSettingDialog = nullptr;
    ImageProcSettingDialog * imageprocSettingDialog = nullptr;
    ParameterSettingDialog* paramSetting = nullptr ;
    CropRoiDialog* cropRoiDialog = nullptr;
    changeCustomerDialog* customerDialog = nullptr;
    changeAuthorityDialog* authorityDialog = nullptr;
    OtherSettingDialog* otherSettingDialog = nullptr; // Jang 20220411
#ifdef BARCODE_READER
    BarcodeDialog* barcodeDialog;
#endif
    LoginDialog* loginDialog;

    // System and Camera Direction Setting. Jimmy. 20220503.
    DirectionSetting &direct_setting_ = DirectionSetting::getInstance();

    int* authority_ = nullptr;

    // Image Process, RST Template Matching
    Match match;        // Class match: for RST template matching
    Match match_R;      // RST class for a right camera; JANG 20211007

    cv::Mat RST_template;     // RST template
    cv::Mat RST_template_R;  // From right

    // log writer
    logwriter::LogWriter lw;
    logwriter::LogWriter lw_mem;
    logwriter::LogWriter lw_result;  //ResultLog_

    std::vector<CoorData> rslt_rstL; // RST final results (left)
    std::vector<CoorData> rslt_rstR; // RST final results (right)
    float t_testL;
    float t_testR;

    // Camera Thread
    std::string cam_setting_path;
    _CAMERA_SDK_ cam_model;
    CaptureThreadL *camL = nullptr;
    CaptureThreadR *camR = nullptr;

    std::string parameter_setting_path;

    // Light brightness controller
    bool is_running_bright_adjust;
    OptConnector *connector_opt_;   // Light source connection. Jimmy. 20220119
    QThread *thread_opt_;

    std::string opt_recipe_path_;
    uint16_t opt_status_;
    static const int opt_channel_idx_L_ = 2;
    static const int opt_channel_idx_R_ = 1;
    int opt_brightness_val_L_;
    int opt_brightness_val_R_;
#ifdef ENABLE_AUTO_ADJUST_BRIGHTNESS
    AdjustBrightThread* adjust_bright;
#endif
    // External Light
    uint8_t lightSourceEnableTiming = _ALWAYS_OPEN_;
    bool optDevice_ackSuccess = false;
    int lightCurrentStatus = _LightTurnOn_;

    cv::Rect roi_rect_l_; // ROI of current collected template
    cv::Rect roi_rect_r_; // ROI of current collected template

    SystemSetting sys_set;
    SystemSetting sys_set_default_yt;
    SystemSetting sys_set_default_uvw;
    SystemSetting sys_set_backup;   // Jang 20220510 // to restore previous settings
#ifdef ENABLE_AUTO_ADJUST_BRIGHTNESS
    CameraSettingParams* param_L;
    CameraSettingParams* param_R;
#endif
    cv::Mat frameL;
    cv::Mat frameR;
    cv::Mat align_img_l_;
    cv::Mat align_img_r_;

    // Calibration Thread
    QThread thread_calib;

#ifdef SPINNAKER_CAM_API
    SpinnakerCamera* spin_cam = NULL;
#endif
    QMutex *mutex_shared_l_;
    QMutex *mutex_shared_r_;
    QMutex mutex_reconnect_;
    // PLC Control
#ifdef MC_PROTOCOL_PLC  // add by garly
    // PLC communication. Jimmy. 20220119
    PlcConnector *connector_plc_;
    QThread *thread_plc_;
    uint16_t plc_status_;
#else
    Communication* com;
#endif

    // Alignment Process
    Align align;
    int count_calpos;
    Coor pc;                    // Platform center point (cam 1 & main center)
    Coor pc2;                   // Platform center point (cam 2)
    Coor pc2_recomputed;        // recomputed according to pc (left cam)
    std::vector<Coor> cal_pos;  // Calibration position: 1(l, r), 2(l, r), ..., 11(l, r)

    QString ori_pos_info1;      // Original position in alignment process
    QString ori_pos_info2;
    QString fin_pos_info1;
    QString fin_pos_info2;
    QString calib_rslt1;        // Calibration result 1
    QString calib_rslt2;        // Calibration result 2

    // Set Reference
    QString ref_info1;
    QString ref_info2;

    // Recipe Manager and Paths
    QString root_path_ = "./";   // std::filesystem::current_path
    QString system_config_path_;
    QString currentRecipeName;
    QString currentTemplateName;
    QString recipe_dir;         // recipe_0006.txt -> recipe_dir = 0006
    QStringList recipe_num_list_;   // Record all recipe num list, Jimmy 20220617.

    int func_mode = _MODE_NONE_;      // Mode: functional mode; _MODE_NONE_ = 0, _MODE_MARKER_DETECT_, _MODE_CALIBRATION_, _MODE_SET_REF_, _MODE_ALIGNMENT_
    int currentMode = _MODE_IDLE_;    // _MODE_IDLE_ , _MODE_MARKER_,  _MODE_CHAMFER_, _MODE_SHAPE_ROI_, _MODE_SHAPE_ONE_WAY_

    int system_type =  _SYSTEM_FINE_ALIGN_ ; // _SYSTEM_FINE_ALIGN_
    int machine_type = _MACHINE_UVW_;    // The machine type. (_MACHINE_YT_ cannot use _MODE_CALIB_XYT_); _MACHINE_UVW_, _MACHINE_YT_
    int calibration_mode = _MODE_CALIB_XYT_; // _MODE_CALIB_XYT_, _MODE_CALIB_YT_
    int system_unit = _UNIT_MM_DEG_;    // _UNIT_MM_DEG

    int sleep_us_plc = 50 * 1000; // 30 ms

    /// Status Flags
    // Template data // Jang 20220331 // to skip loading when the matching setting button is clicked.
    bool is_template_loaded_ = false;
    // PLC
    bool isPLCConnected = false;
    // Opt Light Source
    bool isOptLightConnected = false;
    bool isOptLightStatusSwitch = false;
    // Camera
    bool switch_cam;        // is camera turn on or not
    bool isCamOpenSuccess;
    bool isCamStop;         // camera is stop or not
    bool isFirstSnap_l;
    bool isFirstSnap_r;
    bool showGuideline;
    // Template
    bool isLoadtemp;        // template loaded or not
    bool isCreatetemp;      // go into create template mode
    bool isMaskEnabled;     // Jimmy. 20220330. Mask
    bool isMaskRoi;         // Jimmy. 20220330. Mask
    bool isLoadtemp_r;      // template loaded or not
    bool isCreatetemp_r;    // go into create template mode
    // Matching
    bool isNoResult;        // RST detect no result
    bool isMultiResult;     // RST detect multiple results. Jimmy. 20211215
    bool isSetRefPt;
    bool refPtExist;
    bool updateRefInfoL;
    bool updateRefInfoR;
    bool detectResultSuccess_c1 = false;    // Matching succeeded or not on the camera 1
    bool detectResultSuccess_c2 = false;    // Matching succeeded or not on the camera 2
    bool markerDetectSuccess = false;       // if detectResultSuccess_c1 and detectResultSuccess_c2 are TRUE, it is TURE.
    bool show_match_result_{false};     // To show matching result on the image views. Jang 20221020
    // Calibration
    bool isRunCalib;        // is calibration run
    bool isDoneCalib;       // is the calib. done?
    bool isCalibReadyForAlign;  // If calibration data ok for alignment, set true;
    // Alignment
    bool isRunAlign;        // is alignment run
    bool isDoneAlign;       // is the alignment done
    bool doAlignRepeat;     // if over than TOL, running 2nd alignment
    bool isAlignmentOK;     // the alignment result is ok or not
    bool isAlignmentCheck;  // the alignment check
    bool is_align_valid_method; // _ALIGN_VALID_TWO_REF_PTS_ ,  _ALIGN_VALID_REF_CENTER_
    // Pre-Detection 20221011 Jimmy
    bool isRunPreDetect{false};
    // Sub-window/Dialog
    bool subMenuActive;     // other sub-window(dialog) is opened, or not.
    bool markerSettingDialogIsShow = false;
#ifdef ORIGINAL_SHAPE_ALIGN
    bool shapeSettingDialogIsShow = false;
#endif
    bool cameraSettingDialogIsShow = false;
    bool msgDialogIsShow = false;
    bool imageprocSettingDialogIsShown = false;
    bool loginDialogIsShown = false;
    bool parameterSettingDialogIsShown = false;
    bool otherSettingDialogIsShown = false; // Jang 20220411
    // for Crop ROI
    bool cropRoiDialogIsShown = false;
    bool isCropping = false;
    bool isUpdateImgL = false;
    bool isUpdateImgR = false;
    // for user changes
    bool customerDialogIsShown = false;
    bool authorityDialogIsShown = false;
    // for Barcode
    bool isBarcodeDialogShown = false;
    // for camera connection problem. Jimmy, 20220613
    bool is_cam_connection_failed = false;
    // for resetting CCD system(include recipe change). Jimmy 20220627
    bool is_reset_CCDsys_ = false;
    // status light signal switch. Jimmy 20220627
    bool is_stat_sig_switch_ = false;

    // Function Tools
    QImage _Mat2QImg(const cv::Mat &);
    QString _Pulse2MM(double pul);
    double _Pix2MM(double pix);
    QString _Setw(const QString& str, unsigned int w);
    void DeleteAllExpiredLogs(const string &logs_path, int duration_days);   // This function to delete expired Logs
    // Check the free space of the system
    bool CheckFreeSpace(int target_free_space_mb, double& available_space_mb); // Jang 20220427 // Unit is megabyte
    int FindOldestLogDate(const std::string &logs_path); // Jang 20220427
    void RemoveTemplates(const QString& dir_path, const int maintain_num);  // Jang 20220504
    void DeleteLogWithCondition(int target_space_gb, int min_maintain_days_img, int min_maintain_days_txt);
    void TimerAutoDeleteLog(); // Jang 20220711

    // Check System-wise data parameters. Jimmy 20220715.
    void CheckSystemConfigDirAndSystemwisePara();

    // Others
    int x1_std;     // Detect result x of cam 1
    int y1_std;     // Detect result y of cam 1
    int x2_std;     // Detect result x of cam 2
    int y2_std;     // Detect result y of cam 2
    int x11_std;    // Ref. point x of cam 1
    int y11_std;    // Ref. point y of cam 1
    int x22_std;    // Ref. point x of cam 2
    int y22_std;    // Ref. point y of cam 2

    // Buffer for draw results
    int buf_1[1000];  // the points to draw the RST result for cam 1
    int buf_size_1 = 0;
    int buf_2[1000];
    int buf_size_2 = 0;

    // Alignment Test
    int alignTestTime = 0;
    int alignRepeatTime = 0;
    // Calib/Align Test
    bool test_detection_failed_ = false;

    int match_mode_single_or_multi = 0;
    int left_template_collected = 0;
    int right_template_collected = 0;
    int is_match_available = 0;
    // Jang 20220307
    bool is_new_collecting_left = true;
    bool is_new_collecting_right = true;

    // For Crop ROI. Jimmy. 20211223
    int crop_cam_idx_;

    // Template files
    std::string TempName;
    std::string Temppath;

#ifdef ENABLE_SHAPE_ALIGNMENT
    /// JANG Modification
    ShapeAlignment shape_align_0;
    ShapeAlignment shape_align_1;
#endif

    int GetCurrentMem();
    int ParseLine(char* line);
    std::string SaveMemInfo(); // Jang 20220509

    struct timespec time_start, time_end;
    float func_timeout_ = 10;   // 10 seconds // Jang 20220712
#ifdef SYNC_TIME_FROM_PLC
    bool syncTimeMode = true;   //true: auto, false: manual
    uint8_t autoSyncTimeUnit = 0;
#endif
    TimeLog time_log;
    QTimer* idle_timer_;
    QTimer* delete_log_timer_ = nullptr;
    QTimer *timer_updateCurrentTime_;//garly_20220224, for update current system time
    void SleepByQtimer(unsigned int timeout_ms);

    // Jang 20220711
    // Set conditions for deleting log files
    int target_space_gb_ = 36;
    int min_maintain_days_img_ = 4;
    int min_maintain_days_txt_ = 30;
    // Jang 20220712
    int check_space_time_ = 0;
    int check_space_interval_ = 3600; // unit: second // 1 hour
    bool check_space_after_align_ = true;
    // Deleting thread. Jimmy 20220804
    QPointer<QThread> th_disk_spc_manager_;
    QPointer<DiskSpaceManager> disk_spc_manager_;

// Jang 20220411
signals:    // Camera
    void signalCaptureReadyLeft();
    void signalCaptureReadyRight();

signals:    // signals for Calib./Align
    void sig_calib(cv::Mat temp, cv::Mat img1, cv::Mat img2);
    void sig_finishCal();
    void sig_readyCal();
    void sig_beginAlign();
    void sig_finishAlign(uint8_t);

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
    void signalPlcWriteResult(int32_t, int32_t, int32_t, int32_t, PlcWriteMode);
    void signalPlcWriteCurrentRecipeNum();
    void signalPlcWriteCurrentErrorCodeNum(QString);
    void signalPlcGetStatus();
#ifdef SYNC_TIME_FROM_PLC
    void signalPlcSetAdjustTimeMode(bool, int);
#endif
#ifdef BARCODE_READER   //garly_20220308
    void signalPlcWriteBarcodeResult(QString, int);
#endif

private slots:
    void UpdatePlcStatus(uint16_t st) { plc_status_ = st; }
    void PrintPlcLogs(QString msg) { this->log(msg); }
    void FinishPlcCmd(){
        emit signalPlcSetCCDTrig();
        emit signalPlcSetStatus(_PLC_IDLE_);
    }
    void StopPlcThread(){
        thread_plc_->quit();
        thread_plc_->wait();
    }

signals:    // signal for OPT(light source) thread. Jimmy.
    void signalOptInit(const QString&, int);
    void signalOptDisconnect(bool);
    void signalOptSetChannelState(char, char);
    void signalOptSetWaitPlcAckSuccess(bool);
    void signalSetChannelBrightness(char, char);
    void signalOptSetWaitingPlcAck(bool);
    void signalOptSetStatus(uint16_t);
    void signalOptGetStatus();
    void signalSetNetworkParameter(QString, QString, QString);
private slots:
    void setCamLBrightness(int value);
    void setCamRBrightness(int value);
    bool SaveOptBrightnessInfo(const string &recipe_path, bool isCamR);
    bool LoadOptBrightnessInfo(const string &recipe_path, bool isCamR);
    void UpdateOptStatus(uint16_t st) { opt_status_ = st; }
    void PrintOptLogs(QString msg) { this->log(msg); }
    void StopOptThread(){
        thread_opt_->quit();
        thread_opt_->wait();
    }
    void updateOptConfigIpResult(bool result) {
        if (result)
            log("[Action] 光源IP位址變更成功！");
        else
            log("[Action] 光源IP位址變更失敗！");
    }

    // For deleting function. Jimmy 20220804.
signals:
    void signalDeleteLogWithCondition(int, int, int);
private slots:
    void PrintDiskSpcMngLogs(QString msg) { this->log(msg); }
    void StopDiskSpcMngThread(){
        th_disk_spc_manager_->quit();
        th_disk_spc_manager_->wait();
    }

// Barcode Reader
signals:
    void signalBarcodeImage(QImage);
    void signalBarcodeDetect();
private slots:
#ifdef BARCODE_READER               // garly_20220308
    void ActionBarcodeReader();     // Jimmy. 20220302.
    void ReceiveBarcodeResult(const QString& info, int st);
    void OpenBarcodeDialog();       // Jimmy. 20220302.
#endif

/// Others slots and signals
signals:
    void signalMarkDetectDialogHint(QString);
protected slots:
    void ProcessRetrainTemplate(QString str);
private slots:
    void ChangeAuthority(int auth);
    void closeChangeAuthorityDialog();
    void RestartMemCheck();
    // Function
    void UpdateFrameL(cv::Mat img);
    void UpdateFrameR(cv::Mat img);
    void HandleCal();
    void DoCalib();     // Handle the Calibration process results
    void ResultStatus(const QString& str);

    // Logs
    void log(const QString &);
    void ErrorLog(const QString &);

    void ActionCalib();	//add by garly
    void ActionAlign();	//add by garly
    void ActionPreDetect(); // Jimmy 20221006.
    void ActionSetRef(int);
    void ActionLoadTempFromFile();
    void ActionCreatTemp();
    void ActionMaskRoi() { isMaskRoi = true; }
    void ActionOpenCamera();
    void ActionMarkerDetect();    // add by garly
    void ActionMarkerDetect(int cam);   // Jang 20211206
    void ActionRefreshUI();
    void ActionClearLogMsg();

    void ClearReferencePts();

    void UpdateSettingDialogFlag();
    void UpdateMarkerSetting();
    void LoadParameters(QString recipeName);
    void WriteLastTemplate(std::string str);
    void ClearTemplates();
    bool SaveSystemSetting();
    bool SaveSystemSetting(const SystemSetting &sys_set, const string &file_path);
    bool SaveDefaultParamsINI(const std::string& path);
    bool LoadSystemSetting();
    bool LoadSystemSetting(SystemSetting& sys_set, const std::string& file_path);
    bool LoadSystemSettingINI(SystemSetting& sys_set, const std::string& file_path, const std::string& section_name);
    void SetSystemSetting(SystemSetting sys_set);
    void SetDefaultSystemSetting();
    void LoadDefaultSystemSetting(int machine);
    void OpenParameterSettingDialog();
    void CloseParameterSettingDialog();
    // Jang 20220411
    void OpenOtherSettingDialog();
    void CloseOtherSettingDialog();
    void RestoreSystemSetting();        // Jang 20220510
    void RestartCamera();               // Jang 20220511
    void UpdateCamIndex(bool is_0l1r);  // Jang 20220511
    // For cropping image
    void OpenCropRoiDialog(int cam_idx);
    void CloseCropRoiDialog(CropInfo &crop_info);
    void CancelCropRoi();               // Jang 20220511
    void RunCropRoiL(cv::Mat full_img_l);
    void RunCropRoiR(cv::Mat full_img_r);

    void on_btn_Detect_clicked();
    void on_btn_markerSetting_clicked();
    void on_btn_changeRecipeNum_clicked();
    QString GetRecipeFullName(QString num);
    void ChangeRecipe(QString recipe_name, int src);
    void on_btn_exit_clicked();

    void UpdateCurrentTemplateName(QString str);
    void ProcessFinishCreatTemplate(QString str);
    void UpdateRSTtemplate(cv::Mat template_);
    void UpdateTemplates(cv::Mat template_l, cv::Mat template_r);// Jang 20220504
    void UpdateROIRectInfo(cv::Rect rect);
    void LoadROIRectInfo();    // Jang 20220629
    void UpdateMaskEnabled(bool enabled) { isMaskEnabled = enabled; }// Jimmy. 20220330. Mask
    void ProcessFinishMaskROI();                    // Jimmy. 20220330. Mask
    void UpdateTemplateMask(cv::Mat template_mask); // Jang 20220401
    void UpdateSrcImgMask(cv::Mat src_img_mask);    // Jang 20220401
    // System and Camera Direction Setting. Jimmy. 20220503.
    void UpdateSysCamDirectInfo();

    void OpenDebugDialog();
    void UpdateCalibrationParms(double dis, double ang);
    void UpdateAlignmentParms(double tol, int align_repeat, double align_dis_tol);
    void EnableEnginneringMode(bool status);
    void on_btn_saveCurrentImage_clicked();

#ifdef ORIGINAL_SHAPE_ALIGN
    void CloseShapeSettingDialog();
    void on_btn_shapeSetting_clicked();    
#endif
    void CloseCameraSettingDialog();
    void SetShapeMode();

    void WriteSystemConfig();
    bool LoadSystemConfig();
    bool LoadLastRecipe();
    bool LoadSelectRecipe();    // Load recipes and select one of them. But if no recipes, create one.
    QStringList ListAllRecipeNum(); // For list all recipe number, Jimmy 20220617
    void WriteResult(const QString &type, const QString &str);

    // Save/Load matching mode data
    bool SaveMatchModeStatus();
    bool LoadMatchModeStatus();

    void on_btn_cameraSetting_clicked();

    void GetBrightnessResult(); // for auto adjust brightness
    void WarningMsg(const QString& title, const QString& str, uint8_t button_type);

    void HoldProcessMsg();// Jang 20220415
    void CloseProcessMsg();// Jang 20220415
    void on_btn_Test_clicked();
    void on_btn_systemSetting_clicked();
    void AutoAdjustLightBrightness(); // for auto adjust brightness
    // is Cam open successfully
    void setCamOpenFailed(QString);
    void AutoRestartSystem(const QString &);
    void removeCamImage(int cam_index);
    void CloseImageProcSettingDialog();
    void closeSettingDialog(uint8_t light_timing);
    void on_pushButton_clicked();
    void lightStatus(uint8_t status);
    void UpdateCurrentMatchMode(int mode_);
    void on_btn_calibration_clicked();
    void on_btn_alignment_clicked();
    void OpenImageProcessSettingDialog();
    bool ChecklicenseFileExist();
    void LoginPasswordFinish();

    void CheckLightSourceEnableTiming();
    void on_checkBox_guideline_clicked();
    void closeChangeCustomerDialog();
    void updateCustomerLabel(const QString &str1, const QString &str2);
    void on_btn_reboot_clicked();
    void WriteSettingAndExit();
    void UpdatePLCState(bool);	// Jimmy. 20220119
    void ResetAllParameters();  //garly_20211221, add for reset all parameters form plc command
    void ResetStatusByTimeout();    // Jang 20220125
    QString GetMacAddress();
    void UpdateLightSourceState(bool);
    void on_btn_cam_offline_clicked();
    void sendAlignCmd();//garly_20220120, add for turn on light source when alignment
    void on_btn_restart_clicked();
    bool ChecklicenseContent(QString file_path);	//garly_20220209, add check license function
    void Writelicense();	//garly_20220209, add check license function

    void on_btn_auth_Operator_clicked();
    void on_btn_auth_Engineer_clicked();
    void SetAlignOption(bool set_align_center); // Jang 20220411
    void SetSpaceManagementParams(int target_free_space, int maintain_days_txt, int maintain_days_img,
                                    int check_space_interval, bool check_space_after_align);// Jang 20220712

#ifdef SYNC_TIME_FROM_PLC
    void UpdateAdjustTimeSetting(bool state, uint8_t syncTimeIndex);
#endif
    void UpdateCurrentTime();
    void UpdateTitle(int mode);
    void updateLightSourceDeivceSN(QString str);
    void ConfigLightSourceNetParm(QString ip, int port, bool isModify);
    void UpdateLightSrcEnableTiming(uint8_t);

    void CloseRecipeDialog();
    void SaveTemplateHistory(); // Jang 20220504
    void RecordLastModifiedTime();  // Jimmy, 20220518.
    void SaveAlignImages(); // Jang 20220802
};

#endif // ALIGNSYS_H
