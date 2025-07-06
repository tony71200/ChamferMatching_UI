#include "alignsys.h"
#include "ui_alignsys.h"
#include <iostream>
using namespace std;

AlignSys::AlignSys(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::AlignSys)
{
    // Alignment System Path Setting
    ui->setupUi(this);
    root_path_ = QString::fromStdString( fs::current_path());
    root_path_ += "/";
    qDebug() << ">========== AlignSys Start ==========<";
    qDebug() << "Current path: " << root_path_;

    system_config_path_ = QString(root_path_ + "Data/system_config/");  // 20220715 Jimmy.

    CheckSystemConfigDirAndSystemwisePara();    // Jimmy 20220715.

    /// ========== Log Writers Setting ==========
    /// lw: system log.                 Dir: Log/systemlog/
    /// lw_result: result log.          Dir: /Log/align/
    /// lw_mem: Process memory log.     Dir: /Log/process_memory
    /// ol::lw: Log of opreation.       Dir: /Log/operate/
    lw.SetDirPath(root_path_.toStdString() + "Log");
    lw.SetShowDate(true);
    lw.SetLogName("systemlog");

    lw_result.SetDirPath(root_path_.toStdString() + "Log");
    lw_result.SetShowDate(true);
    lw_result.SetLogName("resultlog");

    lw_mem.SetDirPath(root_path_.toStdString() + "Log");
    lw_mem.SetShowDate(true);
    lw_mem.SetLogName("process_memory");

    // Jang 20220419
    // Save the default parameter files
    match.CreateDefaultParamsIniFile(root_path_.toStdString() + "Data/default_matching_parameters.ini");
    SaveDefaultParamsINI(root_path_.toStdString() + "Data/default_system_parameters.ini");

    // Jang 20220413
    // Logs of operator
    ol::lw.SetDirPath(root_path_.toStdString() + "Log");
    ol::lw.SetShowDate(true);
    ol::lw.SetLogName("operate");

    //garly_20220224, for display current system time
    UpdateCurrentTime();

    // Customer designed label
    ui->lab_customer->installEventFilter(this);     //garly_20211221, add for change customer/factory
    // Modify the path to /system_config. Jimmy 20220715.
    std::string customer_lbl_path = system_config_path_.toStdString() + "customer_label.ini";
    fio::FileIO if_customer_lbl;    // FileIO, Jimmy. 20220413. #fio01
    if(dm::log) qDebug() << "line " << __LINE__;
    std::string customer_name = "INNOLUX LCD3";
    std::string station_name = "CT2";

    if (!fio::exists(customer_lbl_path)) {  // if file is not existed, create a default new one.
        // Check /system_config folder is existed. If it is not, then create new. Jimmy 20220715.
        if (!fio::exists(system_config_path_.toStdString()))
            fio::mkdir(system_config_path_.toStdString());

        if_customer_lbl = fio::FileIO(customer_lbl_path, fio::FileioFormat::RW);
        if_customer_lbl.Write("CustomerName", customer_name);
        if_customer_lbl.Write("StationName", station_name);
    } else {
        if_customer_lbl = fio::FileIO(customer_lbl_path, fio::FileioFormat::R);
    }

    customer_name = if_customer_lbl.Read("CustomerName");
    station_name = if_customer_lbl.Read("StationName");
    if_customer_lbl.close();

    ui->lab_customer->setText(QString("%1, %2")
                              .arg(QString::fromStdString(customer_name))
                              .arg(QString::fromStdString(station_name)));
    ui->textEdit_log->setTextColor(QColor(0xFFFFFF));
    authority_ = new int();
    *authority_ = _AUTH_OPERATOR_;
    //FIXME: for debug - directly go to super user mode.
//    *authority_ = _AUTH_SUPER_USER_;
    ui->label_current_authority->setVisible(false);
    ui->btn_auth_Operator->setStyleSheet("background-color: rgb(253, 185, 62);"
                                         "color: rgb(46, 52, 54);");
    ui->btn_auth_Engineer->setStyleSheet("");

    time_log.SetProjectPath(root_path_.toStdString());

    SystemLog("[SRT][System Init] ##### System Initialize #####");

    UpdatePLCState(false);  		//garly_20220103, modify for connect state
    UpdateLightSourceState(false);	//garly_20220103, modify for connect state

    ///****************** Load system setting ******************///
    //garly modify
    SystemLog("[AXN] Loading system configuration.");
    currentRecipeName = "";
    recipe_num_list_.clear();
    currentMode = _MODE_IDLE_;
    bool systemConfig_exist = LoadSystemConfig();
    recipe_num_list_ = ListAllRecipeNum();  // Jimmy 20220617

    if (!systemConfig_exist)
    {
        LoadSelectRecipe(); // Jang added 20220217
        SystemLog("[FAIL] Fail to load system config!");
        if (currentRecipeName.isEmpty())
            currentRecipeName = "recipe_0000.txt";  //default
        SystemLog(QString("[AXN] Set default recipe name: %1").arg(currentRecipeName));
        WriteSystemConfig();
    }
    else
    {
        SystemLog("[OK] Load system config. successfully!");
        if (currentRecipeName.isEmpty())
            currentRecipeName = "recipe_0000.txt";  //default
        SystemLog(QString("[RSLT] Current type: %1, current recipe name: %2").arg(currentMode).arg(currentRecipeName));
    }
    recipe_dir = currentRecipeName.split("_").at(1).split(".").at(0);
    qDebug() << "Current recipe: " << currentRecipeName;

    LoadSystemSetting();

    ui->label_systemType_cn->setText("對位系統");
    ui->label_systemType_en->setText("Alignment System");

    if (sys_set.cam_title_swap == false)	//garly_20230206, cam title swap
    {
        ui->groupBox_Cam1->setTitle("相機1視野 Camera 1 FOV");
        ui->groupBox_Cam2->setTitle("相機2視野 Camera 2 FOV");
    }
    else if (sys_set.cam_title_swap == true)
    {
        ui->groupBox_Cam1->setTitle("相機2視野 Camera 2 FOV");
        ui->groupBox_Cam2->setTitle("相機1視野 Camera 1 FOV");
    }
#ifdef SYNC_TIME_FROM_PLC
    syncTimeMode = sys_set.sync_time_mode;
    autoSyncTimeUnit = sys_set.sync_time_unit;
#endif

    UpdateCalibrationParms(sys_set.calib_distance, sys_set.calib_rot_degree);
    align.SetTOL(sys_set.tolerance);
    align.SetCalibDistTolerance(sys_set.calib_dist_diff_tolerance);

    ///****************** camera L/R Initialize ******************///
    camL = new CaptureThreadL();
    camR = new CaptureThreadR();
    camL->SetCameraRotation(sys_set.cam_rot_l);
    camR->SetCameraRotation(sys_set.cam_rot_r);
    camL->SetExposureGain(sys_set.cam_exposure, sys_set.cam_gain);
    camR->SetExposureGain(sys_set.cam_exposure, sys_set.cam_gain);
    camL->SelectCam(sys_set.camera_l_idx);
    camR->SelectCam(sys_set.camera_r_idx);
    camL->setVideoFormat(sys_set.crop_l.crop_x, sys_set.crop_l.crop_y, sys_set.crop_l.crop_w, sys_set.crop_l.crop_h, sys_set.cam_flip_l);
    camR->setVideoFormat(sys_set.crop_r.crop_x, sys_set.crop_r.crop_y, sys_set.crop_r.crop_w, sys_set.crop_r.crop_h, sys_set.cam_flip_r);

    ///****************** markerSettingDialog Initialize ******************///
    markerSettingDialog = new MarkerSettingDialog(this);
    markerSettingDialog->move((this->width() - markerSettingDialog->width())/2 + 200, (this->height() - markerSettingDialog->height())/2 - 10);
    markerSettingDialog->UpdateTolUnit(QString::fromStdString(sys_set.tolerance_unit));
    markerSettingDialog->SetAuthority(this->authority_);
    markerSettingDialog->SetProjectPath(root_path_, recipe_dir);
    markerSettingDialog->SetDistanceCal(align.GetDistanceCal());
    markerSettingDialog->SetAngCal(align.GetAngCal());
    markerSettingDialog->SetTOL(align.GetTOL());
    markerSettingDialog->SetCalibDistTolerance(sys_set.calib_dist_diff_tolerance);
    markerSettingDialog->SetAlignDistanceTol(sys_set.align_distance_tol);

    markerSettingDialog->InitUI();
    // Setting dialog connect
    QObject::connect(this, &AlignSys::signalMarkDetectDialogHint, markerSettingDialog, &MarkerSettingDialog::Hint);

    QObject::connect(markerSettingDialog, SIGNAL(signalLoadTemplate()), this, SLOT(ActionLoadTempFromFile()));
    QObject::connect(markerSettingDialog, SIGNAL(signalCreatTemplate()), this, SLOT(ActionCreatTemp()));
    QObject::connect(markerSettingDialog, SIGNAL(signalMaskRoi()), this, SLOT(ActionMaskRoi()));    // Jimmy. 20220330. Mask
    QObject::connect(markerSettingDialog, SIGNAL(signalOpenCamera()), this, SLOT(ActionOpenCamera()));
    QObject::connect(markerSettingDialog, SIGNAL(signalMarkerDetect()), this, SLOT(ActionMarkerDetect()));
    QObject::connect(markerSettingDialog, SIGNAL(signalMarkerDetect(int)), this, SLOT(ActionMarkerDetect(int)));
    QObject::connect(markerSettingDialog, SIGNAL(signalSetRef(int)), this, SLOT(ActionSetRef(int)));
    QObject::connect(markerSettingDialog, SIGNAL(signalCloseMarkerSettingDialog()), this, SLOT(UpdateSettingDialogFlag()));
    QObject::connect(markerSettingDialog, SIGNAL(signalUpdateSetting()), this, SLOT(UpdateMarkerSetting()));
    QObject::connect(markerSettingDialog, SIGNAL(signalSetTemplateName(QString)), this, SLOT(UpdateCurrentTemplateName(QString)));
    QObject::connect(markerSettingDialog, SIGNAL(signalFinishCreatTemplate(QString)), this, SLOT(ProcessFinishCreatTemplate(QString)));
    QObject::connect(markerSettingDialog, SIGNAL(signalFinishMaskROI()), this, SLOT(ProcessFinishMaskROI()));
    QObject::connect(markerSettingDialog, SIGNAL(signalChangedMatchSetting(QString)), this, SLOT(ProcessRetrainTemplate(QString)));
    QObject::connect(markerSettingDialog, SIGNAL(signalOpenProcessMsg()), this, SLOT(HoldProcessMsg()));// Jang 20220415
    QObject::connect(markerSettingDialog, SIGNAL(signalCloseProcessMsg()), this, SLOT(CloseProcessMsg()));// Jang 20220415
    QObject::connect(markerSettingDialog, SIGNAL(signalUpdateRSTtemplate(cv::Mat)), this, SLOT(UpdateRSTtemplate(cv::Mat)));
    QObject::connect(markerSettingDialog, SIGNAL(signalUpdateTemplates(cv::Mat, cv::Mat)), this, SLOT(UpdateTemplates(cv::Mat, cv::Mat)));
    QObject::connect(markerSettingDialog, SIGNAL(signalSaveTemplateHistory()), this, SLOT(SaveTemplateHistory()));  // Jang 20220506

    QObject::connect(markerSettingDialog, SIGNAL(signalUpdateROIRectInfo(cv::Rect)), this, SLOT(UpdateROIRectInfo(cv::Rect)));  // debug0420. Jimmy
    QObject::connect(markerSettingDialog, SIGNAL(signalMaskEnabled(bool)), this, SLOT(UpdateMaskEnabled(bool)));    // Jimmy. 20220330. Mask
    QObject::connect(markerSettingDialog, SIGNAL(signalDebugMsg()), this, SLOT(OpenDebugDialog()));
    QObject::connect(markerSettingDialog, SIGNAL(signalRefreshUI()), this, SLOT(ActionRefreshUI()));
    QObject::connect(markerSettingDialog, SIGNAL(signalClearLogMsg()), this, SLOT(ActionClearLogMsg()));
    QObject::connect(markerSettingDialog, SIGNAL(signalUpdateCalibrationParms(double, double)), this, SLOT(UpdateCalibrationParms(double, double)));
    QObject::connect(markerSettingDialog, SIGNAL(signalUpdateAlignmentParms(double, int, double)), this, SLOT(UpdateAlignmentParms(double, int, double)));

    QObject::connect(markerSettingDialog, SIGNAL(signalEnableEnginneringMode(bool)), this, SLOT(EnableEnginneringMode(bool)));
    QObject::connect(markerSettingDialog, SIGNAL(signalAdjustLightBrightness()), this, SLOT(AutoAdjustLightBrightness()));
    QObject::connect(markerSettingDialog, SIGNAL(signalSetMatchMode(int)), this, SLOT(UpdateCurrentMatchMode(int)));

//    QObject::connect(markerSettingDialog, SIGNAL(signalUpdateTemplateMask(cv::Mat)), this, SLOT(UpdateTemplateMask(cv::Mat)));
    QObject::connect(markerSettingDialog, SIGNAL(signalUpdateSrcImgMask(cv::Mat)), this, SLOT(UpdateSrcImgMask(cv::Mat)));

#ifdef ORIGINAL_SHAPE_ALIGN
    shapeSettingDialog = new ShapeSettingDialog(this);
    shapeSettingDialog->SetProjectPath(systemConfigPath, recipe_dir);
    shapeSettingDialog->SetDistanceCal(align.GetDistanceCal());
    shapeSettingDialog->SetAngCal(align.GetAngCal());
    shapeSettingDialog->SetTOL(align.GetTOL());
    QObject::connect(shapeSettingDialog, SIGNAL(signalCloseShapeSettingDialog()), this, SLOT(CloseShapeSettingDialog()));
    QObject::connect(shapeSettingDialog, SIGNAL(signalSetRef(int)), this, SLOT(ActionSetRef(int)));
    QObject::connect(shapeSettingDialog, SIGNAL(signalShapeDetect()), this, SLOT(ActionMarkerDetect())) ;
    QObject::connect(shapeSettingDialog, SIGNAL(signalUpdateCalibrationParms(double, double)), this, SLOT(UpdateCalibrationParms(double, double)));
    QObject::connect(shapeSettingDialog, SIGNAL(signalUpdateAlignmentParms(double)), this, SLOT(UpdateAlignmentParms(double)));
    QObject::connect(shapeSettingDialog, SIGNAL(signalSetMode()), this, SLOT(SetShapeMode())) ;
#endif

    ///****************** BarcodeDialog Initialize ******************///
    // Barcode Detection. Jimmy. 20220302
#ifdef BARCODE_READER
    barcodeDialog = new BarcodeDialog(this);
    barcodeDialog->InitUI();
    QObject::connect(this, &AlignSys::signalBarcodeImage, barcodeDialog, &BarcodeDialog::SetCurrentImage);
    QObject::connect(this, &AlignSys::signalBarcodeDetect, barcodeDialog, &BarcodeDialog::on_btn_Detect_clicked);
    QObject::connect(barcodeDialog, &BarcodeDialog::signalSendBarcodeResult, this, &AlignSys::ReceiveBarcodeResult);
    QObject::connect(barcodeDialog, &BarcodeDialog::signalUpdateBarcodeDetectCam, this, [this](bool isCamR){
        if (isCamR)
            barcodeDialog->SetCurrentImage(_Mat2QImg(camR->GetCurImg()));
        else
            barcodeDialog->SetCurrentImage(_Mat2QImg(camL->GetCurImg()));
    });
#endif

    ///****************** SystemSettingDialog Initialize ******************///
    systemSettingDialog = new SystemSettingDialog(this);
    systemSettingDialog->SetAuthority(this->authority_);
    systemSettingDialog->SetProjectPath(root_path_, recipe_dir);
    systemSettingDialog->SetSystemSetting(this->sys_set);
    systemSettingDialog->SetBrightnessVal(0, 0);
    QObject::connect(systemSettingDialog, SIGNAL(signalSetBrightnessCamL(int)), this, SLOT(setCamLBrightness(int)));
    QObject::connect(systemSettingDialog, SIGNAL(signalSetBrightnessCamR(int)), this, SLOT(setCamRBrightness(int)));
    QObject::connect(systemSettingDialog, SIGNAL(signalCloseSettingDialog(uint8_t)), this, SLOT(closeSettingDialog(uint8_t)));
    QObject::connect(systemSettingDialog, SIGNAL(signalImageProcessDialog()), this, SLOT(OpenImageProcessSettingDialog()));
    QObject::connect(systemSettingDialog, SIGNAL(signalSaveSysSet(SystemSetting)), this, SLOT(SetSystemSetting(SystemSetting)));
    QObject::connect(systemSettingDialog, SIGNAL(signalParameterSettingDialog()), this, SLOT(OpenParameterSettingDialog()));
    // Catch the enable timing info. Jimmy 20220630.
    QObject::connect(systemSettingDialog, &SystemSettingDialog::signalLightSrcEnableTiming, this, &AlignSys::UpdateLightSrcEnableTiming);
    QObject::connect(systemSettingDialog, &SystemSettingDialog::signalRestart, this, [this](){
        RecordLastModifiedTime();   // Record closed time. Jimmy 20220518.
        is_reset_CCDsys_ = true;
        ResetAllParameters();
    });
#ifdef SYNC_TIME_FROM_PLC
    QObject::connect(systemSettingDialog, SIGNAL(signalAdjustTimeSetting(bool, uint8_t)), this, SLOT(UpdateAdjustTimeSetting(bool, uint8_t)));
#endif
    QObject::connect(systemSettingDialog, SIGNAL(signalConfigLightSourceNetParm(QString, int, bool)), this, SLOT(ConfigLightSourceNetParm(QString, int, bool)));
    QObject::connect(systemSettingDialog, SIGNAL(signalOtherSettingDialog()), this, SLOT(OpenOtherSettingDialog())); // Jang 20220411

    // Jang 20220415
    // Waiting message
    msgDialog = new MsgBoxDialog(this);

    ///****************** UI Initialize ******************///
    InitializeUI();
    ui->textEdit_log->setText("");
    QPixmap logo_pixmap(":/Data/icon/contrel.png");
    ui->logo->setPixmap(logo_pixmap);
    ui->logo->show();
    ui->logo->installEventFilter(this);

    ui->lab_systemVersion->setText(QString("v%1").arg(SOFTWARE_VERSION));    //garly_20211227, add for software version
    ui->lab_systemVersion->installEventFilter(this);
    ui->lab_systemVersion->show();

    ui->pushButton->setVisible(false);
    ui->btn_Test->setVisible(false);
    ui->btn_Detect->setVisible(false);
    ui->btn_shapeSetting->setVisible(false);
    ui->btn_cameraSetting->setVisible(false);
    ui->btn_saveCurrentImage->setVisible(false);
    ui->lineEdit_currentFilename->setVisible(false);
    ui->lbl_CoorTR1->setVisible(false);
    ui->lbl_CoorTR2->setVisible(false);
    ui->lbl_CoorO1->setVisible(false);
    ui->lbl_CoorO2->setVisible(false);
    ui->btn_restart->setVisible(false);
    ui->btn_exit->setVisible(false);
    ui->btn_calibration->setVisible(false);
    ui->btn_alignment->setVisible(false);
    ui->statusBar->setVisible(false);           //garly_20220111, hide the statusBar
    ui->btn_cam_offline->setVisible(false);
    ui->checkBox_Test_Ignore_NG->setVisible(false);
    ui->checkBox_Test_Ignore_NG->setChecked(false);

    // ==================== Threads =======================
    SystemLog("[AXN] Camera Initialize...");
    ///****************** Camera Thread ********************///
    cam_setting_path =(root_path_ + "/Data/" + recipe_dir + "/parameters").toStdString();
    frameL = cv::Mat(); // cv::Mat::zeros(cv::Size(1, 1),CV_8UC3);
    frameR = cv::Mat(); // cv::Mat::zeros(cv::Size(1, 1),CV_8UC3);

    qRegisterMetaType<cv::Mat>("cv::Mat");
    QObject::connect(camL, SIGNAL(ProcessedImgL(cv::Mat)), this, SLOT(UpdateFrameL(cv::Mat)));
    QObject::connect(camL, SIGNAL(signalCameraLErrorMsg(QString)), this, SLOT(setCamOpenFailed(QString)));
    QObject::connect(camL, &CaptureThreadL::signalWriteErrorCodeToPlc, this, &AlignSys::WriteErrorCodeToPlc);
    //Restart to fix the camera problem. Jimmy. 20220610
//    QObject::connect(camL, SIGNAL(signalCameraLErrorMsg(QString)), this, SLOT(AutoRestartSystem(QString)));
    QObject::connect(camL, SIGNAL(signalCameraConnectionTimeout(QString)), this, SLOT(AutoRestartSystem(QString)));
    QObject::connect(camL, SIGNAL(signalMemChek()), this, SLOT(RestartMemCheck()));
    QObject::connect(camL, SIGNAL(signalSetEmptyImg(int)), this, SLOT(removeCamImage(int)));
    // Jang 20220408
    QObject::connect(this, SIGNAL(signalCaptureReadyLeft()), camL, SLOT(CaptureReady()));

    //-----------------------------------------------------

    qRegisterMetaType<cv::Mat>("cv::Mat");
    QObject::connect(camR, SIGNAL(ProcessedImgR(cv::Mat)), this, SLOT(UpdateFrameR(cv::Mat)));
    QObject::connect(camR, SIGNAL(signalCameraRErrorMsg(QString)), this, SLOT(setCamOpenFailed(QString)));
    QObject::connect(camR, &CaptureThreadR::signalWriteErrorCodeToPlc, this, &AlignSys::WriteErrorCodeToPlc);
//    QObject::connect(camR, SIGNAL(signalCameraRErrorMsg(QString)), this, SLOT(AutoRestartSystem(QString)));  //Restart system to fixed
    QObject::connect(camR, SIGNAL(signalCameraConnectionTimeout(QString)), this, SLOT(AutoRestartSystem(QString)));  //Restart system to fixed
    QObject::connect(camR, SIGNAL(signalSetEmptyImg(int)), this, SLOT(removeCamImage(int)));
    // Jang 20220408
    QObject::connect(this, SIGNAL(signalCaptureReadyRight()), camR, SLOT(CaptureReady()));
    //-----------------------------------------------------
    // Calibration connect
    QObject::connect(this, &AlignSys::sig_finishCal, this, &AlignSys::HandleCal);
    QObject::connect(this, &AlignSys::sig_readyCal, this, &AlignSys::DoCalib);

    //=====================================================

    // Jang modified: calibration mode is according to the machine's structure.
    // _MODE_CALIB_XYT_, _MODE_CALIB_YT_, _MODE_CALIB_XT_
    align.SetSystemMode(this->machine_type, this->calibration_mode, this->system_unit);

    align.SetProjectPath(root_path_, recipe_dir);
    parameter_setting_path = (root_path_ + "/Data/" + recipe_dir + "/parameters/").toStdString();

    shape_align_0.SetDataPath(parameter_setting_path, root_path_.toStdString() + "/Log");
    shape_align_1.SetDataPath(parameter_setting_path, root_path_.toStdString() + "/Log");
    shape_align_0.SetObjectCamId(0, 0);
    shape_align_1.SetObjectCamId(0, 1);

    markerSettingDialog->SetImageProcessing(shape_align_0.GetImageProc());

#ifdef ENABLE_AUTO_ADJUST_BRIGHTNESS
    //FIXME: future function - automatical brightness adjustment
    // ==================== Brightness Adjustment Function ====================
    is_running_bright_adjust = false;

    cameraSettingDialog = new CameraSettingDialog(this);
    cameraSettingDialog->InitUI();
    cameraSettingDialog->SetProjectPath((systemConfigPath + "/Data/").toStdString(), recipe_dir.toStdString());

    param_L = new CameraSettingParams();
    param_L->mouse_left = false;
    param_L->mouse_right = false;
    param_L->mouse_release = false;
    param_L->is_create_roi = false;
    param_L->mouseRightClickCounter = 0;
    param_L->brightness_opt_dev_id = 2;
    param_R = new CameraSettingParams();
    param_R->mouse_left = false;
    param_R->mouse_right = false;
    param_R->mouse_release = false;
    param_R->is_create_roi = false;
    param_R->mouseRightClickCounter = 0;
    param_R->brightness_opt_dev_id = 1;
    cameraSettingDialog->SetCameraSettingParams(param_L, param_R);
    systemSettingDialog->SetCameraSettingParams(param_L, param_R);
    // Control the external light source
    adjust_bright = new AdjustBrightThread();
    cameraSettingDialog->SetAdjustBrightThread(adjust_bright);
#endif

    ///****************** Opt (light source) Connect ********************///
    /// The main OPT control part is in the Class cameraSettingDialog
    SystemLog("[AXN] OPT Light Source Initialize...");
    // OPT connection by Jimmy. 20220119
    opt_status_ = 0;
    opt_recipe_path_ = (root_path_ + "/Data/").toStdString() + "/" + recipe_dir.toStdString();
    connector_opt_ = new OptConnector();
    thread_opt_ = new QThread();

    connector_opt_->moveToThread(thread_opt_);
    /// Signal communication to threads
    QObject::connect(this, &AlignSys::signalOptInit, connector_opt_, &OptConnector::Initialize);
    QObject::connect(this, &AlignSys::signalOptDisconnect, connector_opt_, &OptConnector::Disconnect);
    QObject::connect(this, &AlignSys::signalOptSetChannelState, connector_opt_, &OptConnector::SetChState);
    QObject::connect(this, &AlignSys::signalSetChannelBrightness, connector_opt_, &OptConnector::SetChBrightness);
    QObject::connect(this, &AlignSys::signalSetNetworkParameter, connector_opt_, &OptConnector::SetNetworkParms);
    /// signal from cameraSettingDialog
#ifdef ENABLE_AUTO_ADJUST_BRIGHTNESS
    QObject::connect(cameraSettingDialog, &CameraSettingDialog::signalOptDisconnect, this, &AlignSys::signalOptDisconnect);
#endif
    /// Release stack
    QObject::connect(thread_opt_, &QThread::finished, connector_opt_, &QObject::deleteLater);
    QObject::connect(thread_opt_, &QThread::finished, thread_opt_, &QObject::deleteLater);
    /// Set wild pointer to nullptr
    QObject::connect(thread_opt_, &QObject::destroyed, this, [this](){ thread_opt_ = nullptr; });
    QObject::connect(connector_opt_, &QObject::destroyed, this, [this](){ connector_opt_ = nullptr; });
    /// Signal for alignsys action from thread
    QObject::connect(connector_opt_, &OptConnector::signalState, this, &AlignSys::UpdateLightSourceState);
    QObject::connect(connector_opt_, &OptConnector::signalLogs, this, &AlignSys::PrintOptLogs);
    QObject::connect(connector_opt_, &OptConnector::signalWriteErrorCodeToPlc, this, &AlignSys::WriteErrorCodeToPlc);
    QObject::connect(connector_opt_, &OptConnector::signalDeviceSN, this, &AlignSys::updateLightSourceDeivceSN);
    QObject::connect(connector_opt_, &OptConnector::signalConfigOptIpResult, this, &AlignSys::updateOptConfigIpResult);

    // OPT connection. Jimmy. 20220224
    thread_opt_->start();
    emit signalOptInit(QString::fromStdString(sys_set.opt_light_IP), sys_set.opt_light_port);
    isOptLightConnected = true;

    usleep(200000);    // After connection, waiting time is needed.
    if (LoadOptBrightnessInfo(opt_recipe_path_, 0) && LoadOptBrightnessInfo(opt_recipe_path_, 1)) {
        systemSettingDialog->SetBrightnessVal(opt_brightness_val_L_, opt_brightness_val_R_);
    } else {
        opt_brightness_val_L_ = 0;
        opt_brightness_val_R_ = 0;
        if (!SaveOptBrightnessInfo(opt_recipe_path_, 0) || !SaveOptBrightnessInfo(opt_recipe_path_, 1)) {
            emit signalOptDisconnect(false);
            isOptLightConnected = false;
        }
    }

#ifdef ENABLE_AUTO_ADJUST_BRIGHTNESS
    cameraSettingDialog->SetOPTDevice(thread_opt_, connector_opt_, sys_set.opt_light_IP, sys_set.opt_light_port);
    QObject::connect(cameraSettingDialog, SIGNAL(signalBrightnessResult()), this, SLOT(GetBrightnessResult()));
    QObject::connect(cameraSettingDialog, SIGNAL(signalCloseCameraSettingDialog()), this, SLOT(CloseCameraSettingDialog()));

    if (cameraSettingDialog->ConnectOPTDevice())
    {
        usleep(200000);    // After connection, waiting time is needed.
        if (cameraSettingDialog->LoadBrightnessInfo())
        {
            isOptLightConnected = true;
        }
        else
        {
            param_L->brightness_save = 0;
            param_R->brightness_save = 0;
            if(!cameraSettingDialog->SaveBrightnessInfo())
            {
                cameraSettingDialog->DisconnectOPTDevice();
                isOptLightConnected = false;
            }
        }
    }
    else
    {
        if(!cameraSettingDialog->LoadBrightnessInfo()){
            param_L->brightness_save = 0;
            param_R->brightness_save = 0;
            if(!cameraSettingDialog->SaveBrightnessInfo()){
            }
        }
        isOptLightConnected = false;
    }
#endif

    //control opt device : default turn on
    lightStatus(_LightTurnOn_);
// ========================  end  =============================

    // Image Processing
    imageprocSettingDialog = new ImageProcSettingDialog(this);
    imageprocSettingDialog->SetAuthority(this->authority_);
    imageprocSettingDialog->SetProjectPath(root_path_.toStdString(), recipe_dir.toStdString());
    imageprocSettingDialog->SetImageProcessing(shape_align_0.GetImageProc(), shape_align_1.GetImageProc());
    imageprocSettingDialog->SetCuda(sys_set.use_cuda_for_image_proc);
    QObject::connect(imageprocSettingDialog, SIGNAL(signalCloseImageProcSettingDialog()), this, SLOT(CloseImageProcSettingDialog()));
    camL->imageprocSettingDialog = imageprocSettingDialog;
    camR->imageprocSettingDialog = imageprocSettingDialog;

    // Parameters Setting
    paramSetting = new ParameterSettingDialog(this);
    paramSetting->SetAuthority(this->authority_);
    paramSetting->SetProjectPath(root_path_, recipe_dir);
    paramSetting->SetSystemSetting(this->sys_set);
    QObject::connect(paramSetting, SIGNAL(signalSaveSystemSetting(SystemSetting)), this, SLOT(SetSystemSetting(SystemSetting)));
    QObject::connect(paramSetting, SIGNAL(signalParamSettingClose()), this, SLOT(CloseParameterSettingDialog()));
    QObject::connect(paramSetting, SIGNAL(signalLoadDefaultSetting(int)), this, SLOT(LoadDefaultSystemSetting(int)));
    QObject::connect(paramSetting, &ParameterSettingDialog::signalRestart, this, [this](){
        RecordLastModifiedTime();   // Record closed time. Jimmy 20220518.
        is_reset_CCDsys_ = true;
        ResetAllParameters();
    });
    QObject::connect(paramSetting, SIGNAL(signalUpdateSysCamDirectInfo()), this, SLOT(UpdateSysCamDirectInfo())); // Jang 20220505
    QObject::connect(paramSetting, SIGNAL(signalRestoreSystemSetting()), this, SLOT(RestoreSystemSetting())); // Jang 20220510
    QObject::connect(paramSetting, SIGNAL(signalRestartCamera()), this, SLOT(RestartCamera())); // Jang 20220511
    QObject::connect(paramSetting, SIGNAL(signalUpdateCamIndex(bool)), this, SLOT(UpdateCamIndex(bool))); // Jang 20220511
    // Update the UI to show exposure time. Jimmy 20221103
    ui->lbl_exp_time_1->setText(QString("曝光時間 Exp. Time: %1").arg(this->sys_set.cam_exposure));
    ui->lbl_exp_time_2->setText(QString("曝光時間 Exp. Time: %1").arg(this->sys_set.cam_exposure));

    // Cropping ROI
    cropRoiDialog = new CropRoiDialog(this);
    crop_cam_idx_ = -1;
    QObject::connect(paramSetting, SIGNAL(signalCropRoi(int)), this, SLOT(OpenCropRoiDialog(int)));
    QObject::connect(cropRoiDialog, SIGNAL(signalCloseCropRoi(CropInfo &)), this, SLOT(CloseCropRoiDialog(CropInfo &)));
    QObject::connect(cropRoiDialog, SIGNAL(signalCancelCropRoi()), this, SLOT(CancelCropRoi()));    // Jang 20220511

    // Jang 20220411
    // Other Setting
    otherSettingDialog = new OtherSettingDialog(this);
    QObject::connect(otherSettingDialog, SIGNAL(signalCloseOtherSettingDialog()), this, SLOT(CloseOtherSettingDialog()));
    QObject::connect(otherSettingDialog, SIGNAL(signalSaveAlignOption(bool)), this, SLOT(SetAlignOption(bool)));
    QObject::connect(otherSettingDialog, &OtherSettingDialog::signalReboot, this, &AlignSys::WriteSettingAndExit);
    QObject::connect(otherSettingDialog, &OtherSettingDialog::signalRestart, this, [this](){
        RecordLastModifiedTime();   // Record closed time. Jimmy 20220518.
        is_reset_CCDsys_ = true;
        ResetAllParameters();
    });
    QObject::connect(otherSettingDialog, SIGNAL(signalSpaceManagement(int, int, int, int, bool)), this, SLOT(SetSpaceManagementParams(int, int, int, int, bool))); // Jang 20220712
    otherSettingDialog->SetProjectPath(root_path_);
    otherSettingDialog->GetParams(target_space_gb_, min_maintain_days_txt_, min_maintain_days_img_, check_space_interval_, check_space_after_align_);    // Jang 20220712

    ///****************** PLC Connect ********************///
    SystemLog("[AXN] PLC Initialize...");
#ifdef MC_PROTOCOL_PLC
    // PLC connection by Jimmy. 20220119
    plc_status_ = 0;
    connector_plc_ = new PlcConnector();
    thread_plc_ = new QThread();

    connector_plc_->moveToThread(thread_plc_);

    /// Signal communication to threads
    QObject::connect(this, &AlignSys::signalPlcSetPath, connector_plc_, &PlcConnector::SetProjectPath);
    QObject::connect(this, &AlignSys::signalPlcInit, connector_plc_, &PlcConnector::Initialize);
    QObject::connect(this, &AlignSys::signalPlcDisconnect, connector_plc_, &PlcConnector::Disconnect);
    QObject::connect(this, &AlignSys::signalPlcLoadDefaultSetting, connector_plc_, &PlcConnector::LoadDefaultSettingfromINIFile);
    QObject::connect(this, &AlignSys::signalPlcWriteSetting, connector_plc_, &PlcConnector::WriteSetting2File);
    QObject::connect(this, &AlignSys::signalPlcSetWaitingPlcAck, connector_plc_, &PlcConnector::SetWaitingPlcAck);
    QObject::connect(this, &AlignSys::signalPlcWriteResult, connector_plc_, &PlcConnector::WriteResult, Qt::BlockingQueuedConnection);
    QObject::connect(this, &AlignSys::signalPlcWriteCurrentRecipeNum, connector_plc_, &PlcConnector::WriteCurrentRecipeNum);
    QObject::connect(this, &AlignSys::signalPlcWriteCurrentErrorCodeNum, connector_plc_, &PlcConnector::WriteCurrentErrorCodeNum); //garly_20220617
    QObject::connect(this, &AlignSys::signalPlcSetCCDTrig, connector_plc_, &PlcConnector::SetCCDTrig, Qt::BlockingQueuedConnection);
    QObject::connect(this, &AlignSys::signalPlcSetStatus, connector_plc_, &PlcConnector::SetPlcStatus);
    QObject::connect(this, &AlignSys::signalPlcSetCurrentRecipeNum, connector_plc_, &PlcConnector::SetCurrentRecipeNum);
    QObject::connect(this, &AlignSys::signalPlcLoadSetting, connector_plc_, &PlcConnector::LoadSettingfromFile);
    QObject::connect(this, &AlignSys::signalPlcGetStatus, connector_plc_, &PlcConnector::GetPLCStatus);

    //garly_20220808, add for retry write when receive write fail from plc
    QObject::connect(connector_plc_, &PlcConnector::signalWriteRetry, connector_plc_, &PlcConnector::WriteCommandRetry);

#ifdef SYNC_TIME_FROM_PLC
    QObject::connect(this, &AlignSys::signalPlcSetAdjustTimeMode, connector_plc_, &PlcConnector::setSyncTimeMode);
#endif
#ifdef BARCODE_READER   //garly_20220309
    QObject::connect(this, &AlignSys::signalPlcWriteBarcodeResult, connector_plc_,&PlcConnector::WriteBarcodeResult, Qt::BlockingQueuedConnection);
#endif

    /// Release stack
    QObject::connect(thread_plc_, &QThread::finished, connector_plc_, &QObject::deleteLater);
    QObject::connect(thread_plc_, &QThread::finished, thread_plc_, &QObject::deleteLater);
    /// Set wild pointer to nullptr
    QObject::connect(thread_plc_, &QObject::destroyed, this, [this](){ thread_plc_ = nullptr; });
    QObject::connect(connector_plc_, &QObject::destroyed, this,[this](){ connector_plc_ = nullptr;});
    /// Signal for alignsys action from thread
    QObject::connect(connector_plc_, &PlcConnector::signalActionCalibration, this, &AlignSys::ActionCalib);
    //QObject::connect(connector_plc_, &PlcConnector::signalActionAlignment, this, &AlignSys::ActionAlign);
    QObject::connect(connector_plc_, &PlcConnector::signalActionSetRef, this, &AlignSys::ActionSetRef);
    QObject::connect(connector_plc_, &PlcConnector::signalState, this, &AlignSys::UpdatePLCState);
    QObject::connect(connector_plc_, &PlcConnector::signalResetAllParameters, this, [this](){
        RecordLastModifiedTime();   // Record closed time. Jimmy 20220518.
        is_reset_CCDsys_ = true;
        ResetAllParameters();
    });
    QObject::connect(connector_plc_, &PlcConnector::signalActionAlignment, this, &AlignSys::CheckLightSourceEnableTiming);
    QObject::connect(connector_plc_, &PlcConnector::signalChangeRecipe, this, &AlignSys::ChangeRecipe);
    QObject::connect(connector_plc_, &PlcConnector::signalStatus, this, &AlignSys::UpdatePlcStatus);
    QObject::connect(connector_plc_, &PlcConnector::signalLogs, this, &AlignSys::PrintPlcLogs);
#ifdef BARCODE_READER   //garly_20220308
    QObject::connect(connector_plc_, &PlcConnector::signalActionBarcodeReader, this, &AlignSys::ActionBarcodeReader);
#endif
#ifdef PRE_DETECT   //garly_20221007
    QObject::connect(connector_plc_, &PlcConnector::signalPreDetect, this, &AlignSys::ActionPreDetect);
#endif
    thread_plc_->start();
    emit signalPlcSetPath(root_path_, recipe_dir);
#ifdef SYNC_TIME_FROM_PLC
    emit signalPlcSetAdjustTimeMode(syncTimeMode, autoSyncTimeUnit);
#endif
    emit signalPlcInit();

#else
    com = new Communication();
    log(com->Connect());
    usleep(10000);
    ControlPLC(com, "Zero", nullptr);
    return_msg = "None";
    QObject::connect(com, SIGNAL(newMessage(QString)), this, SLOT(readACK(QString)));
#endif    

    //****************** Parameters ******************//
    func_mode = _MODE_NONE_;
    switch_cam = false;
    isCamStop = false;
    isFirstSnap_l = true;
    isFirstSnap_r = true;
    showGuideline = false;
    isLoadtemp = false;
    isCreatetemp = false;
    isMaskEnabled = false;
    isMaskRoi = false;
    isLoadtemp_r = false;
    isCreatetemp_r = false;
    isNoResult = false;
    isMultiResult = false;
    isRunCalib = false;
    isDoneCalib = false;
    isRunAlign = false;
    isDoneAlign = false;
    isSetRefPt = false;
    doAlignRepeat = false;
    refPtExist = false;
    isCamOpenSuccess = false;
    isAlignmentCheck = false;	//add by garly
    alignTestTime = 0;
    alignRepeatTime = 0;
    updateRefInfoL = false;
    updateRefInfoR = false;
    isCalibReadyForAlign = false;

    // For Calibration
    count_calpos = 0;
    x11_std=0;
    y11_std=0;
    x22_std=0;
    y22_std=0;

    ///****************** ELSE ******************///
    // Jimmy. modified 20211203
    ui->lbl_ImgL->setScaledContents(true);
    ui->lbl_ImgR->setScaledContents(true);

    SystemLog("[AXN] Turning on camera...");
    AutoTurnOnCamera(); // add by garly
    SystemLog("[OK] Success to turn on camera!");

    // Set current mode
    if(currentMode == _MODE_MARKER_ || currentMode == _MODE_CHAMFER_)
        markerSettingDialog->SetMode(currentMode);

    if(currentMode == _MODE_CHAMFER_){
        // Initialisation for RST matching
        match.InitPaths(root_path_.toStdString() + "Data/" + recipe_dir.toStdString() + "/methods/chamfer/match_0",
                        root_path_.toStdString() + "Log/chamfer/match_0",
                        root_path_.toStdString() + "Data/" + recipe_dir.toStdString() + "/parameters/");
        match_R.InitPaths(root_path_.toStdString() + "Data/" + recipe_dir.toStdString() + "/methods/chamfer/match_1",
                          root_path_.toStdString() + "Log/chamfer/match_1",
                          root_path_.toStdString() + "Data/" + recipe_dir.toStdString() + "/parameters/");

        qDebug  () << "Current mode: _MODE_CHAMFER_" ;
    }else{
        // Initialisation for RST matching
        match.InitPaths(root_path_.toStdString() + "Data/" + recipe_dir.toStdString() + "/methods/rst/match_0",
                        root_path_.toStdString() + "Log/rst/match_0",
                        root_path_.toStdString() + "Data/" + recipe_dir.toStdString() + "/parameters/");
        match_R.InitPaths(root_path_.toStdString() + "Data/" + recipe_dir.toStdString() + "/methods/rst/match_1",
                          root_path_.toStdString() + "Log/rst/match_1",
                          root_path_.toStdString() + "Data/" + recipe_dir.toStdString() + "/parameters/");
        qDebug  () << "Current mode: _MODE_MARKER_" ;
    }
#ifdef BARCODE_READER
    if (this->system_type == _SYSTEM_BARCODE_READER)
    {
        UpdateTitle(_MODE_BARCODE_READER_);
        OpenBarcodeDialog();
    }
    else
#endif
    UpdateTitle(currentMode);//garly_20220224, change title by matching method

    // Set image processing class
    match.SetImageProc(shape_align_0.GetImageProc(), true, sys_set.use_cuda_for_image_proc);
    match_R.SetImageProc(shape_align_1.GetImageProc(), true, sys_set.use_cuda_for_image_proc);
    if(sys_set.enable_mask){
        cv::Mat source_img_mask_l, source_img_mask_r;
        match.GetSrcImageMask(source_img_mask_l);
        match_R.GetSrcImageMask(source_img_mask_r);
        markerSettingDialog->SetMask(source_img_mask_l, source_img_mask_r);
    }

    LoadMatchModeStatus();

    // Load current recipe
    if (systemConfig_exist){
        LoadParameters(currentRecipeName);
        // Jang 20220331
        is_template_loaded_ = true;
    }

    subMenuActive = false;


    //usleep(2000000);
    SleepByQtimer(2000);
    ShowCurrentSystemStatus();

    //garly_20220209, add check license function
    if (!ChecklicenseFileExist())
    {
        loginDialog = new LoginDialog(this);
        loginDialog->setWindowFlags(Qt::FramelessWindowHint);
        loginDialog->move((1920-loginDialog->width())/2, (1080-loginDialog->height())/2);
        QObject::connect(loginDialog, SIGNAL(signalPasswordCorrect()), this, SLOT(LoginPasswordFinish()));

        loginDialogIsShown = true;
        SetMainMenuBtnEnable(false);
        loginDialog->show();
    }

    //garly_20220224, for display current system time
    timer_updateCurrentTime_ = new QTimer(this);
    QObject::connect(timer_updateCurrentTime_, &QTimer::timeout, this, &AlignSys::UpdateCurrentTime);
    // Update status signal. Jimmy 20220627
    QObject::connect(timer_updateCurrentTime_, &QTimer::timeout, this, [this](){
        if (is_stat_sig_switch_) {
            ui->lbl_stat_sig_L->setStyleSheet("QLabel { background-color : rgb(52, 67, 104); }");
            ui->lbl_stat_sig_R->setStyleSheet("QLabel { background-color : rgb(252, 175, 62); }");
        } else {
            ui->lbl_stat_sig_L->setStyleSheet("QLabel { background-color : rgb(114, 159, 207); }");
            ui->lbl_stat_sig_R->setStyleSheet("QLabel { background-color : rgb(52, 67, 104); }");
        }
        is_stat_sig_switch_ = !is_stat_sig_switch_;
    });
    timer_updateCurrentTime_->start(1000);

    // Jang 20220711, Jimmy 20220804
    // Delete logs and images which correspond to conditions
    TimerAutoDeleteLog();
    disk_spc_manager_ = new DiskSpaceManager();
    th_disk_spc_manager_ = new QThread();

    disk_spc_manager_->moveToThread(th_disk_spc_manager_);
    /// Signal communication to threads
    QObject::connect(this, &AlignSys::signalDeleteLogWithCondition,
                     disk_spc_manager_, &DiskSpaceManager::DeleteLogWithCondition);
    /// Release stack
    QObject::connect(th_disk_spc_manager_, &QThread::finished, disk_spc_manager_, &QObject::deleteLater);
    QObject::connect(th_disk_spc_manager_, &QThread::finished, th_disk_spc_manager_, &QObject::deleteLater);
    /// Signal for alignsys action from thread
    QObject::connect(disk_spc_manager_, &DiskSpaceManager::signalLogs, this, &AlignSys::PrintDiskSpcMngLogs);

    th_disk_spc_manager_->start();

    // Jang 20220307
    ChangeAuthority(*authority_);

    // If reset CCD system finished, return the OK signal to PLC. Jimmy 20220627 #fio44
    QString sysconf_file = QString(root_path_ + "/Data/systemConfig.ini");
    emit signalPlcSetCurrentRecipeNum(currentRecipeName.mid(7, 4).toUInt());
    if (fio::exists(sysconf_file.toStdString())) {
        fio::FileIO fs(sysconf_file.toStdString(), fio::FileioFormat::RW);
        std::string ret = fs.Read("IsCCDSysReset");

        if (ret == "1") {   // Changed recipe by PLC
            emit signalPlcWriteCurrentRecipeNum();    //garly_20220120, add for change recipe from plc
            emit signalPlcWriteResult(0, 0, 0, 1, PlcWriteMode::kOthers);  //result=> 0: continue; 1: finish; 2: NG

            usleep(100000);
            FinishPlcCmd();
            usleep(100000);

            fs.ReplaceByKey("IsCCDSysReset", "0");
            is_reset_CCDsys_ = false;
        }

        fs.close();
    }

    SystemLog("[END][System Init]");
    SystemLog("[SRT][System] ##### System Begin #####");
}

//garly_20211230, modify for light source reconnect
void AlignSys::UpdatePLCState(bool state)
{
    isPLCConnected = state;

    if (isPLCConnected) {
        ui->lab_plcState->setStyleSheet("background-color: rgb(52, 67, 104);color: rgb(0, 255, 0);");
        ui->lab_plcState->setText("Connect");
        //SystemLog(QString("[PLC] %1").arg(str));
    } else {
        ui->lab_plcState->setStyleSheet("background-color: rgb(52, 67, 104);color: rgb(255, 0, 0);");
        ui->lab_plcState->setText("Disconnect");
        //SystemLog(QString("%1").arg(str));
    }
//    lw_mem << "Using memory(" << std::to_string((int)getpid()) << "): " << (float)GetCurrentMem() / 1000 << lw_mem.endl;
}

void AlignSys::updateLightSourceDeivceSN(QString str)
{
    this->sys_set.opt_light_sn = str.toStdString();

    systemSettingDialog->autoUpdateLightSourceNetParm(QString::fromStdString(this->sys_set.opt_light_IP),
                                                      this->sys_set.opt_light_port,
                                                      QString::fromStdString(this->sys_set.opt_light_sn));
}

void AlignSys::UpdateTitle(int mode)//garly_20220224, change title by matching method
{
    if (mode == _MODE_MARKER_)
    {
        ui->label_systemType_cn->setText("標記對位系統");
        ui->label_systemType_en->setText("Alignment System");
        ui->btn_markerSetting->setText("標記偵測\nMarker Detect");
        ui->btn_markerSetting->setVisible(true);
    }
    else if (mode == _MODE_CHAMFER_)
    {
        ui->label_systemType_cn->setText("外型對位系統");
        ui->label_systemType_en->setText("Alignment System");
        ui->btn_markerSetting->setText("外型偵測\nShape Detect");
        ui->btn_markerSetting->setVisible(true);
    }
    else if (mode == _MODE_BARCODE_READER_)
    {
        ui->label_systemType_cn->setText("條碼辨識系統");
        ui->label_systemType_en->setText("Barcode System");
        ui->btn_markerSetting->setVisible(false);
    }
    else
    {
        ui->label_systemType_cn->setText("對位系統");
        ui->label_systemType_en->setText("Alignment System");
    }
}

void AlignSys::ChangeAuthority(int auth){
    if (auth == _AUTH_SUPER_USER_) {
        *authority_ = _AUTH_SUPER_USER_;
        if(dm::log) {
            qDebug() << "_AUTH_SUPER_USER_" ;
        }
        ui->label_current_authority->setText("Super User");
        ui->label_current_authority->setVisible(true);
        ui->btn_restart->setVisible(true);
        ui->btn_exit->setVisible(true);
        ui->btn_calibration->setVisible(true);
        ui->btn_alignment->setVisible(true);
        ui->btn_cam_offline->setVisible(true);
        ui->checkBox_Test_Ignore_NG->setVisible(true);
        EnableEnginneringMode(true);

        ui->btn_auth_Operator->setStyleSheet("");
        ui->btn_auth_Engineer->setStyleSheet("");
    }
    else if(auth == _AUTH_ENGINEER_){
        *authority_ = _AUTH_ENGINEER_;
        if(dm::log) {
            qDebug() << "_AUTH_ENGINEER_" ;
        }
        ui->label_current_authority->setText("Engineer");
        ui->label_current_authority->setVisible(false);
        ui->btn_restart->setVisible(true);
        ui->btn_exit->setVisible(true);
        ui->btn_calibration->setVisible(false);
        ui->btn_alignment->setVisible(false);
        ui->btn_cam_offline->setVisible(false);
        ui->checkBox_Test_Ignore_NG->setVisible(false);
        ui->checkBox_Test_Ignore_NG->setChecked(false);
        EnableEnginneringMode(false);

        ui->btn_auth_Operator->setStyleSheet("");
        ui->btn_auth_Engineer->setStyleSheet("background-color: rgb(253, 185, 62);"
                                             "color: rgb(46, 52, 54);");
    }
    else{
        *authority_ = _AUTH_OPERATOR_;
        if(dm::log) {
            qDebug() << "_AUTH_OPERATOR_" ;
        }
        ui->label_current_authority->setText("Operator");
        ui->label_current_authority->setVisible(false);
        ui->btn_restart->setVisible(false);
        ui->btn_exit->setVisible(false);
        ui->btn_calibration->setVisible(false);
        ui->btn_alignment->setVisible(false);
        ui->btn_cam_offline->setVisible(false);
        ui->checkBox_Test_Ignore_NG->setVisible(false);
        ui->checkBox_Test_Ignore_NG->setChecked(false);
        EnableEnginneringMode(false);

        ui->btn_auth_Operator->setStyleSheet("background-color: rgb(253, 185, 62);"
                                             "color: rgb(46, 52, 54);");
        ui->btn_auth_Engineer->setStyleSheet("");
    }

    closeChangeAuthorityDialog();
}


bool AlignSys::SaveMatchModeStatus(){
    // TEST
    std::string dir_path = root_path_.toStdString() + "/Data/" + recipe_dir.toStdString();
    std::string file_path = dir_path + "/match_mode_status.txt";

    if(!fs::exists(dir_path))   fs::create_directories(dir_path);

    fio::FileIO write_file(file_path, fio::FileioFormat::W);    // FileIO. Jimmy. 20220413. #fio02

    if (!write_file.isOK()) {
        if (dm::log) std::cerr << "File isn't opened." << std::endl;
        return false;
    }

    write_file.Write("currentMode", "=", std::to_string(currentMode));
    write_file.Write("enum _Mode_", "->", "0: _MODE_IDLE_, 1: _MODE_MARKER_, 2: _MODE_CHAMFER_, 3: _MODE_SHAPE_ROI_, 4: _MODE_SHAPE_ONE_WAY_");

    match_mode_single_or_multi = markerSettingDialog->getCurrentMode();
    write_file.Write("singleOrMultiMatch", "=", std::to_string(markerSettingDialog->getCurrentMode()));
    write_file.Write("enum _MARKER_MODE_", "->", "0: _ONLY_TEMPLATE_, 1: _EACH_TEMPLATE_L_, 2: _EACH_TEMPLATE_R_");

    write_file.Write("leftTemplate", "=", std::to_string(left_template_collected));
    write_file.Write("", "->", "0: Not Collected, 1: Collected");
    write_file.Write("rightTemplate", "=", std::to_string(right_template_collected));
    write_file.Write("", "->", "0: Not Collected, 1: Collected");
    write_file.Write("isMatchingAvailable", "=", std::to_string(is_match_available));
    write_file.Write("", "->", "0: Not available, 1: Available");

    write_file.close();

    return true;
}
bool AlignSys::LoadMatchModeStatus(){
    std::string file_path = root_path_.toStdString() + "/Data/" + recipe_dir.toStdString() + "/match_mode_status.txt";
    fio::FileIO get_file(file_path, fio::FileioFormat::R);  // FileIO. Jimmy. 20220418. #fio03

    if(!get_file.isOK()){
        if (dm::log) std::cerr << "[WARN] File \"match_mode_status.txt\" isn't opened. "
                                  "Recreate a new one." << std::endl;
        if (!SaveMatchModeStatus()) {
            return false;
        } else {
            get_file = fio::FileIO(file_path, fio::FileioFormat::R);
        }
    }
    bool read_all_ok = true;
    try{
        if(currentMode != _MODE_IDLE_){
            is_match_available = get_file.ReadtoInt( "currentMode", is_match_available);
        }
        match_mode_single_or_multi = get_file.ReadtoInt("singleOrMultiMatch", match_mode_single_or_multi);
        left_template_collected = get_file.ReadtoInt( "leftTemplate", left_template_collected);
        right_template_collected = get_file.ReadtoInt( "rightTemplate", right_template_collected);
        is_match_available = get_file.ReadtoInt( "isMatchingAvailable", is_match_available);

        read_all_ok = get_file.isAllOK();
        get_file.close();
    }
    catch(...){
        get_file.close();
        read_all_ok = false;
    }

    if(!read_all_ok){
        if(dm::log){
            qDebug() << "[DEBUG] Reading files failed. Func " << __func__ << ", line " << __LINE__;
        }
        SaveMatchModeStatus();
    }

    is_new_collecting_left = true;
    is_new_collecting_right = true;

    return true;

}

void AlignSys::ShowCurrentSystemStatus(){
    std::vector<std::string> status_str_list;
    std::string status_str;
    // PLC
    if (isPLCConnected){
        status_str = "[OK] PLC已連線.";
        status_str_list.push_back(status_str);
    }
    else{
        status_str = "[FAIL] PLC未連線.";
        status_str_list.push_back(status_str);
    }
    // Camera
    if(camL->IsConnected()){
        status_str = "[OK] 左相機已連線.";
        status_str_list.push_back(status_str);
    }else{
        status_str = "[FAIL] 左相機未連線.";
        status_str_list.push_back(status_str);
    }
    if(camR->IsConnected()){
        status_str = "[OK] 右相機已連線.";
        status_str_list.push_back(status_str);
    }else{
        status_str = "[FAIL] 右相機未連線.";
        status_str_list.push_back(status_str);
    }
    // Opt light source
    if (isOptLightConnected){
        status_str = "[OK] 光源控制器已連線.";
        status_str_list.push_back(status_str);
    }
    else{
        status_str = "[FAIL] 光源控制器未連線.";
        status_str_list.push_back(status_str);
    }

    // Mode
    if(currentMode == _MODE_IDLE_){
        status_str = "[MODE] 閒置 IDLE.";
        status_str_list.push_back(status_str);
    }
    else if(currentMode == _MODE_MARKER_){
        status_str = "[MODE] 標記特徵辨識 RST Matching.";
        status_str_list.push_back(status_str);
    }
    else if(currentMode == _MODE_CHAMFER_){
        status_str = "[MODE] 外型特徵辨識 Chamfer Matching.";
        status_str_list.push_back(status_str);
    }
    else if(currentMode == _MODE_SHAPE_ROI_){
        status_str = "[MODE] Shape Detection with ROI.";
        status_str_list.push_back(status_str);
    }
    else if(currentMode == _MODE_SHAPE_ONE_WAY_){
        status_str = "[MODE] Line Shape Detection.";
        status_str_list.push_back(status_str);
    }
    else{
        status_str = "[MODE] NONE.";
        status_str_list.push_back(status_str);
    }

    if(is_match_available > 0){
        status_str = "[OK] 模板樣本已準備好 Template is ready.";
        status_str_list.push_back(status_str);
    }
    else{
        status_str = "[FAIL] 模板樣本尚未準備 Template is not ready.";
        status_str_list.push_back(status_str);
    }

    for(uint32_t i = 0; i < status_str_list.size(); i++){
        log(QString::fromStdString(status_str_list.at(i)));
    }
}

void AlignSys::UpdateLightSourceState(bool state)
{
    if (isOptLightConnected != state)
        isOptLightStatusSwitch = true;
    else
        isOptLightStatusSwitch = false;
    isOptLightConnected = state;

    if (isOptLightConnected) {
        ui->lab_lightState->setStyleSheet("background-color: rgb(52, 67, 104);color: rgb(0, 255, 0);");
        ui->lab_lightState->setText("Connect");
        if (isOptLightStatusSwitch) systemSettingDialog->SetEnabledOpt(true);   // Jimmy. 20220105
    } else { // if Opt light didn't connect
        ui->lab_lightState->setStyleSheet("background-color: rgb(52, 67, 104);color: rgb(255, 0, 0);");
        ui->lab_lightState->setText("Disconnect");
        if (isOptLightStatusSwitch) systemSettingDialog->SetEnabledOpt(false);  // Jimmy. 20220105
    }
}

void AlignSys::RestartMemCheck(){   // Test function
    ActionMarkerDetect();

    while(true){
        lw_mem << "Using memory(" << std::to_string((int)getpid()) << "): " << (float)GetCurrentMem() / 1000 << lw_mem.endl;
        on_btn_calibration_clicked();
        on_btn_alignment_clicked();
    }
}

AlignSys::~AlignSys()
{
#ifdef ORIGINAL_SHAPE_ALIGN
    delete shapeSettingDialog;
#endif
    SystemLog("[END][System]");
    rslt_rstL.clear();
    rslt_rstR.clear();
    if(dm::log){ qDebug() << "[CLEAR MEM] " << __LINE__; }
    DisconnectCameras();
    if(dm::log){ qDebug() << "[CLEAR MEM] " << __LINE__; }
    mutex_shared_l_->unlock();
    mutex_shared_r_->unlock();
    delete mutex_shared_l_;
    delete mutex_shared_r_;
    if(dm::log){ qDebug() << "[CLEAR MEM] " << __LINE__; }
    StopOptThread();
    //cameraSettingDialog->DisconnectOPTDevice();
#ifdef ENABLE_AUTO_ADJUST_BRIGHTNESS
    delete adjust_bright;
    delete cameraSettingDialog;
#endif
    if(dm::log){ qDebug() << "[CLEAR MEM] " << __LINE__; }
#ifdef ENABLE_AUTO_ADJUST_BRIGHTNESS
    delete param_L;
    delete param_R;
#endif
    if(dm::log){ qDebug() << "[CLEAR MEM] " << __LINE__; }
#ifdef SPINNAKER_CAM_API
    if(spin_cam != NULL)
        delete spin_cam;
#endif
    if(dm::log){ qDebug() << "[CLEAR MEM] " << __LINE__; }
    StopPlcThread();
    if(dm::log){ qDebug() << "[CLEAR MEM] " << __LINE__; }
// Jang 20220411
    if (authorityDialog != nullptr)
        delete authorityDialog;
    //delete paramSetting ;
    if(dm::log){ qDebug() << "[CLEAR MEM] " << __LINE__; }
//    if (loginDialog != nullptr) delete loginDialog;
//    if(dm::log){ qDebug() << "[CLEAR MEM] " << __LINE__; }
    if (systemSettingDialog != nullptr) delete systemSettingDialog;
    if(dm::log){ qDebug() << "[CLEAR MEM] " << __LINE__; }
    if (imageprocSettingDialog != nullptr) delete imageprocSettingDialog;
    if(dm::log){ qDebug() << "[CLEAR MEM] " << __LINE__; }
    if (markerSettingDialog != nullptr) delete markerSettingDialog;
    if(dm::log){ qDebug() << "[CLEAR MEM] " << __LINE__; }
    if (debugDialog != nullptr) delete debugDialog;
    if(dm::log){ qDebug() << "[CLEAR MEM] " << __LINE__; }
    //delete recipeChangeDialog;
    if(dm::log){ qDebug() << "[CLEAR MEM] " << __LINE__; }
    if (msgDialog != nullptr) delete msgDialog;
    if(dm::log){ qDebug() << "[CLEAR MEM] " << __LINE__; }
    if (cropRoiDialog != nullptr) delete cropRoiDialog;
    if(dm::log){ qDebug() << "[CLEAR MEM] " << __LINE__; }
    if (customerDialog != nullptr) delete customerDialog;
    if(dm::log){ qDebug() << "[CLEAR MEM] " << __LINE__; }
    if (timer_updateCurrentTime_ != nullptr)   delete timer_updateCurrentTime_;
    if(dm::log){ qDebug() << "[CLEAR MEM] " << __LINE__; }
    // Jang 20220711
    if (delete_log_timer_ != nullptr) {
        delete_log_timer_->stop();
        delete_log_timer_->deleteLater();
        if(dm::log){ qDebug() << "[CLEAR MEM] " << __LINE__; }
    }
    StopDiskSpcMngThread();
    if(dm::log){ qDebug() << "[CLEAR MEM] " << __LINE__; }
    delete authority_;
    if(dm::log){ qDebug() << "[CLEAR MEM] " << __LINE__; }
    delete ui;

    qDebug() << ">---------- End of AlignSys ----------<\n";
}

bool AlignSys::ChecklicenseFileExist()	//garly_20220209, add check license function
{
    QString licensePath = this->root_path_ + "../"; // "/home/contrel";

    QDir tmp_dir(licensePath);
    if (!tmp_dir.exists()) {
        SystemLogfWithCodeLoc("[ERR] Can't find path : " + licensePath, __FILE__, __func__);
    } else {
        QString LicenseName = QString("%1/.bash_fmt").arg(licensePath);
        QFile checkFile(LicenseName);
        if (checkFile.exists()) {
            if (ChecklicenseContent(LicenseName)) {
                return true;
            }
        } else {
            QString old_license_path = QString("%1/.nvidia_license.ini").arg(licensePath);
            // Check old license path. Jimmy 20220718.
            QFile old_license(old_license_path);
            if (old_license.exists()) {
                if (old_license.rename(LicenseName)) {
                    if (ChecklicenseContent(LicenseName)) {
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

bool AlignSys::ChecklicenseContent(QString file_path)	//garly_20220209, add check license function
{
    // FileIO. Jimmy, 20220510. #fio32
    fio::FileIO fs(file_path.toStdString(), fio::FileioFormat::R);
    if (fs.isOK()) {
        QString data = QString::fromStdString(fs.ReadAll());
        fs.close();

        // Check data is end with "\n". Jimmy 20220627
        if (data.endsWith("\n"))
            data.remove(data.size()-1, 1);

        if (!data.isEmpty())
        {
            //qDebug() << "data = " << data;
            //qDebug() << "MAC = " << GetMacAddress();
            if (data == GetMacAddress())
            {
                //qDebug() << "license is the same!";
                return true;
            }
        }
    }
    fs.close();
    return false;
}

void AlignSys::Writelicense()
{
    QString str = GetMacAddress();
    QString licensePath = root_path_ +  "../.bash_fmt"; // "/home/contrel/.bash_fmt";

    // FileIO. Jimmy, 20220511. #fio38
    fio::FileIO fs(licensePath.toStdString(), fio::FileioFormat::W);
    fs.Write(str.toStdString());
    fs.close();

    QFile(licensePath).setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner
                                      | QFileDevice::ReadGroup | QFileDevice::WriteGroup | QFileDevice::ExeGroup
                                      | QFileDevice::ReadOther | QFileDevice::WriteOther | QFileDevice::ExeOther);
}
//*******************************************************//
//****************** Action Functions *******************//
//*******************************************************//
void AlignSys::InitializeUI()
{
    /*
     * Goal:    Initialize the buttons
     */
    //SetBtnShow();

    QFont font = ui->lbl_Mode->font();
    font.setPointSize(34);
    ui->lbl_Mode->setFont(font);
    ui->lbl_Mode->setText("IDLE");

    font = ui->label_resultStatus->font();
    font.setPointSize(34);
    ui->label_resultStatus->setFont(font);
    ResultStatus("IDLE");

    // Init. variables
    func_mode = _MODE_NONE_;

    SetPLCValues(nullptr, 0, 0, 0);
    ui->lbl_moveDeg->setText("0");
    count_calpos = 0;
    cal_pos.clear();

#ifdef RESULT_FORMAT_XYT
    ui->label->setText("X node:");
    ui->label_2->setVisible(false);
    ui->lbl_moveX2->setVisible(false);
    ui->label_14->setVisible(false);
#endif
}

void AlignSys::LoginPasswordFinish()
{
    loginDialogIsShown = false;
    SetMainMenuBtnEnable(true);
    //qDebug() << "Write license";
    Writelicense();
}

void AlignSys::SetSystemSetting(SystemSetting sys_set){
    this->sys_set = sys_set;
    this->system_type = sys_set.system_type;
    this->machine_type = sys_set.machine_type ;
    this->calibration_mode = sys_set.calibration_mode ;
    this->system_unit = sys_set.system_unit ;
    align.SetSystemMode(this->machine_type, this->calibration_mode, this->system_unit);
    // Update the UI to show exposure time. Jimmy 20221103
    ui->lbl_exp_time_1->setText(QString("曝光時間 Exp. Time: %1").arg(this->sys_set.cam_exposure));
    ui->lbl_exp_time_2->setText(QString("曝光時間 Exp. Time: %1").arg(this->sys_set.cam_exposure));

    SaveSystemSetting();
}

void AlignSys::LoadDefaultSystemSetting(int machine){
    std::string file_path = root_path_.toStdString() + "/Data/default_system_parameters.ini";
    std::string machine_name = "";
    if(machine == _MACHINE_YT_){
        machine_name = "SystemSettingYT";
    }
    else if(machine == _MACHINE_UVW_){
        machine_name = "SystemSettingUVW";
    }

    SystemSetting sys_set_default;
    LoadSystemSettingINI(sys_set_default, file_path, machine_name);
    paramSetting->SetSystemSetting(sys_set_default);
}

bool AlignSys::SaveSystemSetting(){
    std::string file_path = system_config_path_.toStdString() + "system_setting.txt";

    return SaveSystemSetting(this->sys_set, file_path);
}

bool AlignSys::SaveSystemSetting(const SystemSetting& sys_set, const std::string& file_path){

    std::string dir_path = file_path;

    for(int i = file_path.size()-1; i > 0; i--){
        if(file_path.at(i) == '/'){
            dir_path = file_path.substr(0, i);
            break;
        }
    }

    if(dm::log){
        qDebug() << "dir_path: " << QString::fromStdString(dir_path);
    }

    if(!fs::exists(dir_path))   fs::create_directories(dir_path);
    fio::FileIO write_file(file_path, fio::FileioFormat::W);    // FileIO. Jimmy. 20220418. #fio04

    if(!write_file.isOK()){
        if (dm::log) std::cerr << "File:system_setting.txt isn't opened." << std::endl;
        return false;
    }

    write_file.Write( "system_type", std::to_string(sys_set.system_type));
    write_file.Write( "machine_type", std::to_string(sys_set.machine_type));
    write_file.Write( "align_type", std::to_string(sys_set.align_type));
    write_file.Write( "calibration_mode", std::to_string(sys_set.calibration_mode));
    write_file.Write( "system_unit", std::to_string(sys_set.system_unit));

    write_file.Write( "calib_distance", std::to_string(sys_set.calib_distance));
    write_file.Write( "calib_rot_degree", std::to_string(sys_set.calib_rot_degree));
    write_file.Write( "align_repeat", std::to_string(sys_set.align_repeat));
    write_file.Write( "tolerance", std::to_string(sys_set.tolerance));
    write_file.Write( "cam_exposure", std::to_string(sys_set.cam_exposure));
    write_file.Write( "cam_gain", std::to_string(sys_set.cam_gain));
    write_file.Write( "tolerance_unit", sys_set.tolerance_unit);
    write_file.Write( "calib_dist_diff_tolerance", std::to_string(sys_set.calib_dist_diff_tolerance));
    write_file.Write( "align_distance_tol", std::to_string(sys_set.align_distance_tol));

    write_file.Write( "camera_l_idx", std::to_string(sys_set.camera_l_idx));
    write_file.Write( "camera_r_idx", std::to_string(sys_set.camera_r_idx));
    write_file.Write( "cam_resolution_w", std::to_string(sys_set.cam_resolution_w));
    write_file.Write( "cam_resolution_h", std::to_string(sys_set.cam_resolution_h));
    write_file.Write( "cam_rot_l", std::to_string(sys_set.cam_rot_l));
    write_file.Write( "cam_rot_r", std::to_string(sys_set.cam_rot_r));
    write_file.Write( "cam_flip_l", std::to_string(sys_set.cam_flip_l));
    write_file.Write( "cam_flip_r", std::to_string(sys_set.cam_flip_r));

    write_file.Write( "do_crop_roi_0", std::to_string(sys_set.crop_l.do_crop_roi));
    write_file.Write( "do_crop_roi_1", std::to_string(sys_set.crop_r.do_crop_roi));
    write_file.Write( "crop_x_0", std::to_string(sys_set.crop_l.crop_x));
    write_file.Write( "crop_y_0", std::to_string(sys_set.crop_l.crop_y));
    write_file.Write( "crop_w_0", std::to_string(sys_set.crop_l.crop_w));
    write_file.Write( "crop_h_0", std::to_string(sys_set.crop_l.crop_h));
    write_file.Write( "crop_x_1", std::to_string(sys_set.crop_r.crop_x));
    write_file.Write( "crop_y_1", std::to_string(sys_set.crop_r.crop_y));
    write_file.Write( "crop_w_1", std::to_string(sys_set.crop_r.crop_w));
    write_file.Write( "crop_h_1", std::to_string(sys_set.crop_r.crop_h));

    write_file.Write( "opt_light_IP", sys_set.opt_light_IP); // "192.168.1.16" "192.168.1.17"
    write_file.Write( "opt_light_port", std::to_string(sys_set.opt_light_port));   //8000

    write_file.Write( "use_cuda_for_image_proc", std::to_string(sys_set.use_cuda_for_image_proc));
    write_file.Write( "lightsource_value", std::to_string(sys_set.lightsource_value));

    write_file.Write( "enable_mask", std::to_string(sys_set.enable_mask));

    write_file.Write( "cam_title_swap", std::to_string(sys_set.cam_title_swap));    //garly_20230206, cam title swap

#ifdef SYNC_TIME_FROM_PLC
    write_file.Write( "sync_time_mode", std::to_string(sys_set.sync_time_mode));
    write_file.Write( "sync_time_unit", std::to_string(sys_set.sync_time_unit));
#endif

    write_file.close();

    return true;
}

void AlignSys::SetDefaultSystemSetting()
{

    //sys_set_default_uvw;    // No need to be changed

    sys_set_default_yt.cam_rot_l = -90;
    sys_set_default_yt.cam_rot_r = 90;
    sys_set_default_yt.cam_resolution_w = 1280;
    sys_set_default_yt.cam_resolution_h = 1024;
    sys_set_default_yt.system_type = _SYSTEM_COARSE_ALIGN_;
    sys_set_default_yt.machine_type = _MACHINE_TYPE_::_MACHINE_YT_;
    sys_set_default_yt.align_type = _ALIGN_TYPE_::_SKIP_ALIGN_SEND_OFFSET_;
    sys_set_default_yt.calibration_mode = _MODE_CALIB_YT_;
    sys_set_default_yt.system_unit = _UNIT_MM_DEG_;
    sys_set_default_yt.camera_l_idx = 1;
    sys_set_default_yt.camera_r_idx = 0;
    sys_set_default_yt.cam_flip_l = false;
    sys_set_default_yt.cam_flip_r = false;
    sys_set_default_yt.lightsource_value = 200;
    sys_set_default_yt.cam_title_swap = false;//garly_20230206, cam title swap
#ifdef SYNC_TIME_FROM_PLC
    sys_set_default_yt.sync_time_mode = true;
    sys_set_default_yt.sync_time_unit = 5;
#endif
    sys_set_default_yt.use_cuda_for_image_proc = false;
    sys_set_default_yt.enable_mask = false;
    sys_set_default_yt.align_repeat = 3;
    sys_set_default_yt.tolerance = 0.1;
    sys_set_default_yt.calib_distance = 4.5;
    sys_set_default_yt.calib_rot_degree = 1.5;
    sys_set_default_yt.cam_exposure = 1500;
    sys_set_default_yt.cam_gain = 0;
    sys_set_default_yt.calib_dist_diff_tolerance = 10;
    sys_set_default_yt.tolerance_unit = "mm";
    sys_set_default_yt.opt_light_IP = "192.168.1.16";
    sys_set_default_yt.opt_light_port = 8000;
    sys_set_default_yt.opt_light_sn = "None";
    sys_set_default_yt.crop_l.do_crop_roi = true;
    sys_set_default_yt.crop_r.do_crop_roi = true;

}

bool AlignSys::SaveDefaultParamsINI(const std::string& path){
    SetDefaultSystemSetting();
    if(!fs::is_directory(path) && fs::exists(path)){
        fs::remove(path);
    }
    fio::FileIO inim(path, fio::FileioFormat::INI);    // FileIO. Jimmy. 20220418. #fio26
    if(!inim.isOK()){
        if (dm::log) std::cerr << "File: default_system_parameters.txt isn't opened." << std::endl;
        SystemLogfWithCodeLoc("[ERR] File failed to open.", __FILE__, __func__);
        return false;
    }

    try{
        inim.IniSetSection("BrightnessCamLYT");
        inim.IniInsert("brightness_save", std::to_string(200));
        inim.IniSetSection("BrightnessCamRYT");
        inim.IniInsert("brightness_save", std::to_string(200));
        inim.IniSetSection("BrightnessCamLUVW");
        inim.IniInsert("brightness_save", std::to_string(30));
        inim.IniSetSection("BrightnessCamRUVW");
        inim.IniInsert("brightness_save", std::to_string(30));

        inim.IniSetSection("ImgProcParams");
        inim.IniInsert("threshold_value", std::to_string(0));
        inim.IniInsert("threshold_type", std::to_string(0));
        inim.IniInsert("blur_type", std::to_string(0));
        inim.IniInsert("blur_size", std::to_string(1));
        inim.IniInsert("blur_amp", std::to_string(1));
        inim.IniInsert("canny_th1", std::to_string(160));
        inim.IniInsert("canny_th2", std::to_string(160));
        inim.IniInsert("clahe_of_off", std::to_string(0));
        inim.IniInsert("clahe_size", std::to_string(0));
        inim.IniInsert("clahe_limit", std::to_string(0));
        inim.IniInsert("image_width", std::to_string(1280));
        inim.IniInsert("image_height", std::to_string(1024));
        inim.IniInsert("percent_contours", std::to_string(98));
        inim.IniInsert("max_percent_contours", std::to_string(100));

        inim.IniSetSection("PLCSettingYT");
        inim.IniInsert("IP", "192.168.1.200");
        inim.IniInsert("Port", "5031");
        inim.IniInsert("PLCTriggerAddr", "D15000");
        inim.IniInsert("CCDTriggerAddr", "D15002");
        inim.IniInsert("CommandAddr", "D15004");
        inim.IniInsert("ResultAddr", "D15108");
        inim.IniInsert("RecipeNumRAddr", "D15034");
        inim.IniInsert("RecipeNumWAddr", "D15134");
        inim.IniInsert("PLCFormat", std::to_string(0));

        inim.IniSetSection("PLCSettingUVW");
        inim.IniInsert("IP", "192.168.1.200");
        inim.IniInsert("Port", "5032");
        inim.IniInsert("PLCTriggerAddr", "D15200");
        inim.IniInsert("CCDTriggerAddr", "D15202");
        inim.IniInsert("CommandAddr", "D15204");
        inim.IniInsert("ResultAddr", "D15308");
        inim.IniInsert("RecipeNumRAddr", "D15234");
        inim.IniInsert("RecipeNumWAddr", "D15334");
        inim.IniInsert("PLCFormat", std::to_string(0));


        inim.IniSetSection("SystemSettingYT");
        inim.IniInsert("system_type", std::to_string(sys_set_default_yt.system_type));
        inim.IniInsert("machine_type", std::to_string(sys_set_default_yt.machine_type));
        inim.IniInsert("align_type", std::to_string(sys_set_default_yt.align_type));
        inim.IniInsert("calibration_mode", std::to_string(sys_set_default_yt.calibration_mode));
        inim.IniInsert("system_unit", std::to_string(sys_set_default_yt.system_unit));
        inim.IniInsert("calib_distance", std::to_string(sys_set_default_yt.calib_distance));
        inim.IniInsert("calib_rot_degree", std::to_string(sys_set_default_yt.calib_rot_degree));
        inim.IniInsert("align_repeat", std::to_string(sys_set_default_yt.align_repeat));
        inim.IniInsert("tolerance", std::to_string(sys_set_default_yt.tolerance));
        inim.IniInsert("cam_exposure", std::to_string(sys_set_default_yt.cam_exposure));
        inim.IniInsert("cam_gain", std::to_string(sys_set_default_yt.cam_gain));
        inim.IniInsert("tolerance_unit", sys_set_default_yt.tolerance_unit);
        inim.IniInsert("calib_dist_diff_tolerance", std::to_string(sys_set_default_yt.calib_dist_diff_tolerance));
        inim.IniInsert("camera_l_idx", std::to_string(sys_set_default_yt.camera_l_idx));
        inim.IniInsert("camera_r_idx", std::to_string(sys_set_default_yt.camera_r_idx));
        inim.IniInsert("cam_resolution_w", std::to_string(sys_set_default_yt.cam_resolution_w));
        inim.IniInsert("cam_resolution_h", std::to_string(sys_set_default_yt.cam_resolution_h));
        inim.IniInsert("cam_rot_l", std::to_string(sys_set_default_yt.cam_rot_l));
        inim.IniInsert("cam_rot_r", std::to_string(sys_set_default_yt.cam_rot_r));
        inim.IniInsert("cam_flip_l", std::to_string(sys_set_default_yt.cam_flip_l));
        inim.IniInsert("cam_flip_r", std::to_string(sys_set_default_yt.cam_flip_r));
        inim.IniInsert("do_crop_roi_0", std::to_string(sys_set_default_yt.crop_l.do_crop_roi));
        inim.IniInsert("do_crop_roi_1", std::to_string(sys_set_default_yt.crop_r.do_crop_roi));
        inim.IniInsert("crop_x_0", std::to_string(sys_set_default_yt.crop_l.crop_x));
        inim.IniInsert("crop_y_0", std::to_string(sys_set_default_yt.crop_l.crop_y));
        inim.IniInsert("crop_w_0", std::to_string(sys_set_default_yt.crop_l.crop_w));
        inim.IniInsert("crop_h_0", std::to_string(sys_set_default_yt.crop_l.crop_h));
        inim.IniInsert("crop_x_1", std::to_string(sys_set_default_yt.crop_r.crop_x));
        inim.IniInsert("crop_y_1", std::to_string(sys_set_default_yt.crop_r.crop_y));
        inim.IniInsert("crop_w_1", std::to_string(sys_set_default_yt.crop_r.crop_w));
        inim.IniInsert("crop_h_1", std::to_string(sys_set_default_yt.crop_r.crop_h));
        inim.IniInsert("opt_light_IP", sys_set_default_yt.opt_light_IP); // "192.168.1.16" "192.168.1.17"
        inim.IniInsert("opt_light_port", std::to_string(sys_set_default_yt.opt_light_port));   //8000
        inim.IniInsert("use_cuda_for_image_proc", std::to_string(sys_set_default_yt.use_cuda_for_image_proc));
        inim.IniInsert("lightsource_value", std::to_string(sys_set_default_yt.lightsource_value));
        inim.IniInsert("enable_mask", std::to_string(sys_set_default_yt.enable_mask));
        inim.IniInsert("cam_title_swap", std::to_string(sys_set_default_yt.cam_title_swap));//garly_20230206, cam title swap
    #ifdef SYNC_TIME_FROM_PLC
        inim.IniInsert("sync_time_mode", std::to_string(sys_set_default_uvw.sync_time_mode));
        inim.IniInsert("sync_time_unit", std::to_string(sys_set_default_uvw.sync_time_unit));
    #endif

        inim.IniSetSection("SystemSettingUVW");
        inim.IniInsert("system_type", std::to_string(sys_set_default_uvw.system_type));
        inim.IniInsert("machine_type", std::to_string(sys_set_default_uvw.machine_type));
        inim.IniInsert("align_type", std::to_string(sys_set_default_uvw.align_type));
        inim.IniInsert("calibration_mode", std::to_string(sys_set_default_uvw.calibration_mode));
        inim.IniInsert("system_unit", std::to_string(sys_set_default_uvw.system_unit));
        inim.IniInsert("calib_distance", std::to_string(sys_set_default_uvw.calib_distance));
        inim.IniInsert("calib_rot_degree", std::to_string(sys_set_default_uvw.calib_rot_degree));
        inim.IniInsert("align_repeat", std::to_string(sys_set_default_uvw.align_repeat));
        inim.IniInsert("tolerance", std::to_string(sys_set_default_uvw.tolerance));
        inim.IniInsert("cam_exposure", std::to_string(sys_set_default_uvw.cam_exposure));
        inim.IniInsert("cam_gain", std::to_string(sys_set_default_uvw.cam_gain));
        inim.IniInsert("tolerance_unit", sys_set_default_uvw.tolerance_unit);
        inim.IniInsert("calib_dist_diff_tolerance", std::to_string(sys_set_default_uvw.calib_dist_diff_tolerance));
        inim.IniInsert("camera_l_idx", std::to_string(sys_set_default_uvw.camera_l_idx));
        inim.IniInsert("camera_r_idx", std::to_string(sys_set_default_uvw.camera_r_idx));
        inim.IniInsert("cam_resolution_w", std::to_string(sys_set_default_uvw.cam_resolution_w));
        inim.IniInsert("cam_resolution_h", std::to_string(sys_set_default_uvw.cam_resolution_h));
        inim.IniInsert("cam_rot_l", std::to_string(sys_set_default_uvw.cam_rot_l));
        inim.IniInsert("cam_rot_r", std::to_string(sys_set_default_uvw.cam_rot_r));
        inim.IniInsert("cam_flip_l", std::to_string(sys_set_default_uvw.cam_flip_l));
        inim.IniInsert("cam_flip_r", std::to_string(sys_set_default_uvw.cam_flip_r));
        inim.IniInsert("do_crop_roi_0", std::to_string(sys_set_default_uvw.crop_l.do_crop_roi));
        inim.IniInsert("do_crop_roi_1", std::to_string(sys_set_default_uvw.crop_r.do_crop_roi));
        inim.IniInsert("crop_x_0", std::to_string(sys_set_default_uvw.crop_l.crop_x));
        inim.IniInsert("crop_y_0", std::to_string(sys_set_default_uvw.crop_l.crop_y));
        inim.IniInsert("crop_w_0", std::to_string(sys_set_default_uvw.crop_l.crop_w));
        inim.IniInsert("crop_h_0", std::to_string(sys_set_default_uvw.crop_l.crop_h));
        inim.IniInsert("crop_x_1", std::to_string(sys_set_default_uvw.crop_r.crop_x));
        inim.IniInsert("crop_y_1", std::to_string(sys_set_default_uvw.crop_r.crop_y));
        inim.IniInsert("crop_w_1", std::to_string(sys_set_default_uvw.crop_r.crop_w));
        inim.IniInsert("crop_h_1", std::to_string(sys_set_default_uvw.crop_r.crop_h));
        inim.IniInsert("opt_light_IP", sys_set_default_uvw.opt_light_IP); // "192.168.1.16" "192.168.1.17"
        inim.IniInsert("opt_light_port", std::to_string(sys_set_default_uvw.opt_light_port));   //8000
        inim.IniInsert("use_cuda_for_image_proc", std::to_string(sys_set_default_uvw.use_cuda_for_image_proc));
        inim.IniInsert("lightsource_value", std::to_string(sys_set_default_uvw.lightsource_value));
        inim.IniInsert("enable_mask", std::to_string(sys_set_default_uvw.enable_mask));
        inim.IniInsert("cam_title_swap", std::to_string(sys_set_default_uvw.cam_title_swap));//garly_20230206, cam title swap
    #ifdef SYNC_TIME_FROM_PLC
        inim.IniInsert("sync_time_mode", std::to_string(sys_set_default_uvw.sync_time_mode));
        inim.IniInsert("sync_time_unit", std::to_string(sys_set_default_uvw.sync_time_unit));
    #endif

        inim.IniSave();
        inim.close();
    }
    catch(std::exception e){
        SystemLogfWithCodeLoc("[WARN] Error to save default_system_parameters.ini file.",
                              __FILE__, __func__);
        lw << "[ERR] Error: " << e.what() << lw.endl;
        return false;
    }
    return true;
}

bool AlignSys::LoadSystemSetting() {
    std::string file_path = system_config_path_.toStdString() + "/system_setting.txt";

    if(!LoadSystemSetting(this->sys_set, file_path)){
        SaveSystemSetting();
    }
    return true;
}

bool AlignSys::LoadSystemSetting(SystemSetting& sys_set, const std::string& file_path){

    fio::FileIO get_file(file_path, fio::FileioFormat::R);  // FileIO. Jimmy. 20220418. #fio05
    if(!get_file.isOK()){
        if (dm::log) qDebug() << "File:" << QString::fromStdString(file_path) << "isn't opened.";
        if(!SaveSystemSetting()){
            return false;
        }
    }

    sys_set.use_cuda_for_image_proc     = get_file.ReadtoInt("use_cuda_for_image_proc", sys_set.use_cuda_for_image_proc);
    sys_set.enable_mask                 = get_file.ReadtoInt("enable_mask", sys_set.enable_mask);
    sys_set.crop_l.do_crop_roi          = get_file.ReadtoInt("do_crop_roi_0", sys_set.crop_l.do_crop_roi);
    sys_set.crop_r.do_crop_roi          = get_file.ReadtoInt("do_crop_roi_1", sys_set.crop_r.do_crop_roi);
    sys_set.cam_flip_l                  = get_file.ReadtoInt("cam_flip_l", sys_set.cam_flip_l);
    sys_set.cam_flip_r                  = get_file.ReadtoInt("cam_flip_r", sys_set.cam_flip_r);

    sys_set.cam_rot_l                   = get_file.ReadtoInt("cam_rot_l", sys_set.cam_rot_l);
    sys_set.cam_rot_r                   = get_file.ReadtoInt("cam_rot_r", sys_set.cam_rot_r);
    sys_set.cam_resolution_w            = get_file.ReadtoInt("cam_resolution_w", sys_set.cam_resolution_w);
    sys_set.cam_resolution_h            = get_file.ReadtoInt("cam_resolution_h", sys_set.cam_resolution_h);
    sys_set.camera_l_idx                = get_file.ReadtoInt("camera_l_idx", sys_set.camera_l_idx);
    sys_set.camera_r_idx                = get_file.ReadtoInt("camera_r_idx", sys_set.camera_r_idx);
    sys_set.lightsource_value           = get_file.ReadtoInt("lightsource_value", sys_set.lightsource_value);
    sys_set.cam_title_swap              = get_file.ReadtoInt("cam_title_swap", sys_set.cam_title_swap);//garly_20230206, cam title swap
#ifdef SYNC_TIME_FROM_PLC
    sys_set.sync_time_mode              = get_file.ReadtoInt("sync_time_mode", sys_set.sync_time_mode);
    sys_set.sync_time_unit              = get_file.ReadtoInt("sync_time_unit", sys_set.sync_time_unit);
#endif
    sys_set.system_type                 = get_file.ReadtoInt("system_type", sys_set.system_type);
    sys_set.machine_type                = get_file.ReadtoInt("machine_type", sys_set.machine_type);
    sys_set.align_type                  = get_file.ReadtoInt("align_type", sys_set.align_type);
    sys_set.calibration_mode            = get_file.ReadtoInt("calibration_mode", sys_set.calibration_mode);
    sys_set.system_unit                 = get_file.ReadtoInt("system_unit", sys_set.system_unit);
    sys_set.calib_dist_diff_tolerance   = get_file.ReadtoInt("calib_dist_diff_tolerance", sys_set.calib_dist_diff_tolerance);
    sys_set.align_repeat                = get_file.ReadtoInt("align_repeat", sys_set.align_repeat);

    sys_set.crop_l.crop_x               = get_file.ReadtoInt("crop_x_0", sys_set.crop_l.crop_x);
    sys_set.crop_l.crop_y               = get_file.ReadtoInt("crop_y_0", sys_set.crop_l.crop_y);
    sys_set.crop_l.crop_w               = get_file.ReadtoInt("crop_w_0", sys_set.crop_l.crop_w);
    sys_set.crop_l.crop_h               = get_file.ReadtoInt("crop_h_0", sys_set.crop_l.crop_h);
    sys_set.crop_r.crop_x               = get_file.ReadtoInt("crop_x_1", sys_set.crop_r.crop_x);
    sys_set.crop_r.crop_y               = get_file.ReadtoInt("crop_y_1", sys_set.crop_r.crop_y);
    sys_set.crop_r.crop_w               = get_file.ReadtoInt("crop_w_1", sys_set.crop_r.crop_w);
    sys_set.crop_r.crop_h               = get_file.ReadtoInt("crop_h_1", sys_set.crop_r.crop_h);

    sys_set.tolerance           = get_file.ReadtoFloat("tolerance", sys_set.tolerance);
    sys_set.calib_distance      = get_file.ReadtoFloat("calib_distance", sys_set.calib_distance);
    sys_set.calib_rot_degree    = get_file.ReadtoFloat("calib_rot_degree", sys_set.calib_rot_degree);
    sys_set.cam_exposure        = get_file.ReadtoFloat("cam_exposure", sys_set.cam_exposure);
    sys_set.cam_gain            = get_file.ReadtoFloat("cam_gain", sys_set.cam_gain);

    sys_set.align_distance_tol  = get_file.ReadtoDouble("align_distance_tol", sys_set.align_distance_tol);

    std::string found_data;
    found_data = get_file.Read("tolerance_unit");
    if(get_file.isOK()) sys_set.tolerance_unit = found_data;
    found_data = get_file.Read("opt_light_IP");
    if(get_file.isOK()) sys_set.opt_light_IP = found_data;

    sys_set.opt_light_port  = get_file.ReadtoInt("opt_light_port", sys_set.opt_light_port);

    get_file.close();

    this->system_type = sys_set.system_type;
    this->machine_type = sys_set.machine_type ;
    this->calibration_mode = sys_set.calibration_mode ;
    this->system_unit = sys_set.system_unit ;

    if (!get_file.isAllOK()) {
        if (dm::log) qDebug() << "func: LoadSystemSetting: Parameter loading has failed. Count: " << get_file.GetErrorNum();
        get_file.clear();
        return false;
    }
    return true;
}

bool AlignSys::LoadSystemSettingINI(SystemSetting& sys_set, const std::string& file_path, const std::string& section_name)
{
    fio::FileIO get_file(file_path, fio::FileioFormat::INI); // FileIO. 20220419. Jimmy.    #fio06

    if(!get_file.isOK()){
        if (dm::log) qDebug() << "File: " << QString::fromStdString(file_path) << "isn't opened.";
        // DEPRECATED by Jimmy. 20220419 => No use.
        //if(!SaveSystemSetting()){
        //    return false;
        //}
    }
    std::string found_data;

    std::string section = section_name;

    get_file.IniSetSection(section);
    sys_set.use_cuda_for_image_proc     = get_file.IniReadtoInt("use_cuda_for_image_proc",      sys_set.use_cuda_for_image_proc  );
    sys_set.enable_mask                 = get_file.IniReadtoInt("enable_mask",                  sys_set.enable_mask              );
    sys_set.crop_l.do_crop_roi          = get_file.IniReadtoInt("do_crop_roi_0",                sys_set.crop_l.do_crop_roi       );
    sys_set.crop_r.do_crop_roi          = get_file.IniReadtoInt("do_crop_roi_1",                sys_set.crop_r.do_crop_roi       );
    sys_set.cam_flip_l                  = get_file.IniReadtoInt("cam_flip_l",                   sys_set.cam_flip_l               );
    sys_set.cam_flip_r                  = get_file.IniReadtoInt("cam_flip_r",                   sys_set.cam_flip_r               );
    sys_set.cam_rot_l                   = get_file.IniReadtoInt("cam_rot_l",                    sys_set.cam_rot_l                );
    sys_set.cam_rot_r                   = get_file.IniReadtoInt("cam_rot_r",                    sys_set.cam_rot_r                );
    sys_set.cam_resolution_w            = get_file.IniReadtoInt("cam_resolution_w",             sys_set.cam_resolution_w         );
    sys_set.cam_resolution_h            = get_file.IniReadtoInt("cam_resolution_h",             sys_set.cam_resolution_h         );
    sys_set.camera_l_idx                = get_file.IniReadtoInt("camera_l_idx",                 sys_set.camera_l_idx             );
    sys_set.camera_r_idx                = get_file.IniReadtoInt("camera_r_idx",                 sys_set.camera_r_idx             );
    sys_set.lightsource_value           = get_file.IniReadtoInt("lightsource_value",            sys_set.lightsource_value        );
    sys_set.cam_title_swap              = get_file.IniReadtoInt("cam_title_swap",               sys_set.cam_title_swap           );//garly_20230206, cam title swap
#ifdef SYNC_TIME_FROM_PLC
    sys_set.sync_time_mode              = get_file.IniReadtoInt("sync_time_mode",               sys_set.sync_time_mode           );
    sys_set.sync_time_unit              = get_file.IniReadtoInt("sync_time_unit",               sys_set.sync_time_unit           );
#endif
    sys_set.system_type                 = get_file.IniReadtoInt("system_type",                  sys_set.system_type              );
    sys_set.machine_type                = get_file.IniReadtoInt("machine_type",                 sys_set.machine_type             );
    sys_set.align_type                  = get_file.IniReadtoInt("align_type",                   sys_set.align_type               );
    sys_set.calibration_mode            = get_file.IniReadtoInt("calibration_mode",             sys_set.calibration_mode         );
    sys_set.system_unit                 = get_file.IniReadtoInt("system_unit",                  sys_set.system_unit              );
    sys_set.align_repeat                = get_file.IniReadtoInt("align_repeat",                 sys_set.align_repeat             );

    sys_set.crop_l.crop_x               = get_file.IniReadtoInt("crop_x_0",                     sys_set.crop_l.crop_x            );
    sys_set.crop_l.crop_y               = get_file.IniReadtoInt("crop_y_0",                     sys_set.crop_l.crop_y            );
    sys_set.crop_l.crop_w               = get_file.IniReadtoInt("crop_w_0",                     sys_set.crop_l.crop_w            );
    sys_set.crop_l.crop_h               = get_file.IniReadtoInt("crop_h_0",                     sys_set.crop_l.crop_h            );
    sys_set.crop_r.crop_x               = get_file.IniReadtoInt("crop_x_1",                     sys_set.crop_r.crop_x            );
    sys_set.crop_r.crop_y               = get_file.IniReadtoInt("crop_y_1",                     sys_set.crop_r.crop_y            );
    sys_set.crop_r.crop_w               = get_file.IniReadtoInt("crop_w_1",                     sys_set.crop_r.crop_w            );
    sys_set.crop_r.crop_h               = get_file.IniReadtoInt("crop_h_1",                     sys_set.crop_r.crop_h            );

    sys_set.tolerance                   = get_file.IniReadtoFloat("tolerance",                  sys_set.tolerance                );
    sys_set.calib_distance              = get_file.IniReadtoFloat("calib_distance",             sys_set.calib_distance           );
    sys_set.calib_rot_degree            = get_file.IniReadtoFloat("calib_rot_degree",           sys_set.calib_rot_degree         );
    sys_set.cam_exposure                = get_file.IniReadtoFloat("cam_exposure",               sys_set.cam_exposure             );
    sys_set.cam_gain                    = get_file.IniReadtoFloat("cam_gain",                   sys_set.cam_gain                 );
    sys_set.calib_dist_diff_tolerance   = get_file.IniReadtoFloat("calib_dist_diff_tolerance",  sys_set.calib_dist_diff_tolerance);
                                          // **NEW ADD** need to add at default file

    found_data = get_file.IniRead("tolerance_unit");
    if(get_file.isOK()) sys_set.tolerance_unit = found_data;
    found_data = get_file.IniRead("opt_light_IP");
    if(get_file.isOK()) sys_set.opt_light_IP = found_data;
    sys_set.opt_light_port              = get_file.IniReadtoInt("opt_light_port",               sys_set.opt_light_port           );

    ////////////////////////////////////////////

    if (section_name == "SystemSettingYT") {
        // Set [BrightnessCam] as default values
        opt_brightness_val_L_   = get_file.IniReadtoInt("BrightnessCamLYT", "brightness_save");
        if(!get_file.isOK()) opt_brightness_val_L_ = 200;
#ifdef ENABLE_AUTO_ADJUST_BRIGHTNESS
        param_L->brightness_save = get_file.IniReadtoInt("BrightnessCamLYT", "brightness_save");
#endif
        opt_brightness_val_R_   = get_file.IniReadtoInt("BrightnessCamRYT", "brightness_save");
        if(!get_file.isOK()) opt_brightness_val_R_ = 200;
#ifdef ENABLE_AUTO_ADJUST_BRIGHTNESS
        param_R->brightness_save = get_file.IniReadtoInt("BrightnessCamRYT", "brightness_save");
#endif
        systemSettingDialog->SetBrightnessVal(opt_brightness_val_L_, opt_brightness_val_R_);
#ifdef ENABLE_AUTO_ADJUST_BRIGHTNESS
        cameraSettingDialog->SetCameraSettingParams(param_L, param_R);
        systemSettingDialog->SetCameraSettingParams(param_L, param_R);
        cameraSettingDialog->SaveBrightnessInfo();
#endif
        // Set [PLCSetting] as default values
        emit signalPlcLoadDefaultSetting(file_path, "PLCSettingYT");
        emit signalPlcWriteSetting();

    } else if (section_name == "SystemSettingUVW") {
        // Set [BrightnessCam] as default values
        opt_brightness_val_L_   = get_file.IniReadtoInt("BrightnessCamLUVW", "brightness_save");
        if(!get_file.isOK()) opt_brightness_val_L_ = 30;
#ifdef ENABLE_AUTO_ADJUST_BRIGHTNESS
        param_L->brightness_save = get_file.IniReadtoInt("BrightnessCamLUVW", "brightness_save");
#endif
        opt_brightness_val_R_   = get_file.IniReadtoInt("BrightnessCamRUVW", "brightness_save");
        if(!get_file.isOK()) opt_brightness_val_R_ = 30;
#ifdef ENABLE_AUTO_ADJUST_BRIGHTNESS
        param_R->brightness_save = get_file.IniReadtoInt("BrightnessCamRUVW", "brightness_save");
#endif
        systemSettingDialog->SetBrightnessVal(opt_brightness_val_L_, opt_brightness_val_R_);
#ifdef ENABLE_AUTO_ADJUST_BRIGHTNESS
        cameraSettingDialog->SetCameraSettingParams(param_L, param_R);
        systemSettingDialog->SetCameraSettingParams(param_L, param_R);
        cameraSettingDialog->SaveBrightnessInfo();
#endif
        // Set [PLCSetting] as default values
        emit signalPlcLoadDefaultSetting(file_path, "PLCSettingUVW");
        emit signalPlcWriteSetting();
    }

    // Set [ImgProcParams] as default values
    ImageParams ip;
    // FileIO. 20220420. Jimmy. #fio23
    get_file.IniSetSection("ImgProcParams");
    ip.threshold_value      = get_file.IniReadtoInt("threshold_value",      ip.threshold_value      );
    ip.threshold_type       = get_file.IniReadtoInt("threshold_type",       ip.threshold_type       );
    ip.blur_type            = get_file.IniReadtoInt("blur_type",            ip.blur_type            );
    ip.blur_size            = get_file.IniReadtoInt("blur_size",            ip.blur_size            );
    ip.blur_amp             = get_file.IniReadtoInt("blur_amp",             ip.blur_amp             );
    ip.canny_th1            = get_file.IniReadtoInt("canny_th1",            ip.canny_th1            );
    ip.canny_th2            = get_file.IniReadtoInt("canny_th2",            ip.canny_th2            );
    ip.clahe_of_off         = get_file.IniReadtoInt("clahe_of_off",         ip.clahe_of_off         );
    ip.clahe_size           = get_file.IniReadtoInt("clahe_size",           ip.clahe_size           );
    ip.clahe_limit          = get_file.IniReadtoInt("clahe_limit",          ip.clahe_limit          );
    ip.image_width          = get_file.IniReadtoInt("image_width",          ip.image_width          );
    ip.image_height         = get_file.IniReadtoInt("image_height",         ip.image_height         );
    ip.percent_contours     = get_file.IniReadtoInt("percent_contours",     ip.percent_contours     );
    ip.max_percent_contours = get_file.IniReadtoInt("max_percent_contours", ip.max_percent_contours );
    ip.dilate_erode_size    = get_file.IniReadtoInt("dilate_erode_size",    ip.dilate_erode_size    ); // Jang 20220628
    ip.downsample_step      = get_file.IniReadtoInt("downsample_step",      ip.downsample_step      ); // Jang 20220629
    ip.reduce_param_scale   = get_file.IniReadtoInt("reduce_param_scale",   ip.reduce_param_scale   ); // Jang 20220629
    ip.blur_canny           = get_file.IniReadtoInt("blur_canny",           ip.blur_canny           ); // Jang 20220712


    shape_align_0.GetImageProc()->SaveParams(ip);
    shape_align_1.GetImageProc()->SaveParams(ip);

    get_file.close();

    this->system_type = sys_set.system_type;
    this->machine_type = sys_set.machine_type ;
    this->calibration_mode = sys_set.calibration_mode ;
    this->system_unit = sys_set.system_unit ;

    if (!get_file.isAllOK()) {
        if (dm::log) qDebug() << "func: LoadSystemSettingINI: Parameter loading has failed. Count: " << get_file.GetErrorNum();
        get_file.clear();
        return false;
    }
    return true;
}

void AlignSys::UpdateMarkerSetting(){

    double cal_dist = markerSettingDialog->GetDistanceCal();
    double cal_angle = markerSettingDialog->GetAngCal();
    double align_tolerance = markerSettingDialog->GetTol();
    int align_repeat = markerSettingDialog->GetAlignRepeat();
    // Jang 20220406
    bool enable_mask = markerSettingDialog->GetEnableMask();
    // Jang 20220414

    int calib_dist_tolerance =  markerSettingDialog->GetCalibDistTolerance();
    // Jimmy 20221124
    double align_distance_tol = markerSettingDialog->GetAlignDistanceTol();

    QString warn_msg = "異常輸入 Invalid values were entered in \n";
    bool warn_flag = false;
    if(cal_dist > 0){
        sys_set.calib_distance = cal_dist;
    }else{
        warn_msg += "[校正距離 the distance option for the calibration]\n";
        warn_flag = true;
    }
    if(cal_angle > 0){
        sys_set.calib_rot_degree = cal_angle;
    }else{
        warn_msg += "[校正角度 the angle option for the calibration]\n";
        warn_flag = true;
    }
    if(align_tolerance > 0){
        sys_set.tolerance = align_tolerance;
    }else{
        warn_msg += "[對位容許值 the tolerance option for the alignment]\n";
        warn_flag = true;
    }
    if(align_repeat > 0){
            sys_set.align_repeat = align_repeat;
    }else{
        warn_msg += "[對位重複數 the alignment repetition option for the alignment]\n";
        warn_flag = true;
    }
    if(calib_dist_tolerance > 0){
        sys_set.calib_dist_diff_tolerance = calib_dist_tolerance;
    }else{
        warn_msg += "[校正誤差量 the calibration distance tolerance]\n";
        warn_flag = true;
    }
    if(align_distance_tol > 0){
        sys_set.align_distance_tol = align_distance_tol;
    }else{
        warn_msg += "[對位距差 the alignment distance tolerance]\n";
        warn_flag = true;
    }
    if(warn_flag){
        WarningMsg("Warning Message", warn_msg, 3);
    }
    sys_set.enable_mask = enable_mask;

    align.SetDistanceCal(sys_set.calib_distance);
    align.SetAngCal(sys_set.calib_rot_degree);
    align.SetTOL(sys_set.tolerance);
    align.SetCalibDistTolerance(sys_set.calib_dist_diff_tolerance);
    align.SetAlignDisTol(sys_set.align_distance_tol);

    SaveSystemSetting();
}

void AlignSys::LoadParameters(QString recipeName)
{
    SystemLog("[AXN] Loading parameters...");
    if(dm::log) {
        qDebug() << "recipeName :" << recipeName;
        qDebug() << "currentRecipeName = " << currentRecipeName;
    }
    QString recipe_path = QString( root_path_ + "Data/recipe/%1").arg((recipeName));
    ui->label_recipeNum->setText(recipeName);

    QFile checkFile(recipe_path);
    if (!checkFile.exists()) {
        SystemLog(QString("[ERR] Recipe: %1 is not existed.").arg(recipeName));
    }
    else
    {
        // FileIO. Jimmy, 20220510. #fio33
        fio::FileIO fs(recipe_path.toStdString(), fio::FileioFormat::R);
        if (!fs.isEmpty() && fs.isOK()) {
            currentMode = fs.ReadtoInt("type", currentMode);
            if (fs.isOK()) {    // Got the recipe type
                /// Below, update the parameters which are relative to currentMode.
                if (camL->GetOfflineMode()) {   // Offline mode
                    camL->setSourceMode(currentMode);
                    camR->setSourceMode(currentMode);
                    usleep(300000);
                }

                // UI color setting
                if (currentMode == _MODE_MARKER_ || currentMode == _MODE_CHAMFER_) {
                    SystemLog(QString("[RSLT] Mode is [%1]")
                              .arg((currentMode == _MODE_MARKER_) ? ("Marker") : ("Chamfer")));
                    SetBtnStyle("background-color: rgb(253, 185, 62);"
                                "color: rgb(46, 52, 54);",
                                ui->btn_markerSetting);
                } else if (currentMode == _MODE_SHAPE_ONE_WAY_) {
                    SystemLog("[RSLT] Mode is [Shape]");
                    SetBtnStyle("background-color: rgb(253, 185, 62);"
                                "color: rgb(46, 52, 54);",
                                ui->btn_shapeSetting);
                } else {
                    SystemLog("[RSLT] Mode is [IDLE]");
                    SetBtnStyle("");
                }

                //Load last template file
                SystemLog("[AXN] Loading template...");
                QString template_name_0 = "\0";
                QString template_name_1 = "\0";

                // Check the template path correction - left cam
                QString template_path_l = QString::fromStdString(fs.Read("last_template_path"));
                if (!fs.isOK()) {
                    SystemLog("[ERR] Left template setting is empty !");
                    isLoadtemp = false;
                }
                if (!template_path_l.contains("AlignmentSys")) {
                    template_path_l = root_path_ + template_path_l;
                }
                QFile template_file_l(template_path_l); // Check file is existed.
                if (template_path_l.isEmpty()) {
                    SystemLog(QString("[FAIL] Template path: %1 is empty.").arg(template_path_l));
                    isLoadtemp = false;
                } else if (!template_file_l.exists()) {
                    SystemLog(QString("[FAIL] Template path: %1 isn't existed.").arg(template_path_l));
                    isLoadtemp = false;
                } else {
                    QStringList list_name = template_path_l.split("/");

                    // Here, it's the successful case.
                    if ((list_name.size() > 3) && (template_path_l.contains(".png")))   // Jang's comment: why it checks the path size?
                    {
                        RST_template = cv::imread(template_path_l.toUtf8().constData(), cv::IMREAD_COLOR);
                        template_name_0 = list_name.at(list_name.size()-1);   //根據`/`區分
                        isLoadtemp = true;

                    } else {
                        SystemLog("[ERR] Can't find template!");
                        markerSettingDialog->setTemplateName("名稱 Name: ");
                        markerSettingDialog->clearImage();
                        markerSettingDialog->setResolutionTemplate(QString("解析度 Resolution: ooo x ooo pixels"));
                        currentTemplateName = "";
                        isLoadtemp = false;
                    }
                }
                template_file_l.close();

                // Check the template path correction - right cam
                QString template_path_r = QString::fromStdString(fs.Read("last_template_R_path"));
                if (!fs.isOK()) {
                    SystemLog("[ERR] Right template setting is empty!");
                    isLoadtemp_r = false;
                }
                if (! template_path_r.contains("AlignmentSys")) {
                    template_path_r = root_path_ +  template_path_r;
                }
                QFile template_file_r( template_path_r); // Check file is existed.
                if ( template_path_r.isEmpty()) {
                    SystemLog(QString("[FAIL] Template path: %1 is empty.").arg( template_path_r));
                    isLoadtemp_r = false;
                } else if (!template_file_r.exists()) {
                    SystemLog(QString("[FAIL] Template path: %1 isn't existed.").arg( template_path_r));
                    isLoadtemp_r = false;
                } else {
                    QStringList list_name = template_path_r.split("/");

                    // Here, it's the successful case.
                    if ((list_name.size() > 3)&&( template_path_r.contains(".png")))   // Jang's comment: why it checks the path size?
                    {
                        RST_template_R = cv::imread( template_path_r.toUtf8().constData(), cv::IMREAD_COLOR);
                        template_name_1 = list_name.at(list_name.size()-1);   //根據`/`區分
                        isLoadtemp_r = true;

                    } else {
                        SystemLog("[ERR] Can't find template!");
                        markerSettingDialog->setTemplateName("名稱 Name: ");
                        markerSettingDialog->clearImage();
                        markerSettingDialog->setResolutionTemplate(QString("解析度 Resolution: ooo x ooo pixels"));
                        currentTemplateName = "";
                        isLoadtemp_r = false;
                    }
                }
                template_file_r.close();

                if (markerSettingDialog->getCurrentMode() == _ONLY_TEMPLATE_ /*&& template_index_1 == 0*/){
                    if(isLoadtemp){
                        RST_template_R = RST_template.clone();

                        markerSettingDialog->setCurrentRecipeName(recipeName);
                        markerSettingDialog->setTemplateNameR("名稱 Name: " + template_name_1);
                        markerSettingDialog->setTemplateName("名稱 Name: " + template_name_0);
                        QImage image= QImage((const uchar*)RST_template.data, RST_template.cols, RST_template.rows, RST_template.step, QImage::Format_RGB888).rgbSwapped();
                        markerSettingDialog->setImageTemplate(image);
                        markerSettingDialog->setImageTemplate(RST_template, RST_template_R);
                        markerSettingDialog->setResolutionTemplate(QString("解析度 Resolution: %1 x %2 pixels").arg(RST_template.cols).arg(RST_template.rows));
                        currentTemplateName = template_name_0;

                        if (template_name_0 == template_name_1) {
                            markerSettingDialog->setCurrentMode(_ONLY_TEMPLATE_);
                        }

                        if(!is_template_loaded_)
                            RST_Train(false);   // No need re-training
                        SystemLog("[OK] Load template successfully!");

                        }
                    } else{
                        if(isLoadtemp && isLoadtemp_r){
                            markerSettingDialog->setCurrentRecipeName(recipeName);
                            markerSettingDialog->setTemplateNameR("名稱 Name: " + template_name_1);
                            markerSettingDialog->setTemplateName("名稱 Name: " + template_name_0);
                            QImage image= QImage((const uchar*)RST_template.data, RST_template.cols, RST_template.rows, RST_template.step, QImage::Format_RGB888).rgbSwapped();
                            markerSettingDialog->setImageTemplate(image);
                            markerSettingDialog->setImageTemplate(RST_template, RST_template_R);
                            markerSettingDialog->setResolutionTemplate(QString("解析度 Resolution: %1 x %2 pixels").arg(RST_template.cols).arg(RST_template.rows));
                            currentTemplateName = template_name_0;

                            markerSettingDialog->setCurrentMode(_EACH_TEMPLATE_R_);
                            if(!is_template_loaded_){
                                RST_Train(false);   // No need re-training
                            }
                            markerSettingDialog->setCurrentMode(_EACH_TEMPLATE_L_);
                            if(!is_template_loaded_){

                                RST_Train(false);
                            }
                            left_template_collected = 1;
                            right_template_collected = 1;
                            if(RST_template.empty()){
                                left_template_collected = 0;
                                isLoadtemp = false;
                            }
                            if(RST_template_R.empty()){
                                right_template_collected = 0;
                                isLoadtemp_r = false;
                            }

                            if(left_template_collected == 1 && right_template_collected == 1) is_match_available = 1;
                            else is_match_available = 0;
                            SaveMatchModeStatus();
                            // Set template status
                            markerSettingDialog->SetTemplateStatus(left_template_collected > 0, right_template_collected > 0);
                        }
                    }

                    if (!isLoadtemp || !isLoadtemp_r)
                    {
                        SystemLog("[ERR] Can't find template file setting!");
                        markerSettingDialog->setTemplateNameR("名稱 Name: ");
                        markerSettingDialog->setTemplateName("名稱 Name: ");

                        markerSettingDialog->clearImage();
                        markerSettingDialog->setResolutionTemplate(QString("解析度 Resolution: ooo x ooo pixels"));
                        currentTemplateName = "";
                    }

                    // Load calibration data from files
                    if (align.LoadCalibrationResultFromFile(recipe_path))
                    {
                        SystemLog("[OK] Load Calibration result successfully!");

                        if ((align.GetPosRef1().x != 0) &&
                                (align.GetPosRef1().y != 0) &&
                                (align.GetPosRef2().x != 0) &&
                                (align.GetPosRef2().y != 0))
                        {
                            refPtExist = true;
                            updateRefInfoL = true;
                            updateRefInfoR = true;
                        }else{
                            refPtExist = false;
                        }

                        if(align.GetPixResolution() > 0.00000000001){
                            isCalibReadyForAlign = true;
                        }
                        else{
                            isCalibReadyForAlign = false;
                        }

                        ui->lbl_CoorO1->setText(" ⬉(0, 0) → (0, 0)");
                        ui->lbl_CoorO2->setText(QString(" ⬉(0, 0) → (%1, %2)")
                                                .arg(QString::number(align.GetOrigin2().x, 'd', 1))
                                                .arg(QString::number(align.GetOrigin2().y, 'd', 1)));

                        isDoneCalib = true;
                    }
                    else
                    {
                        WarningMsg("Warning Message", "無法取得校正資訊, 請先進行校正 ！！", 3);
                        SystemLog("[WARN] 無法取得校正資訊, 請先進行校正！！");
                        isDoneCalib = false;
                    }

            } else {    // Cannot get the recipe type
                SystemLog("[ERR] Cannot get the recipe type.");
                currentMode = _MODE_IDLE_;
                markerSettingDialog->setTemplateName("名稱 Name: ");
                markerSettingDialog->clearImage();
                markerSettingDialog->setResolutionTemplate(QString("解析度 Resolution: ooo x ooo pixels"));
                currentTemplateName = "";
            }
        }
        fs.close();
    }
}



void AlignSys::ClearTemplates()
{
    QString recipeName = currentRecipeName;
    QString recipe_path = QString( root_path_ + "Data/recipe/%1").arg((recipeName));

    if(dm::log) {
        qDebug() << "ClearTemplates recipe_path:" << recipe_path;
    }
    QFile checkFile(recipe_path);
    if (!checkFile.exists())
    {
        SystemLog(QString("[WARN] Recipe: %1 is not existed.").arg(recipeName));
    }
    else
    {
        // FileIO. Jimmy, 20220511. #fio34
        fio::FileIO fs(recipe_path.toStdString(), fio::FileioFormat::R);
        if (!fs.isEmpty() && fs.isOK() && fs.contains("type")) {
            if (markerSettingDialog->getCurrentMode() == _EACH_TEMPLATE_L_) {
                QString template_path_l = QString::fromStdString(fs.Read("last_template_path"));
                if (fs.isOK()) {    // left template path is found.
                    if (!template_path_l.contains("AlignmentSys"))
                        template_path_l = root_path_ + template_path_l;

                    if (dm::log)
                        qDebug() << "template_path:" << template_path_l;

                    // ** Remove the previous left template
                    if (!fs::is_directory(template_path_l.toStdString()))
                        fs::remove(template_path_l.toStdString());
                }
            } else if (markerSettingDialog->getCurrentMode() == _EACH_TEMPLATE_R_) {
                QString template_path_r = QString::fromStdString(fs.Read("last_template_R_path"));
                if (fs.isOK()) {    // right template path is found.
                    if (!template_path_r.contains("AlignmentSys"))
                        template_path_r = root_path_ + template_path_r;

                    if (dm::log)
                        qDebug() << "template_path:" << template_path_r;

                    // ** Remove the previous right template
                    if (!fs::is_directory(template_path_r.toStdString()))
                        fs::remove(template_path_r.toStdString());
                }
            }
        }
        fs.close();
    }
}
//****************************************************//
//****************** Slot Functions ******************//
//****************************************************//
void AlignSys::HandleCal()
{
#ifdef MC_PROTOCOL_PLC	// add by garly
    FinishPlcCmd();
if(dm::log) {
    qDebug() << "Next calibration count : " << count_calpos << endl;
}
#else // MC_PROTOCOL_PLC
    emit sig_readyCal();
#endif
}

/*************************************************
 * Set options
 *************************************************/
void AlignSys::UpdateCurrentMatchMode(int mode_){
    if(currentMode != mode_){
        // if the matching methods changed, status of template should be initialized.
        left_template_collected = 0;
        right_template_collected = 0;
        is_match_available = 0;
        // Jang 20220307
        is_new_collecting_left = true;
        is_new_collecting_right = true;

        currentMode = mode_;
        SaveMatchModeStatus();

        isLoadtemp = false;
        isLoadtemp_r = false;
        ClearTemplates();
    }
    else{

    }
    currentMode = mode_;

    if(currentMode == _MODE_CHAMFER_){
        // Initialisation for Chamfer matching
        match.InitPaths(root_path_.toStdString() + "Data/" + recipe_dir.toStdString() + "/methods/chamfer/match_0",
                        root_path_.toStdString() + "Log/chamfer/match_0",
                        root_path_.toStdString() + "Data/" + recipe_dir.toStdString() + "/parameters/");

        match_R.InitPaths(root_path_.toStdString() + "Data/" + recipe_dir.toStdString() + "/methods/chamfer/match_1",
                          root_path_.toStdString() + "Log/chamfer/match_1",
                          root_path_.toStdString() + "Data/" + recipe_dir.toStdString() + "/parameters/");
    }else{
        // Initialisation for RST matching
        match.InitPaths(root_path_.toStdString() + "Data/" + recipe_dir.toStdString() + "/methods/rst/match_0",
                        root_path_.toStdString() + "Log/rst/match_0",
                        root_path_.toStdString() + "Data/" + recipe_dir.toStdString() + "/parameters/");
        match_R.InitPaths(root_path_.toStdString() + "Data/" + recipe_dir.toStdString() + "/methods/rst/match_1",
                          root_path_.toStdString() + "Log/rst/match_1",
                          root_path_.toStdString() + "Data/" + recipe_dir.toStdString() + "/parameters/");
    }
    if(sys_set.enable_mask){
        cv::Mat source_img_mask_l, source_img_mask_r;
        match.GetSrcImageMask(source_img_mask_l);
        match_R.GetSrcImageMask(source_img_mask_r);
        markerSettingDialog->SetMask(source_img_mask_l, source_img_mask_r);
    }

    LoadSystemConfig();
    currentMode = mode_;
    WriteSystemConfig();

    UpdateTitle(currentMode);//garly_20220224, change title by matching method

    // Update the recipe type. Jimmy. 20220406.
    // FileIO. 20220419. Jimmy. #fio07
    std::string file_path = root_path_.toStdString() + "Data/recipe/" + currentRecipeName.toStdString();
    fio::FileIO replace_file(file_path, fio::FileioFormat::RW);
    if (replace_file.isOK())
        replace_file.ReplaceByKey("type", std::to_string(currentMode));
    replace_file.close();
}

void AlignSys::UpdateSettingDialogFlag()
{
    markerSettingDialogIsShow = false;
    subMenuActive = false;
    isCreatetemp = false;
    isMaskRoi = false;

    // Jang 20220419
    // Remove templates if collection failed
    if(left_template_collected == 0 || right_template_collected == 0){
        RST_template.release();
        RST_template_R.release();
        ClearTemplates();
        markerSettingDialog->SetTemplateStatus(false, false);
        // Not to show the ROIs in the screen
        buf_size_1 = 0;
        buf_size_2 = 0;

    }
    // Jang 20220307
    is_new_collecting_left = true;
    is_new_collecting_right = true;
    left_template_collected = 0;
    right_template_collected = 0;

    SetMainMenuBtnEnable(true);
}


/*************************************************
 * Parameter and file contoller
 *************************************************/


void AlignSys::WriteSystemConfig()
{
    SystemLog("[AXN] Write system configuration.");

    QDir tmp_dir(root_path_);
    if (!tmp_dir.exists())
        SystemLog("[ERR] Can't find path : " + root_path_);
    else
    {
        QString systemConfigName = QString("%1/Data/systemConfig.ini").arg(root_path_);
        QFile checkFile(systemConfigName);

        if (checkFile.exists())
        {
            // FileIO. Jimmy, 20220511. #fio35
            fio::FileIO fs(systemConfigName.toStdString(), fio::FileioFormat::W);
            if (fs.isOK()) {   // The file can open.
                fs.Write("Type", std::to_string(currentMode));
                fs.Write("RecipeName", currentRecipeName.toStdString());
                // Record recipe change by PLC. Jimmy 20220627
                if (is_reset_CCDsys_)
                    fs.Write("IsCCDSysReset", "1");
                else
                    fs.Write("IsCCDSysReset", "0");
            } else {    // File cannot open.
                SystemLog(QString("[ERR] Cannot create file: %1").arg(systemConfigName));
            }
            fs.close();
        }
    }
}

bool AlignSys::LoadSystemConfig()
{
    SystemLog("[AXN] Load system configuration in loading function...");

    QDir tmp_dir(root_path_);
    if (!tmp_dir.exists())
    {
        SystemLog("[ERR] Can't find path : " + root_path_);
        return false;
    }
    else
    {
        QString systemConfigName = QString("%1/Data/systemConfig.ini").arg(root_path_);
        QFile checkFile(systemConfigName);

        if (checkFile.exists())
        {
            // FileIO. Jimmy, 20220511. #fio36
            fio::FileIO fs(systemConfigName.toStdString(), fio::FileioFormat::R);
            if (!fs.isEmpty() && fs.isOK()) {
                currentMode = fs.ReadtoInt("Type", currentMode);
                currentRecipeName = QString::fromStdString(fs.Read("RecipeName", "=", currentRecipeName.toStdString()));

                switch (currentMode) {
                case _MODE_MARKER_:
                case _MODE_CHAMFER_:
                    SetBtnStyle("background-color: rgb(253, 185, 62);"
                                "color: rgb(46, 52, 54);",
                                ui->btn_markerSetting);
                    break;
                case _MODE_SHAPE_ONE_WAY_:
                    SetBtnStyle("background-color: rgb(253, 185, 62);"
                                "color: rgb(46, 52, 54);",
                                ui->btn_shapeSetting);
                    break;
                case _MODE_IDLE_:
                default:
                    // Do nothing
                    break;
                }
                fs.close();
                return true;
            } else {
                fs.close();
                SystemLog("[FAIL] Configuration setting is empty!");
                return false;
            }
        }
        else
        {
            SystemLog("[FAIL] Can't find systemConfig.ini");
            return false;
        }
    }
    return false;
}

bool AlignSys::LoadLastRecipe()
{
    return LoadSystemConfig();
}

void AlignSys::ActionLoadTempFromFile()
{
    // stop the thread
    camL->Stop();
    camR->Stop();

    /// Tri's Load Image
    if(dm::log) {
        qDebug() << ">>> Open File dialog <<<" << endl;
    }
    QFileDialog *fileDialog = new QFileDialog(this);
    fileDialog->setAttribute(Qt::WA_DeleteOnClose);
    fileDialog->setWindowTitle(tr("Open Image"));
    fileDialog->setDirectory( root_path_ + "Data/");

    if(fileDialog->exec() == QDialog::Accepted)
    {
        isLoadtemp = true;
        QString path = fileDialog->selectedFiles()[0];

        RST_template = cv::imread(path.toUtf8().constData(), cv::IMREAD_COLOR); //cvLoadImage(path.toUtf8().constData(),3);
        QImage image= _Mat2QImg(RST_template);

        markerSettingDialog->setImageTemplate(image);
        markerSettingDialog->setResolutionTemplate(QString("解析度 Resolution: %1 x %2 pixels").arg(RST_template.cols).arg(RST_template.rows));

        cv::String imageName1 = path.toUtf8().constData();
        WriteLastTemplate(imageName1);

        Temppath = imageName1;
        QStringList tmp = QString::fromStdString(imageName1).split("/");
        QString str = tmp.at(tmp.size()-1);
        TempName = str.toStdString();

        markerSettingDialog->setTemplateName("名稱 Name: " + QString::fromStdString(TempName));
        currentTemplateName = QString::fromStdString(TempName);
        if(dm::log) {
            qDebug() << ">>> Open File dialog finish<<<" << endl;
        }
    }
    // start the thread
    camL->Play();
    camR->Play();

    // Train Template
    RST_Train(true);
    log("[Action] 完成載入與訓練模板影像."); //log("Complete load & train temp!!");
    ActionMarkerDetect(); // add by agrly
}

void AlignSys::ActionCreatTemp()
{
    isCreatetemp = true;
}

void AlignSys::UpdateCalibrationParms(double dis, double ang)
{
    if (dis != 0.0) {
        align.SetDistanceCal(dis);
    } else {
        // Do nothing
    }
    if (ang != 0.0) {
        align.SetAngCal(ang);
    } else {
        // Do nothing
    }
}

void AlignSys::UpdateAlignmentParms(double tol, int align_repeat, double align_dis_tol)
{
    if (tol != 0){
        align.SetTOL(tol);
        align.SetAlignDisTol(align_dis_tol);
        sys_set.align_repeat = align_repeat;
        sys_set.align_distance_tol = align_dis_tol;
    }
}

void AlignSys::SetShapeMode(){
    this->currentMode = _MODE_SHAPE_ONE_WAY_;
}

QString AlignSys::GetRecipeFullName(QString num){
    QDir dir(root_path_ + "Data/recipe");

    for(int i = num.size(); i < 4; i ++){
        num.push_front("0");
    }

    bool numeric_ok = false;
    QString item_idx_name = QString("%1").arg(num.toInt(&numeric_ok), 4, 10, QLatin1Char('0'));
    num = item_idx_name;
    if (!numeric_ok) {
        num = "0000";
    }

    QStringList recipe_list = dir.entryList(QDir::Files);
    foreach (QString recipe_file_name, recipe_list) {
        // recipe_0000+UVWdefault.txt
        QString file_name_txt = recipe_file_name.split("_").at(1);
        // 0000+UVWdefault.txt

        if (num.contains(file_name_txt.mid(0, 4))){
            return file_name_txt.split(".").at(0);
            // 0000+UVWdefault
        }
    }

    return "none";
}

void AlignSys::ChangeRecipe(QString recipe_name, int src)
{
    recipe_num_list_ = ListAllRecipeNum(); // Sould update the current recipe list. Jimmy 20220628

    ol::WriteLog("[SYSTEM] Recipe is changed: " + recipe_name.toStdString());
    QString recipe_num = recipe_name.mid(0, 4); //Ex: 0014+test -> 0014
    QString recipe_num_curr = currentRecipeName.mid(7, 4);
    QString recipe_fullname = GetRecipeFullName(recipe_num);

    if(dm::log) {
        qDebug() << "recipe_name: " << recipe_name;
        qDebug() << "recipe_fullname: " << recipe_fullname;
        qDebug() << "[From PLC] Select recipe: " << recipe_name;
        qDebug() << "[Action] *** Align System *** Change Recipe :"
                 << recipe_name << ",num :" << recipe_num << endl;
    }

    // Check the recipe num is existed. Jimmy 20220622.
    if (!recipe_num_list_.contains(QString::number(recipe_num.toUInt()))) {
        // Cannot find the recipe num in all recipe list
        // emit the error code which means we cannot find recipe number.
        ErrorLog("[ERR][Error r001] 無法找到對應recipe，請重新檢查.");
        WriteErrorCodeToPlc("r001");
        //QString error_code = "r001";
        //emit signalPlcWriteCurrentErrorCodeNum(error_code);
        if (dm::log) {
            qDebug() << "[Warning] Cannot find the recipe number: " << recipe_num << ".";
        }
        return;
    }

    /// Change recipe by PLC.
    if (src == 1) { // src == 1 : Change recipe by PLC

        // Check change the same with current recipe number, and it should be skip. Jimmy 20220627
        if (dm::log) {
            qDebug() << " ========== Change Recipe by PLC ========== ";
            qDebug() << "[Info]:" << recipe_num_curr << "," << recipe_num;
            qDebug() << "[Info]:" << recipe_num_curr.toUInt() << "," << recipe_num.toUInt();
            qDebug() << "[Info]:" << (recipe_num_curr.toUInt() == recipe_num.toUInt());
        }
        if (recipe_num_curr.toUInt() == recipe_num.toUInt()) {
            if (dm::log) {
                qDebug() << "[Warning] Changing recipe is the same as current recipe! Skip.";
            }
            //garly_20220120, add for change recipe from plc
            emit signalPlcSetCurrentRecipeNum(recipe_num.toUInt());
            emit signalPlcWriteCurrentRecipeNum();    //garly_20220120, add for change recipe from plc
            emit signalPlcWriteResult(0, 0, 0, 1, PlcWriteMode::kOthers);  //result=> 0: continue; 1: finish; 2: NG

            usleep(100000);
            FinishPlcCmd();
            usleep(100000);


            return;
        } else if (recipe_fullname == "none") {
            emit signalPlcWriteCurrentRecipeNum();    //garly_20220120, add for change recipe from plc
            emit signalPlcWriteResult(0, 0, 0, 2, PlcWriteMode::kOthers);  //result=> 0: continue; 1: finish; 2: NG

            usleep(100000);
            FinishPlcCmd();
            usleep(100000);


            return;
        } else {
            // Changing recipe name is diff. with currenct one.
            // Change and restart the program first before send the OK to PLC.
            // The checking file is /Data/systemConfig.ini
            is_reset_CCDsys_ = true;
        }
    }
    emit signalPlcSetCurrentRecipeNum(recipe_num.toUInt());

    RecordLastModifiedTime();   // Record closed time. Jimmy 20220518.

    SystemLog(QString("[AXN] Change recipe : %1, num : %2").arg(recipe_fullname).arg(recipe_num));
    currentRecipeName = QString("recipe_%1.txt").arg(recipe_fullname);
    /// Update path
    recipe_dir = recipe_fullname;
    isLoadtemp = false;
    LoadParameters(currentRecipeName);

    WriteSystemConfig();

    emit signalPlcSetPath(root_path_, recipe_dir);
    emit signalPlcLoadSetting();

    markerSettingDialog->SetProjectPath(root_path_, recipe_dir);
    systemSettingDialog->SetProjectPath(root_path_, recipe_dir);
    systemSettingDialog->SetSystemSetting(this->sys_set);
    cam_setting_path =(root_path_ + "/Data/" + recipe_dir + "/parameters").toStdString();
    align.SetSystemMode(this->machine_type, this->calibration_mode, this->system_unit);
    align.SetProjectPath(root_path_, recipe_dir);
    parameter_setting_path = (root_path_ + "/Data/" + recipe_dir + "/parameters/").toStdString();
    shape_align_0.SetDataPath(parameter_setting_path, root_path_.toStdString() + "/Log");
    shape_align_1.SetDataPath(parameter_setting_path, root_path_.toStdString() + "/Log");
    shape_align_0.SetObjectCamId(0, 0);
    shape_align_1.SetObjectCamId(0, 1);
    opt_recipe_path_ = (root_path_ + "/Data/").toStdString() + "/" + recipe_dir.toStdString();
#ifdef ENABLE_AUTO_ADJUST_BRIGHTNESS
    cameraSettingDialog->SetProjectPath((systemConfigPath + "/Data/").toStdString(), recipe_dir.toStdString());
#endif
    if(currentMode == _MODE_CHAMFER_){
        // Initialisation for RST matching
        match.InitPaths(root_path_.toStdString() + "Data/" + recipe_dir.toStdString() + "/methods/chamfer/match_0",
                        root_path_.toStdString() + "Log/chamfer/match_0",
                        root_path_.toStdString() + "Data/" + recipe_dir.toStdString() + "/parameters/");
        match_R.InitPaths(root_path_.toStdString() + "Data/" + recipe_dir.toStdString() + "/methods/chamfer/match_1",
                          root_path_.toStdString() + "Log/chamfer/match_1",
                          root_path_.toStdString() + "Data/" + recipe_dir.toStdString() + "/parameters/");
    }else{
        // Initialisation for RST matching
        match.InitPaths(root_path_.toStdString() + "Data/" + recipe_dir.toStdString() + "/methods/rst/match_0",
                        root_path_.toStdString() + "Log/rst/match_0",
                        root_path_.toStdString() + "Data/" + recipe_dir.toStdString() + "/parameters/");
        match_R.InitPaths(root_path_.toStdString() + "Data/" + recipe_dir.toStdString() + "/methods/rst/match_1",
                          root_path_.toStdString() + "Log/rst/match_1",
                          root_path_.toStdString() + "Data/" + recipe_dir.toStdString() + "/parameters/");
    }
    if(sys_set.enable_mask){
        cv::Mat source_img_mask_l, source_img_mask_r;
        match.GetSrcImageMask(source_img_mask_l);
        match_R.GetSrcImageMask(source_img_mask_r);
        markerSettingDialog->SetMask(source_img_mask_l, source_img_mask_r);
    }

    UpdateCurrentMatchMode(currentMode);    // currentMode from LoadParameters()
    if (dm::log) {
        qDebug() << "currentMode: " << currentMode;
    }
    markerSettingDialog->SetMode(currentMode);

    func_mode = _MODE_NONE_;

    refPtExist = false;
    InitializeUI();

    LoadSystemSetting();

    ResetAllParameters();
}

void AlignSys::UpdateROIRectInfo(cv::Rect rect)
{
    // SAVE Rect
    std::string bright_file_path;
    if(markerSettingDialog->getCurrentMode() == _EACH_TEMPLATE_R_){
        roi_rect_r_ = rect;
        bright_file_path = parameter_setting_path + "/roi_cam_R.txt";
    }
    else{
        roi_rect_l_ = rect;
        bright_file_path = parameter_setting_path + "/roi_cam_L.txt";
    }
    cv::FileStorage cv_fs;

    try
    {
        if (cv_fs.open(bright_file_path, cv::FileStorage::WRITE))
        {
            cv_fs << "roi_rect" <<  rect;
        }
        else
        {
            SystemLog("[WARN] The file cannot be opened: " + QString::fromStdString(bright_file_path));
            return;
        }
    }
    catch (cv::Exception e) {
        SystemLogfWithCodeLoc("[Error] FileStorage error: " + QString(e.what()),
                              __FILE__, __func__);
    }
    cv_fs.release();
}
// Jang 20220629
void AlignSys::LoadROIRectInfo()
{
    std::string bright_file_path_l = parameter_setting_path + "/roi_cam_R.txt";
    std::string bright_file_path_r = parameter_setting_path + "/roi_cam_L.txt";

    cv::FileStorage cv_fs;
    try
    {
        if (cv_fs.open(bright_file_path_l, cv::FileStorage::READ))
        {
            cv_fs["roi_rect"] >>  roi_rect_l_;
        }
        else
        {
            SystemLog("[WARN] The file cannot be opened: " + QString::fromStdString(bright_file_path_l));
            return;
        }
    }
    catch (cv::Exception e) {
        SystemLog("[ERR] FileStorage error: " +  QString(e.what()));
    }
    cv_fs.release();
    try
    {
        if (cv_fs.open(bright_file_path_r, cv::FileStorage::READ))
        {
            cv_fs["roi_rect"] >>  roi_rect_r_;
        }
        else
        {
            SystemLog("[WARN] The file cannot be opened: " + QString::fromStdString(bright_file_path_r));
            return;
        }
    }
    catch (cv::Exception e) {
        SystemLog("[ERR] FileStorage error: " +  QString(e.what()));
    }
    cv_fs.release();
}

void AlignSys::ProcessFinishMaskROI()
{
    isMaskRoi = false;
}

void AlignSys::WriteLastTemplate(std::string str)
{
    QString recipe_path = QString(root_path_ + "Data/recipe/%1").arg((currentRecipeName));

    QFile checkFile(recipe_path);
    if (!checkFile.exists()) {
        SystemLog(QString("[ERR] Recipe: %1 is not existed.").arg(currentRecipeName));
        if(dm::log) {
            qDebug() << QString("%1 is not exist").arg(currentRecipeName) << endl;
        }
    } else {
        QStringList list_name = QString::fromStdString(str).split("/");              //根據`/`區分
        QString recipeName = currentRecipeName.split("_").at(1).split(".").at(0);
        QString wr_str = QString("last_template_path=Data/%1/%2").arg(recipeName).arg(list_name.at(list_name.size()-1));

        // FileIO. Jimmy, 20220511. #fio37
        fio::FileIO fs(recipe_path.toStdString(), fio::FileioFormat::RW);
        if (!fs.isEmpty()) {
            fs.ReplaceByKey("type", std::to_string(currentMode));
            fs.ReplaceByKey("last_template_path", wr_str.toStdString());
        }
        fs.close();

        QString dst_file = QString(root_path_ + "Data/%1/%2").arg(recipeName).arg(list_name.at(list_name.size()-1));
        if (QFile::exists(dst_file))
        {
            SystemLog(QString("[OK] File:%1 is existed. Remove it.").arg(dst_file));
            if(dm::log) {
                qDebug() << "file is exist" << endl;
            }
            QFile::remove(dst_file);
        }
        QFile::copy(QString::fromStdString(str), dst_file);
    }
}

void AlignSys::UpdateCurrentTemplateName(QString str)
{
    currentTemplateName = str;
    if(dm::log) {
        qDebug() << "currentTemplateName = " << currentTemplateName << endl;
    }
}

void AlignSys::SaveCalibrationImage(const cv::Mat &img, int index, int lr)
{
    QString recipeName = currentRecipeName.mid(7).split(".").at(0);
    QString calib_path = QString( root_path_ + "Data/%1/calibration").arg(recipeName);
    QDir new_temp_dir(calib_path);
    if (!new_temp_dir.exists())
        new_temp_dir.mkpath(calib_path);

    QString temp_path = QString("%1/cal_%2_%3.png").arg(calib_path).arg(index).arg(lr == 0 ? "L" : "R");

    // QImage.save takes about 500ms
    // It is too slow
    // QImage image=_Mat2QImg(img);
    // image.save(temp_path, "PNG");

    // cv::imwrite takes about 100ms
    cv::imwrite(temp_path.toStdString(), img);
}
/*************************************************
 * Camera and Image
 *************************************************/

void AlignSys::ResetStatusByTimeout(){
    double host_elapsed;
    // To reset the current status after the alignment (YT system)
    if(func_mode == _MODE_ALIGNMENT_){
        if(this->sys_set.align_type == _SKIP_ALIGN_SEND_OFFSET_){
            if(isDoneAlign){
                clock_gettime(CLOCK_MONOTONIC, &time_end);
                host_elapsed = clock_diff (&time_start, &time_end);
                if(host_elapsed > (func_timeout_ / 2)){
//                    func_mode = _MODE_NONE_;
//                    ResultStatus("IDLE");
//                    ui->lbl_Mode->setText("IDLE");
                    show_match_result_ = false; // Jang 20221020

                }
            }
            if(isRunAlign){
                clock_gettime(CLOCK_MONOTONIC, &time_end);
                host_elapsed = clock_diff (&time_start, &time_end);
                if(host_elapsed > func_timeout_){
                    func_mode = _MODE_NONE_;
                    ResultStatus("NG");
                    ui->lbl_Mode->setText("IDLE");

                    isRunAlign = false;
                    isAlignmentCheck = false;
                    isAlignmentOK = false;
                    show_match_result_ = false; // Jang 20221020
                    alignRepeatTime = 0;

                    time_log.SaveElapsedTime("Failed");
                    time_log.FinishRecordingTime("Alignment failed");
                    ErrorLog("[ERR][Error a006] Alignment failed. (Timeout)");
                    WriteErrorCodeToPlc("a006");
                }
            }
        }
        else{
//            if(isDoneAlign && plc_status_ == _PLC_IDLE_){
//                clock_gettime(CLOCK_MONOTONIC, &time_end);
//                host_elapsed = clock_diff (&time_start, &time_end);
//                if(host_elapsed > func_timeout_){
//                    clock_gettime(CLOCK_MONOTONIC, &time_start);
//                    func_mode = _MODE_NONE_;
//                    ResultStatus("IDLE");
//                    ui->lbl_Mode->setText("IDLE");
//                }
//            }
            if(isRunAlign || (!isRunAlign && isAlignmentCheck)){
                clock_gettime(CLOCK_MONOTONIC, &time_end);
                host_elapsed = clock_diff (&time_start, &time_end);
                if(host_elapsed > func_timeout_){
                    func_mode = _MODE_NONE_;
                    ResultStatus("NG");
                    ui->lbl_Mode->setText("IDLE");

                    isRunAlign = false;

                    isAlignmentCheck = false;
                    isAlignmentOK = false;
                    show_match_result_ = false; // Jang 20221020
                    alignRepeatTime = 0;

                    time_log.SaveElapsedTime("Failed");
                    time_log.FinishRecordingTime("Alignment failed");
                    ErrorLog("[ERR][Error a006] Alignment failed. (Timeout)");
                    WriteErrorCodeToPlc("a006");
                }
            }
        }
    }

    // To reset the current status after signals lost (while calibration)
    else if(isRunCalib){
        clock_gettime(CLOCK_MONOTONIC, &time_end);
        host_elapsed = clock_diff (&time_start, &time_end);
        if(host_elapsed > func_timeout_){
            func_mode = _MODE_NONE_;

            isDoneCalib = false;
            count_calpos = 0;
            isRunCalib = false;
            isCalibReadyForAlign = false;

            ResultStatus("NG");
            ui->lbl_Mode->setText("IDLE");
            time_log.SaveElapsedTime("Failed");
            time_log.FinishRecordingTime("Calibration Failed");
            ErrorLog("[ERR][Error b004] Calibration failed. (Timeout)");
            WriteErrorCodeToPlc("b004");
        }
    }

}

void AlignSys::UpdateFrameL(cv::Mat img)
{

//    lw_mem << "Using memory(" << std::to_string((int)getpid()) << "): " << (float)GetCurrentMem() / 1000 << lw_mem.endl;
//    SleepByQtimer(1);
//    log("oo");

    /*
     * Goal:    Update new frame of left camera FOV.
     * Input:   Image we update
     * Output:  None
     */
    /** Important Thing: it can't update the frame when cam->Stop()
        You should cam->Play() agian                            **/

    // Init status
    // Jang 20220630
    isUpdateImgL = true;
    isCamStop = false;
    if(isUpdateImgR){
        isCamOpenSuccess = true;
        paramSetting->SetEnabledCameraSetting(true);
    }

    if (!isUpdateImgL) isUpdateImgL = true;
    cv::Mat frtemp = img.clone();

    // Jang 20220718
//    imageprocSettingDialog->PreprocImage(frtemp, frtemp);

    // Barcode image showing, Jimmy 20220308.
#ifdef BARCODE_READER
    if (isBarcodeDialogShown) {
        if (!barcodeDialog->GetDetCamIndex()) {     // Left cam
            QImage qimg = _Mat2QImg(frtemp);
            emit signalBarcodeImage(qimg);
        }
    }
#endif

#ifdef ORIGINAL_SHAPE_ALIGN
    if(shapeSettingDialogIsShow){
        shapeSettingDialog->setImageL(frtemp);
        frameL = frtemp.clone();
    }
#endif

    if (!frtemp.empty() && !isCamStop)
    {
#ifdef ENABLE_AUTO_ADJUST_BRIGHTNESS
        if(is_running_bright_adjust){
            cameraSettingDialog->setImageL(frtemp, false);
        }
#endif
        frameL = frtemp.clone();

        // Update the image size
        ui->lbl_CoorTR1->setText(QString("(%1, %2)⬊").arg(frameL.cols - 1).arg(frameL.rows - 1));

        if (updateRefInfoL)
        {
            x11_std = align.GetPosRef1().x;
            y11_std = frameL.rows - align.GetPosRef1().y;
            updateRefInfoL = false;
        }

//        frtemp = img.clone();

        // Draw calibration cross-mark positions
        if (func_mode == _MODE_CALIBRATION_ && !cal_pos.empty())
        {
            for (unsigned int i=0; i<cal_pos.size(); i++)
            {
                if (!(i%2))
                {
                    Coor pos = cal_pos.at(i);
                    int x_calcross = int(pos.x);
                    int y_calcross = (frameL.rows - int(pos.y));
                    // Draw cross-mark
                    cv::line(frtemp,cv::Point(x_calcross-20,y_calcross),cv::Point(x_calcross+20,y_calcross),cv::Scalar(0,255,0),3);
                    cv::line(frtemp,cv::Point(x_calcross,y_calcross-20),cv::Point(x_calcross,y_calcross+20),cv::Scalar(0,255,0),3);
                    // Draw number
                    cv::putText(frtemp, std::to_string(i/2+1), cv::Point(x_calcross - 22, y_calcross + 18),
                                cv::FONT_HERSHEY_DUPLEX, 0.5, cv::Scalar(0, 255, 0), 1);
                }
            }
            if (isDoneCalib)
            {
                cv::Point center = cv::Point(pc.x, (-pc.y + frameL.rows));
                int r = (int)align.GetPcRadius();
                // Jang 20220517 // the circle size changed to be (r, r) from (r+20, r+20)
                cv::ellipse(frtemp, center, cv::Size(r, r), 0, 0, 360, cv::Scalar(255, 255, 255), 1);
                cv::line(frtemp, cv::Point(cal_pos.at(0).x, frameL.rows - cal_pos.at(0).y),
                         center, cv::Scalar(0, 255, 255), 2);
            }
        }
        else
        {
            if ((isCreatetemp == false) && (refPtExist == true) && x11_std > 0 && y11_std > 0)
            {
                // Draw reference position
                int x_cross1 = x11_std;
                int y_corss1 = y11_std;
                cv::line(frtemp, cv::Point(x_cross1-20,y_corss1), cv::Point(x_cross1+20,y_corss1), cv::Scalar(0,0,255),3);
                cv::line(frtemp, cv::Point(x_cross1,y_corss1-20), cv::Point(x_cross1,y_corss1+20), cv::Scalar(0,0,255),3);

                if ((markerSettingDialogIsShow) && (currentMode == _MODE_MARKER_ || currentMode == _MODE_CHAMFER_))
                    markerSettingDialog->setRefPointCoordinate(QString("參考點 Ref 1: (%1, %2)").arg(x11_std).arg((frameL.rows-y11_std)),
                                                               QString("參考點 Ref 2: (%1, %2)").arg(x22_std).arg((frameR.rows-y22_std)));
            #ifdef ORIGINAL_SHAPE_ALIGN
                if ((shapeSettingDialogIsShow) && (currentMode == _MODE_SHAPE_ONE_WAY_))
                    shapeSettingDialog->setRefpt(QString("參考點 Ref 1: (%1, %2)").arg(x11_std).arg((frameL.rows-y11_std)),
                                                 QString("參考點 Ref 2: (%1, %2)").arg(x22_std).arg((frameR.rows-y22_std)));
            #endif
                ui->lbl_Ref1->setText(QString("參考點 Ref 1: (%1, %2)").arg(x11_std).arg((frameL.rows-y11_std)));
                ui->lbl_Ref2->setText(QString("參考點 Ref 2: (%1, %2)").arg(x22_std).arg((frameR.rows-y22_std)));
            }

            // Draw transparent cross line on the screen
//            double alpha = 0.3;
//            cv::Mat frtemp_woline = frtemp.clone();

            if(showGuideline){
                cv::line(frtemp, cv::Point(0, frameL.rows / 2), cv::Point(frameL.cols, frameL.rows / 2), cv::Scalar(255,0,255), 2);
                cv::line(frtemp, cv::Point(frameL.cols / 2, 0), cv::Point(frameL.cols / 2, frameL.rows), cv::Scalar(255,0,255), 2);
            }

//            cv::addWeighted(frtemp, alpha, frtemp_woline, 1, -alpha, frtemp);
        }

        cv::Mat draw_result_img;
        emit signalPlcGetStatus();
        // Draw ROI of create rectangle
        if (isCreatetemp)
        {
            /*cv::rectangle(frtemp, ROI_lefttop_pos, ROI_rightbuttom_pos, cv::Scalar(0, 0, 255), 1, 8, 0);
            if (settingDialogIsShow)
            {
                markerSettingDialog->setImageL(frtemp);
            }*/
        }
        else
        {
            ResetStatusByTimeout();

            if (func_mode == _MODE_MARKER_DETECT_ || func_mode == _MODE_ALIGNMENT_) //marker detect and alignment
            {
                // doAlignRepeat is always true, so no need
                // isAlignmentOK is true when the alignment succeeded.
                // When the alignment started 'DrawResult' will be skipped (if isAlignmentCheck is true, it is second step)
                // Jang 20221020
                if(show_match_result_)
                /*if(isAlignmentCheck || isAlignmentOK)*/{
                    draw_result_img = DrawResult(frtemp,0, 1);
                    draw_result_img.copyTo(frtemp);
                }
            }

            // Jimmy 20221011, for pre-detection
            if (func_mode == _MODE_PREDETECT_) {
                draw_result_img = DrawResult(frtemp,0, 1);
                draw_result_img.copyTo(frtemp);
            }

            QImage image;
            image = _Mat2QImg(frtemp);

            // 20211207 Jimmy.
            if(isFirstSnap_l){
                if(dm::log){
                    qDebug() << "------------------------------------------";
                    qDebug() << "[DEBUG] Height: " << (float)image.height();
                    qDebug() << "[DEBUG] Width: " << (float)image.width() ;
                }
                float max_w = 800;
                float max_h = 640;
                float ratio_img =  (float)image.width() / (float)image.height();

                if(max_w / ratio_img > max_h){
                    ui->lbl_ImgL->setFixedHeight(max_h);
                    ui->lbl_ImgL->setFixedWidth((int)(max_h *ratio_img));
                }

                else if(max_h *ratio_img > max_w){
                    ui->lbl_ImgL->setFixedWidth(max_w);
                    ui->lbl_ImgL->setFixedHeight((int)(max_w / ratio_img));
                }
                else{
                    ui->lbl_ImgL->setFixedWidth(max_w);
                    ui->lbl_ImgL->setFixedHeight(max_h);
                }
                isFirstSnap_l = false;
            }

            ui->lbl_ImgL->setPixmap(QPixmap::fromImage(image));
            ui->lbl_ResL->setText(QString("解析度 Resolution: %1 x %2 pixels").arg(frameL.cols).arg(frameL.rows));
        }
        if(!draw_result_img.empty())
             draw_result_img.release();
    }
#ifdef ENABLE_AUTO_ADJUST_BRIGHTNESS
    else if(cameraSettingDialogIsShow)
    {
        cameraSettingDialog->setImageL(frtemp, true);
        frameL = frtemp.clone();
    }
#endif
    else{
        lw << "[ERR] Cameras might be disconnected. "
              "In the matching result can be shown as OK because it may use previous image(frameL). " << lw.endl;
        SystemLog("# Please check parameters below: #");
        SystemLog(QString("[RSLT] func_mode: %1").arg(func_mode));
        SystemLog(QString("[RSLT] isAlignmentCheck: %1").arg(isAlignmentCheck));
        SystemLog(QString("[RSLT] isAlignmentOK: %1").arg(isAlignmentOK));
        SystemLog(QString("[RSLT] doAlignRepeat: %1").arg(doAlignRepeat));
    }
    img.release();  //garly modify, for test memory problem
    // Jang 20220411
    emit signalCaptureReadyLeft();
}

void AlignSys::UpdateFrameR(cv::Mat img)
{
    /*
     * Goal:    Update new frame of right camera FOV.
     * Input:   Image we update
     * Output:  None
     */

    // Init status
    // Jang 20220630
    isUpdateImgR = true;
    isCamStop = false;

    if (!isUpdateImgR) isUpdateImgR = true;
    cv::Mat frtemp = img.clone();

    // Barcode image showing
#ifdef BARCODE_READER
    if (isBarcodeDialogShown) {
        if (barcodeDialog->GetDetCamIndex()) {     // Right cam
            QImage qimg = _Mat2QImg(frtemp);
            emit signalBarcodeImage(qimg);
        }
    }
#endif
    //imageprocSettingDialog->PreprocImage(frtemp, frtemp);

#ifdef ORIGINAL_SHAPE_ALIGN
    if(shapeSettingDialogIsShow){
        shapeSettingDialog->setImageR(frtemp);
        frameR = frtemp.clone();
    }
#endif

    if (!frtemp.empty() && !isCamStop) {
#ifdef ENABLE_AUTO_ADJUST_BRIGHTNESS
        if(is_running_bright_adjust){
            cameraSettingDialog->setImageR(frtemp, false);
        }
#endif
        frameR = frtemp.clone();

        // Update the image size
        if (isDoneCalib)
            ui->lbl_CoorTR2->setText(QString("(%1, %2)⬊")
                                     .arg(QString::number(frameR.cols - 1 + align.GetOrigin2().x, 'd', 1))
                                     .arg(QString::number(frameR.rows - 1 + align.GetOrigin2().y, 'd', 1)));
        else
            ui->lbl_CoorTR2->setText(QString("(%1, %2)⬊").arg(frameR.cols - 1).arg(frameR.rows - 1));

        if (updateRefInfoR)
        {
            x22_std = align.GetPosRef2().x;
            y22_std = frameR.rows - align.GetPosRef2().y;
            updateRefInfoR = false;
        }

//        frtemp = img.clone();

        // Draw calibration cross-mark positions
        if (func_mode == _MODE_CALIBRATION_ && !cal_pos.empty())
        {
            for (size_t i=0; i<cal_pos.size(); i++){
                if ((i%2)){
                    Coor pos = cal_pos.at(i);
                    int x_calcross = int(pos.x);
                    int y_calcross = (frameR.rows - int(pos.y));
                    // Draw cross-mark
                    cv::line(frtemp,cv::Point(x_calcross-20,y_calcross),cv::Point(x_calcross+20,y_calcross),cv::Scalar(0,255,0),3);
                    cv::line(frtemp,cv::Point(x_calcross,y_calcross-20),cv::Point(x_calcross,y_calcross+20),cv::Scalar(0,255,0),3);
                    // Draw number
                    cv::putText(frtemp, std::to_string(i/2+1), cv::Point(x_calcross - 22, y_calcross + 18),
                                cv::FONT_HERSHEY_DUPLEX, 0.5, cv::Scalar(0, 255, 0), 1);
                }
            }
            if (isDoneCalib){
                cv::Point center = cv::Point(pc2.x, (-pc2.y + frameR.rows));
                int r = (int)align.GetPcRadius2();
                // Jang 20220517 // the circle size changed to be (r, r) from (r+20, r+20)
                cv::ellipse(frtemp, center, cv::Size(r, r), 0, 0, 360, cv::Scalar(255, 255, 255), 1);
                cv::line(frtemp, cv::Point(cal_pos.at(1).x, frameR.rows - cal_pos.at(1).y),
                         center, cv::Scalar(0, 255, 255), 2);
            }
        }
        else
        {
            if ((isCreatetemp == false) && (refPtExist == true) && x22_std > 0 && y22_std > 0)
            {
                int x_cross1 = x22_std; //frame2.cols / 2;//(ui->lbdisplay->width() / 2);
                int y_corss1 = y22_std; //frame2.rows / 2;//(ui->lbdisplay->height() / 2);

                cv::line(frtemp,cv::Point(x_cross1-20,y_corss1),cv::Point(x_cross1+20,y_corss1),cv::Scalar(0,0,255),3);
                cv::line(frtemp,cv::Point(x_cross1,y_corss1-20),cv::Point(x_cross1,y_corss1+20),cv::Scalar(0,0,255),3);

                if ((markerSettingDialogIsShow) && (currentMode == _MODE_MARKER_ || currentMode == _MODE_CHAMFER_))
                    markerSettingDialog->setRefPointCoordinate(QString("參考點 Ref 1: (%1, %2)").arg(x11_std).arg((frameL.rows-y11_std)),
                                                               QString("參考點 Ref 2: (%1, %2)").arg(x22_std).arg((frameR.rows-y22_std)));
            #ifdef ORIGINAL_SHAPE_ALIGN
                if ((shapeSettingDialogIsShow) && (currentMode == _MODE_SHAPE_ONE_WAY_))
                    shapeSettingDialog->setRefpt(QString("參考點 Ref 1: (%1, %2)").arg(x11_std).arg((frameL.rows-y11_std)),
                                                 QString("參考點 Ref 2: (%1, %2)").arg(x22_std).arg((frameR.rows-y22_std)));
            #endif
                ui->lbl_Ref1->setText(QString("參考點 Ref 1: (%1, %2)").arg(x11_std).arg((frameL.rows-y11_std)));
                ui->lbl_Ref2->setText(QString("參考點 Ref 2: (%1, %2)").arg(x22_std).arg((frameR.rows-y22_std)));
            }

            if(showGuideline){
                cv::line(frtemp, cv::Point(0, frameR.rows / 2), cv::Point(frameR.cols, frameR.rows / 2), cv::Scalar(255,0,255), 2);
                cv::line(frtemp, cv::Point(frameR.cols / 2, 0), cv::Point(frameR.cols / 2, frameR.rows), cv::Scalar(255,0,255), 2);
            }
        }

        cv::Mat draw_result_img;
        if (isCreatetemp)
        {

        }
        else
        {

            if (func_mode == _MODE_MARKER_DETECT_ || func_mode == _MODE_ALIGNMENT_)
            {
                // Jang 20221020
                if(show_match_result_)
                /*if(isAlignmentCheck || isAlignmentOK || !doAlignRepeat)*/{
                    draw_result_img = DrawResult(frtemp,0, 2);
                    draw_result_img.copyTo(frtemp);
                }
            }

            // Jimmy 20221011, for pre-detection
            if (func_mode == _MODE_PREDETECT_) {
                draw_result_img = DrawResult(frtemp,0, 2);
                draw_result_img.copyTo(frtemp);
            }

            QImage image;
            image = _Mat2QImg(frtemp);

            // 20211207 Jimmy.
            if(isFirstSnap_r){

                float max_w = 800;
                float max_h = 640;
                float ratio_img =  (float)image.width() / (float)image.height();
                if(max_w / ratio_img > max_h){
                    ui->lbl_ImgR->setFixedHeight((int)max_h);
                    ui->lbl_ImgR->setFixedWidth((int)(max_h *ratio_img));
                }
                else if(max_h *ratio_img > max_w){
                    ui->lbl_ImgR->setFixedWidth((int)max_w);
                    ui->lbl_ImgR->setFixedHeight((int)(max_w / ratio_img));
                }
                else{
                    ui->lbl_ImgR->setFixedWidth((int)max_w);
                    ui->lbl_ImgR->setFixedHeight((int)max_h);
                }
                isFirstSnap_r = false;
            }

            ui->lbl_ImgR->setPixmap(QPixmap::fromImage(image));
            ui->lbl_ResR->setText(QString("解析度 Resolution: %1 x %2 pixels").arg(frameR.cols).arg(frameR.rows));
        }

//        frtemp.release();   //garly modify, for test memory problem
        if(!draw_result_img.empty()){ draw_result_img.release();}
    }
#ifdef ENABLE_AUTO_ADJUST_BRIGHTNESS
    else if (cameraSettingDialogIsShow){
        cameraSettingDialog->setImageR(frtemp, true);
        frameR = frtemp.clone();
    }
#endif
    else {
        lw << "[ERR] Cameras might be disconnected. "
              "In the matching result can be shown as OK because it may use previous image(frameR). " << lw.endl;
        SystemLog("# Please check parameters below: #");
        SystemLog(QString("[RSLT] func_mode: %1").arg(func_mode));
        SystemLog(QString("[RSLT] isAlignmentCheck: %1").arg(isAlignmentCheck));
        SystemLog(QString("[RSLT] isAlignmentOK: %1").arg(isAlignmentOK));
        SystemLog(QString("[RSLT] doAlignRepeat: %1").arg(doAlignRepeat));
    }

    img.release();  //garly modify, for test memory problem
    // Jang 20220411
    emit signalCaptureReadyRight();
}

void AlignSys::UpdateRSTtemplate(cv::Mat template_)
{
    // Jang 20220415
    HoldProcessMsg();

//    // Jang 20220307
    if(is_match_available){
        if(is_new_collecting_left && is_new_collecting_right){
            int tmp_mode = markerSettingDialog->getCurrentMode();
            markerSettingDialog->setCurrentMode(_ONLY_TEMPLATE_);
            ClearTemplates();
            markerSettingDialog->setCurrentMode(tmp_mode);
        }
    }

    if(markerSettingDialog->getCurrentMode() == _EACH_TEMPLATE_R_){
        RST_template_R = template_.clone();
    }
    else{
        RST_template = template_.clone();
    }

}

// Jang 20220504
void AlignSys::UpdateTemplates(cv::Mat template_l, cv::Mat template_r)
{
    RST_template = template_l.clone();
    RST_template_R = template_r.clone();
}


void AlignSys::UpdateTemplateMask(cv::Mat template_mask){
    if(markerSettingDialog->getCurrentMode() == _EACH_TEMPLATE_R_){
        this->match_R.SetTemplateMask(template_mask);
    }
    else{
        this->match.SetTemplateMask(template_mask);
    }
}

void AlignSys::UpdateSrcImgMask(cv::Mat src_img_mask){
//    qDebug() << "UpdateSrcImgMask(), src_img_mask.cols: " << src_img_mask.cols;
    if(markerSettingDialog->getCurrentMode() == _EACH_TEMPLATE_R_){
        this->match_R.SetSrcImageMask(src_img_mask);
    }
    else{
        this->match.SetSrcImageMask(src_img_mask);
    }
}

void AlignSys::UpdateSysCamDirectInfo()
{
    // System and Camera Direction Setting. Jimmy. 20220503.
    CamSysParams csp = direct_setting_.GetCameraAndSystemParameters();
    /// Update the relative infomation.
    //move to /system_config. Jimmy 20220715.
    std::string system_setting_path = system_config_path_.toStdString() + "/system_setting.txt";
    // FileIO. Jimmy, 20220504. #fio29
    fio::FileIO sys_set_file(system_setting_path, fio::FileioFormat::RW);
    if (!sys_set_file.isOK()) {
        if (dm::log)
            qDebug() << "[ERR] Cannot open file: "
                     << QString::fromStdString(system_setting_path);
        return;
    }
    // Machine Type
    sys_set_file.ReplaceByKey("machine_type", std::to_string((int)csp.machine_type));
    // Camera Index
    sys_set_file.ReplaceByKey("camera_l_idx", std::to_string(csp.camera_idx_l));
    sys_set_file.ReplaceByKey("camera_r_idx", std::to_string(csp.camera_idx_r));
    // Camera Flipping
    sys_set_file.ReplaceByKey("cam_flip_l", std::to_string((int)csp.cam_flip_l));
    sys_set_file.ReplaceByKey("cam_flip_r", std::to_string((int)csp.cam_flip_r));
    // Camera Rotation
    sys_set_file.ReplaceByKey("cam_rot_l", std::to_string((int)csp.cam_rot_angle_l));
    sys_set_file.ReplaceByKey("cam_rot_r", std::to_string((int)csp.cam_rot_angle_r));

    sys_set_file.close();   // END of editing system_setting.txt

    ////////////////////////////////////////////////////////////
    // FileIO. Jimmy, 20220504. #fio29
    //move to /system_config. Jimmy 20220715.
    std::string plc_setting_path = system_config_path_.toStdString() + "plc_setting.ini";
    fio::FileIO plc_set_file(plc_setting_path, fio::FileioFormat::RW);
    if (!plc_set_file.isOK()) {
        if (dm::log)
            qDebug() << "[ERR] Cannot open file: "
                     << QString::fromStdString(plc_setting_path);
        return;
    }
    plc_set_file.ReplaceByKey("IsPLCSwapXY", std::to_string((int)csp.is_plc_swap_xy));
    plc_set_file.ReplaceByKey("PLCSignX", std::to_string(csp.plc_sign_x));
    plc_set_file.ReplaceByKey("PLCSignY", std::to_string(csp.plc_sign_y));
    plc_set_file.ReplaceByKey("PLCSignTheta", std::to_string(csp.plc_sign_theta));

    plc_set_file.close();   // END of editing system_setting.txt

    emit signalPlcLoadSetting();
}

void AlignSys::ActionOpenCamera()
{
    if (!switch_cam)
    {
        isCamOpenSuccess = true;
        OpenCamera();
        isCamStop = false;
        switch_cam = !switch_cam;
        paramSetting->SetEnabledCameraSetting(true);
        log("[Action] CCD已開啟.");     // open camera
    }
    else
    {
        CloseCamera();
        isCamOpenSuccess = false;
        isCamStop = true;
        switch_cam = !switch_cam;
        paramSetting->SetEnabledCameraSetting(false);
        log("[Action] CCD已關閉.");     // close camera
    }
}

void AlignSys::AutoTurnOnCamera()   // add by garly
{
    if (!switch_cam)
    {
        isCamOpenSuccess = true;
        OpenCamera();
        isCamStop = false;
        switch_cam = !switch_cam;
        log("[Action] CCD已開啟.");     // open camera
        SystemLog("[OK] Camera opened!");
        //ui->pushButton->setText(QString("Turn off camera"));
        //markerSettingDialog->setCameraBtnText(QString("關閉相機"));
    }
}

void AlignSys::OpenImage(QImage* opt_img)
{
    /*
     * Goal:    Open an image
     */
    QFileDialog chooseImgDlg(this);

    QString itemPath = chooseImgDlg.getOpenFileName(this, tr("Open Images"), QDir::currentPath());
    if (itemPath.isEmpty()) {
        //ui->lbl_TempName->setText(chooseImgDlg.directory().absolutePath());   //garly, take it off for move to setting dialog
        markerSettingDialog->setTemplateName(chooseImgDlg.directory().absolutePath());
    }
    else {
        //ui->lbl_TempName->setText("名稱 Name: " + QFileInfo(itemPath).fileName()); //garly, take it off for move to setting dialog
        markerSettingDialog->setTemplateName("名稱 Name: " + QFileInfo(itemPath).fileName());
    }

    LoadImage(opt_img, itemPath);
}

bool AlignSys::LoadImage(QImage* opt_img, const QString & filename)
{
    /*
     * Goal:    To load an image.
     * Input:   Output Image container; image path
     * Output:  Null (in input)
     */
    QImageReader reader(filename);
    reader.setAutoTransform(true);
    QImage new_image = reader.read();

    if (new_image.isNull()) {
        QMessageBox::information(this, QGuiApplication::applicationDisplayName(),
                                 tr("Cannot load %1 %2").arg(QDir::toNativeSeparators(filename), reader.errorString()));
        return false;
    }

    *opt_img = new_image;
    return true;
}

void AlignSys::OpenCamera()
{
    /*
     * Goal: Turn on the camera.
     */
    isCamStop = false;

    try{
        cam_model = _CAMERA_SDK_::BASLER_PYLON;

        mutex_shared_l_ = new QMutex();
        mutex_shared_r_ = new QMutex();
        camL->mutex_shared = mutex_shared_l_;
        camR->mutex_shared = mutex_shared_r_;

        camL->SetCameraModel(cam_model);
        camR->SetCameraModel(cam_model);

        if(cam_model == _CAMERA_SDK_::BASLER_PYLON){

        }
        else if(cam_model == _CAMERA_SDK_::FLIR_SPINNAKER)
        {
#ifdef SPINNAKER_CAM_API
            spin_cam = new SpinnakerCamera();
            // Camera parameter settings
            CamParams l_cam_params;
            CamParams r_cam_params;
            if(!LoadCameraParams(l_cam_params, r_cam_params))
            {
                std::string l_cam_id = "21085917";
                std::string r_cam_id = "19269995";

                // Camera parameter settings
                l_cam_params.buffer_mode_ = bufferMode::NEWEST_ONLY;
                l_cam_params.cam_pos_ = cameraPosition::LEFT;
                l_cam_params.exposure_ = 1000;
                l_cam_params.frame_rate_ = 30;
                l_cam_params.gain_ = 0;
                l_cam_params.packet_size_ = 0;
                l_cam_params.throughput_limit_ = 60000000;
                l_cam_params.trigger_type_ = triggerType::OFF;
                l_cam_params.width_ = 900;
                l_cam_params.height_ = 900;
                l_cam_params.offset_x_ = 748;
                l_cam_params.offset_y_ = 560;

                l_cam_params.id_ = l_cam_id;

                r_cam_params = l_cam_params;
                r_cam_params.id_ = r_cam_id;
                r_cam_params.cam_pos_ = cameraPosition::RIGHT;
                r_cam_params.width_ = 900;
                r_cam_params.height_ = 900;
                r_cam_params.offset_x_ = 888;
                r_cam_params.offset_y_ = 500;

                SaveCameraParams(l_cam_params, r_cam_params);
            }


            spin_cam->Init("./");
            int detected_cams = spin_cam->InitCameras(l_cam_params, r_cam_params);

            cv::Mat tmp_img_0, tmp_img_1;
            spin_cam->GetLMats(tmp_img_0);
            spin_cam->GetRMats(tmp_img_1);

            camL->SetSpinnakerPtr(spin_cam);
            camR->SetSpinnakerPtr(spin_cam);
#endif
        }

        camL->Play();
        camR->Play();

        isCamOpenSuccess = true;
    }
    catch (cv::Exception e) {
        SystemLog(QString("[ERR] Open camera error: %1").arg(e.what()));
    }
}

void AlignSys::StopCamera()
{
    /*
     * Goal: Stop the camera and take a picture
     */
    try
    {
        camL->Stop();
        camR->Stop();
        isCamStop = true;
    }
    catch (cv::Exception e) {
        SystemLog(QString("[ERR] Stop camera error: %1").arg(e.what()));
    }
}

void AlignSys::CloseCamera()
{
    /*
     * Goal: Close the cam
     */
    try{
        camL->Stop();
        camR->Stop();
        isCamStop = true;
        isCamOpenSuccess = false;
        isUpdateImgL = false;
        isUpdateImgR = false;
        QPixmap w = QPixmap(QSize(600, 400));
        w.fill(Qt::white);
        ui->lbl_ImgL->setPixmap(w);
        ui->lbl_ImgR->setPixmap(w);
    }
    catch (cv::Exception e) {
        SystemLog(QString("[ERR] Camera close error: %1").arg(e.what()));
    }
}



void AlignSys::on_btn_markerSetting_clicked()
{
    ol::WriteLog("[BUTTON] MatchingSetting(RST, Chamfer) clicked.");
    if(*authority_ == _AUTH_ENGINEER_ || *authority_ == _AUTH_OPERATOR_){
        if(!camL->IsConnected() || !camR->IsConnected()){
            WarningMsg("警告 ( Warning )", "請先確認攝影機是否已開啟，並重新啟動！\n"
                                  "Please check camera connections and reboot the system.", 1);
            return;
        }
        if(*authority_ == _AUTH_OPERATOR_){
            WarningMsg("警告 ( Warning )", "您無權限執行此功能！\n請點擊左下角按鈕切換權限。", 1);
            //WarningMsg("警告 ( Warning )", "It is not allowed to control the function. "
            //                      "Please click the button(Bottom-Left) and change the authority.", 1);
            return;
        }
    }

    if(isRunCalib){
        WarningMsg("警告 ( Warning )", "The Calibration process is running. Changing options may occur critial problems. \n\nPlease DO NOT change any parameters.", 1);
    }
    else if(isRunAlign){
        WarningMsg("警告 ( Warning )", "The Alignment process is running. Changing options may occur critial problems. \n\nPlease DO NOT change any parameters.", 1);
    }

    if (subMenuActive == false)
    {
        SystemLog("[SRT][Marker Setting] ##### Marker Setting Start #####");
        left_template_collected = 0;
        right_template_collected = 0;

        imageprocSettingDialog->SetAdvanvedOption(false);
        if(camL->GetOfflineMode()){
            camL->setSourceMode(_MODE_MARKER_);
            camR->setSourceMode(_MODE_MARKER_);
            usleep(300000);
        }

        SystemLog("[AXN] Marker setting initialize...");
        func_mode = _MODE_NONE_;
        markerSettingDialog->setWindowFlags(Qt::FramelessWindowHint);
        markerSettingDialog->SetDistanceCal(align.GetDistanceCal());
        markerSettingDialog->SetAngCal(align.GetAngCal());
        markerSettingDialog->SetTOL(align.GetTOL());
        markerSettingDialog->SetAlignRepeat(sys_set.align_repeat);
        markerSettingDialog->SetEnableMask(sys_set.enable_mask);
        markerSettingDialog->SetCalibDistTolerance(sys_set.calib_dist_diff_tolerance);
        if(sys_set.align_type == _DO_ALIGN_) markerSettingDialog->EnableTol(true);
        else markerSettingDialog->EnableTol(false);

        markerSettingDialog->InitUI();

        markerSettingDialog->setImageR(frameR);
        markerSettingDialog->setImageL(frameL); // camL->GetCurImg()
        markerSettingDialog->setCurrentRecipeName(currentRecipeName);

        SetBtnStyle("background-color: rgb(253, 185, 62);"
                    "color: rgb(46, 52, 54);",
                    ui->btn_markerSetting);

        SystemLog("[AXN] Change to last-recipe: " + currentRecipeName);

        if(currentMode == _MODE_IDLE_){
            UpdateCurrentMatchMode(_MODE_MARKER_);

            markerSettingDialog->SetMode(currentMode);
            markerSettingDialog->setCurrentMode(_EACH_TEMPLATE_L_);

            cv::Mat empty_mat;
            markerSettingDialog->WriteLastTemplate("", empty_mat, _EACH_TEMPLATE_L_, false);
        }
        else{

            markerSettingDialog->setCurrentMode(_EACH_TEMPLATE_L_);
            currentMode = markerSettingDialog->GetMode();

            SaveMatchModeStatus();
        }

        if (LoadSystemConfig())
            LoadParameters(currentRecipeName);
        WriteSystemConfig();

        markerSettingDialog->LoadPreviousTemplate();    // Jang 20220504
        markerSettingDialog->show();
        markerSettingDialogIsShow = true;
        subMenuActive = true;

        SetMainMenuBtnEnable(false);
    }
}

void AlignSys::on_btn_Detect_clicked()
{
    if (subMenuActive == false)
    {
        SystemLog("[AXN] Marker Detect ...");
        ActionMarkerDetect();
    }
}

void AlignSys::on_btn_changeRecipeNum_clicked()
{
    ol::WriteLog("[BUTTON] Chaging Recipe is clicked.");
    if(*authority_ == _AUTH_OPERATOR_){
        WarningMsg("警告 ( Warning )", "您無權限執行此功能！\n請點擊左下角按鈕切換權限。", 1);
        return;
    }

    if (subMenuActive == false)
    {
        SystemLog("[AXN] ##### Recipe Changing #####");
        recipeChangeDialog = new RecipeChange(this);
        recipeChangeDialog->setWindowFlags(Qt::FramelessWindowHint);
        recipeChangeDialog->SetProjectPath(this->root_path_);
        recipeChangeDialog->SetCurrentRecipeName(recipe_dir);
        recipeChangeDialog->FindFile(this->root_path_ + "Data/recipe/");

        QObject::connect(recipeChangeDialog, SIGNAL(signalRecipeNumber(QString, int)), this, SLOT(ChangeRecipe(QString, int)));
        QObject::connect(recipeChangeDialog, SIGNAL(signalCloseDialog()), this, SLOT(CloseRecipeDialog()));

        recipeChangeDialog->show();
        subMenuActive = true;
    }
}

void AlignSys::CloseRecipeDialog()
{
    subMenuActive = false;
}

void AlignSys::closeEvent(QCloseEvent *event)
{
    QMessageBox::StandardButton button;
    button = QMessageBox::question(this, tr("退出程式"),
                                   QString("是否退出程式？"),
                                   QMessageBox::Yes | QMessageBox::No);

    if (button == QMessageBox::Yes)
    {
        if (!isCamStop)
        {
            StopCamera();
            isCamStop = true;
        }

        event->accept();
    }
    else
    {
        event->ignore();
    }
}

void AlignSys::on_btn_exit_clicked()
{
    QMessageBox::StandardButton button;
    button = QMessageBox::question(this, tr("退出程式"),
                                   QString("是否退出程式？"),
                                   QMessageBox::Yes | QMessageBox::No);

    if (button == QMessageBox::Yes) {
        if (!isCamStop) {
            StopCamera();
            isCamStop = true;
        }
        log("[AXN] 正在關閉程式...");
        RecordLastModifiedTime();   // Closed recipe time. Jimmy 20220518.
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        QApplication* app;
        app->quit();
    }
    ol::WriteLog("[BUTTON] Exit clicked.");
}


/*************************************************
 * UI
 *************************************************/

void AlignSys::ResultStatus(const QString &str)
{
    if (str == "OK")
    {
        ui->label_resultStatus->setText(QString("<font color=green>%1</font>").arg(str));
    }
    else if (str == "NG")
    {
        ui->label_resultStatus->setText(QString("<font color=red>%1</font>").arg(str));
    }
    else if (str == "Busy")
    {
        ui->label_resultStatus->setText(QString("<font color=yellow>%1</font>").arg(str));
    }
    else
    {
        ui->label_resultStatus->setText(QString("<font color=#f5f5f5>%1</font>").arg(str));
    }
}

void AlignSys::ActionRefreshUI()
{
    InitializeUI();
    OpenCamera();

#ifndef MC_PROTOCOL_PLC
    ControlPLC(com, "Zero", nullptr);
#endif
    ui->textEdit_log->setText("");
}

void AlignSys::ActionClearLogMsg()
{
    ui->textEdit_log->setText("");
}

void AlignSys::WarningMsg(const QString &title, const QString &str, uint8_t button_type)
{
    if (msgDialogIsShow)
    {
        msgDialog->closeMsgDialog();
        msgDialogIsShow = false;
    }

    msgDialog->setWindowFlags(Qt::FramelessWindowHint);
    msgDialog->InitUI();
    msgDialog->setTitle(title);
    msgDialog->setButtonsType(button_type);   // only "ok"
    msgDialog->setFontSize(9);
    msgDialog->setMsg(str);
    msgDialog->show();

    msgDialogIsShow = true;
}

// Jang 20220415
void AlignSys::HoldProcessMsg()
{
    if (msgDialogIsShow)
    {
        msgDialog->closeMsgDialog();
        msgDialogIsShow = false;
    }

    msgDialog->setWindowFlags(Qt::FramelessWindowHint);
    msgDialog->InitUI();
    msgDialog->setAnimated();
    msgDialog->setFontSize(15);
    msgDialog->setTitle("執行中（Processing）");
    msgDialog->setMsg("請稍後...\nProcessing...");
    msgDialog->setButtonsType(0);
    msgDialog->show();  // Show progress message

    msgDialogIsShow = true;
}

void AlignSys::CloseProcessMsg(){
    msgDialog->closeMsgDialog();
    msgDialogIsShow = false;
}

cv::Mat AlignSys::DrawResult(const cv::Mat &img, int state, uint32_t c)
{
    /*
     * Goal:    Draw result image
     * Input:   Which image you wanna draw on; and state
     * Output:  Result images
     */

    int* buf = nullptr;
    int* buf_size = nullptr;
    if (c == 1){
        buf = buf_1;
        buf_size = &buf_size_1;
    }
    else if (c == 2){
        buf = buf_2;
        buf_size = &buf_size_2;
    }

    cv::Mat img_result_show = cv::Mat::zeros(cv::Size(img.cols,img.rows),CV_8UC3);
    img.copyTo(img_result_show);


#ifdef ENABLE_SHAPE_ALIGNMENT
    /// JANG Modification
    if (currentMode == _MODE_SHAPE_ONE_WAY_ || currentMode == _MODE_SHAPE_ROI_)
    {
        if(c == 1)
        {
            cv::line(img_result_show, shape_align_0.li_0.pt_0, shape_align_0.li_0.pt_1, cv::Scalar(0, 255, 0), 3, 8, 0);
            cv::line(img_result_show, shape_align_0.li_1.pt_0, shape_align_0.li_1.pt_1, cv::Scalar(0, 255, 0), 3, 8, 0);
            cv::line(img_result_show, cv::Point(shape_align_0.li_0.intersect_x, shape_align_0.li_0.intersect_y),
                     cv::Point(shape_align_0.li_0.intersect_x, shape_align_0.li_0.intersect_y),
                     cv::Scalar(0, 0, 255), 10, 8, 0);
        }
        else
        {
            cv::line(img_result_show, shape_align_1.li_0.pt_0, shape_align_1.li_0.pt_1, cv::Scalar(0, 255, 0), 3, 8, 0);
            cv::line(img_result_show, shape_align_1.li_1.pt_0, shape_align_1.li_1.pt_1, cv::Scalar(0, 255, 0), 3, 8, 0);
            cv::line(img_result_show, cv::Point(shape_align_1.li_0.intersect_x, shape_align_1.li_0.intersect_y),
                     cv::Point(shape_align_1.li_0.intersect_x, shape_align_1.li_0.intersect_y),
                     cv::Scalar(0, 0, 255), 10, 8, 0);
        }

    }
    else
#endif
    {
        if(buf[0]!=1234567 || *buf_size > 0 ){
            int count = *buf_size / 12;
            int a = 0;
            a=state*12;
            for(int i=state;i!=count;i++)
            {
                cv::line(img_result_show, cv::Point(buf[a]-20, buf[a + 1]), cv::Point(buf[a]+20, buf[a + 1]), cv::Scalar(0, 0, 255), 2);
                cv::line(img_result_show, cv::Point(buf[a], buf[a + 1]-20), cv::Point(buf[a], buf[a + 1]+20), cv::Scalar(0, 0, 255), 2);

                cv::line(img_result_show, cv::Point(buf[a], buf[a + 1]), cv::Point(buf[a + 2], buf[a + 3]), cv::Scalar(255, 255, 0), 4.5);
                cv::line(img_result_show, cv::Point(buf[a + 4], buf[a + 5]), cv::Point(buf[a + 6], buf[a + 7]), cv::Scalar(0, 255, 255), 4.5);
                cv::line(img_result_show, cv::Point(buf[a + 8], buf[a + 9]), cv::Point(buf[a + 6], buf[a + 7]), cv::Scalar(0, 255, 255), 4.5);
                cv::line(img_result_show, cv::Point(buf[a + 8], buf[a + 9]), cv::Point(buf[a + 10], buf[a + 11]), cv::Scalar(0, 255, 255), 4.5);
                cv::line(img_result_show, cv::Point(buf[a + 4], buf[a + 5]), cv::Point(buf[a + 10], buf[a + 11]), cv::Scalar(0, 255, 255), 4.5);
                a += 12;
            }
        }
    }
    //garly modify, for test memory problem
    buf = NULL;
    buf_size = NULL;
    return img_result_show;
}

void AlignSys::SetMainMenuBtnEnable(bool enabled, QPushButton* selected_button){
    if(selected_button != nullptr){
        enabled = !enabled;
    }

    ui->btn_markerSetting->setEnabled(enabled);
    //ui->btn_Barcode->setEnabled(enabled);
    ui->btn_systemSetting->setEnabled(enabled);

    ui->btn_changeRecipeNum->setEnabled(enabled);	//garly_20211221, fixed bug
    ui->lab_customer->setEnabled(enabled);          //garly_20211221, add for software version
    ui->checkBox_guideline->setEnabled(enabled);

    ui->btn_alignment->setEnabled(enabled);
    ui->btn_calibration->setEnabled(enabled);

    if(selected_button != nullptr){
        selected_button->setEnabled(!enabled);
    }
}

void AlignSys::SetBtnStyle(QString styleSheet, QPushButton* selected_button){
    QString styleSheet_ = styleSheet;
    if(selected_button != nullptr){
        styleSheet_ = "";
    }
    //ui->btn_LightAdjustment->setStyleSheet(styleSheet_);
    ui->btn_markerSetting->setStyleSheet(styleSheet_);
    //ui->btn_shapeSetting->setStyleSheet(styleSheet_);
    //ui->btn_cameraSetting->setStyleSheet(styleSheet_);
    ui->btn_systemSetting->setStyleSheet(styleSheet_);
    //ui->btn_imageprocSetting->setStyleSheet(styleSheet_); //garly, move into setting dialog

    if(selected_button != nullptr){
        selected_button->setStyleSheet(styleSheet);
    }
}


/*************************************************
 * Engineering mode func
 *************************************************/
void AlignSys::EnableEnginneringMode(bool status)
{
    ui->btn_saveCurrentImage->setVisible(status);
    ui->lineEdit_currentFilename->setVisible(status);
    ui->lbl_CoorTR1->setVisible(status);
    ui->lbl_CoorTR2->setVisible(status);
    ui->lbl_CoorO1->setVisible(status);
    ui->lbl_CoorO2->setVisible(status);
}

void AlignSys::on_btn_saveCurrentImage_clicked()
{
    std::string limg_path = QString("./pic/%1_L.png").arg(ui->lineEdit_currentFilename->text()).toStdString();
    std::string rimg_path = QString("./pic/%1_R.png").arg(ui->lineEdit_currentFilename->text()).toStdString();
    cv::imwrite(limg_path, camL->GetCurImg());
    cv::imwrite(rimg_path, camR->GetCurImg());
}

void AlignSys::OpenDebugDialog()
{
    debugDialog = new DebugDialog(this);  //garly modify, for test memory problem
    debugDialog->setWindowFlags(Qt::FramelessWindowHint);
    SystemLog("[AXN] Open debug dialog...");
    //debugDialog->setWindowFlags(Qt::FramelessWindowHint);
    debugDialog->ClearRsltList();//garly modify, for test memory problem
    debugDialog->setRsltL("");//garly modify, for test memory problem
    debugDialog->setRsltR("");//garly modify, for test memory problem
    debugDialog->show();
}


/*************************************************
 * For the Spinnaker FLIR camera
 * SaveCameraParams()
 * LoadCameraParams()
 *************************************************/
#ifdef SPINNAKER_CAM_API
bool AlignSys::SaveCameraParams(CamParams l_cam_params, CamParams r_cam_params){

    cv::FileStorage cv_fs;

    try {
        if (cv_fs.open(cam_setting_path + "/left_camera.txt", cv::FileStorage::WRITE)) {
            cv_fs << "id" << l_cam_params.id_;
            cv_fs << "cam_pos" << (int)l_cam_params.cam_pos_;
            cv_fs << "buffer_mode" << (int)l_cam_params.buffer_mode_;
            cv_fs << "exposure" << l_cam_params.exposure_;
            cv_fs << "gain" << l_cam_params.gain_;
            cv_fs << "frame_rate" << l_cam_params.frame_rate_;
            cv_fs << "packet_size_" << l_cam_params.packet_size_;
            cv_fs << "throughput_limit" << l_cam_params.throughput_limit_;
            cv_fs << "trigger_type" << (int)l_cam_params.trigger_type_;
            cv_fs << "width" << l_cam_params.width_;
            cv_fs << "height" << l_cam_params.height_;
            cv_fs << "offset_x" << l_cam_params.offset_x_;
            cv_fs << "offset_y" << l_cam_params.offset_y_;
        }
        else {
            SystemLog("The file couldn't be opened: " + QString::fromStdString(cam_setting_path));
            return false;
        }
    }
    catch (cv::Exception e) {
        SystemLog("FileStorage error: " + QString(e.what()));
        return false;
    }
    cv_fs.release();

    try {
        if (cv_fs.open(cam_setting_path + "/right_camera.txt", cv::FileStorage::WRITE)) {
            cv_fs << "id" << r_cam_params.id_;
            cv_fs << "cam_pos" << (int)r_cam_params.cam_pos_;
            cv_fs << "buffer_mode" << (int)r_cam_params.buffer_mode_;
            cv_fs << "exposure" << r_cam_params.exposure_;
            cv_fs << "gain" << r_cam_params.gain_;
            cv_fs << "frame_rate" << r_cam_params.frame_rate_;
            cv_fs << "packet_size_" << r_cam_params.packet_size_;
            cv_fs << "throughput_limit" << r_cam_params.throughput_limit_;
            cv_fs << "trigger_type" << (int)r_cam_params.trigger_type_;
            cv_fs << "width" << r_cam_params.width_;
            cv_fs << "height" << r_cam_params.height_;
            cv_fs << "offset_x" << r_cam_params.offset_x_;
            cv_fs << "offset_y" << r_cam_params.offset_y_;
        }

        else {
            SystemLog("The file couldn't be opened: " + QString::fromStdString(cam_setting_path));
            return false;
        }

    }
    catch (cv::Exception e) {
        SystemLog("FileStorage error: " + QString(e.what()));
        return false;
    }
    cv_fs.release();

    return true;
}

bool AlignSys::LoadCameraParams(CamParams& l_cam_params, CamParams& r_cam_params)
{
    cv::FileStorage cv_fs;
    try
    {
        if (cv_fs.open(cam_setting_path + "/left_camera.txt", cv::FileStorage::READ))
        {
            int int_cam_pos;
            int int_buffer_mode;
            int int_trigger_type;

            cv_fs["id"] >> l_cam_params.id_;
            cv_fs["cam_pos"] >> int_cam_pos;
            cv_fs["buffer_mode"] >> int_buffer_mode;
            cv_fs["exposure"] >> l_cam_params.exposure_;
            cv_fs["gain"] >> l_cam_params.gain_;
            cv_fs["frame_rate"] >> l_cam_params.frame_rate_;
            cv_fs["packet_size_"] >> l_cam_params.packet_size_;
            cv_fs["throughput_limit"] >> l_cam_params.throughput_limit_;
            cv_fs["trigger_type"] >> int_trigger_type;
            cv_fs["width"] >> l_cam_params.width_;
            cv_fs["height"] >> l_cam_params.height_;
            cv_fs["offset_x"] >> l_cam_params.offset_x_;
            cv_fs["offset_y"] >> l_cam_params.offset_y_;

            l_cam_params.cam_pos_ = (cameraPosition)int_cam_pos;
            l_cam_params.buffer_mode_ = (bufferMode)int_buffer_mode;
            l_cam_params.trigger_type_ = (triggerType)int_trigger_type;

        }
        else
        {
            SystemLog("The file couldn't be opened: " + QString::fromStdString(cam_setting_path));
            return false;
        }

    }
    catch (cv::Exception e) {
        SystemLog("FileStorage error: " + QString(e.what()));
        return false;
    }
    cv_fs.release();

    try
    {
        if (cv_fs.open(cam_setting_path + "/right_camera.txt", cv::FileStorage::READ))
        {
            int int_cam_pos;
            int int_buffer_mode;
            int int_trigger_type;

            cv_fs["id"] >> r_cam_params.id_;
            cv_fs["cam_pos"] >> int_cam_pos;
            cv_fs["buffer_mode"] >> int_buffer_mode;
            cv_fs["exposure"] >> r_cam_params.exposure_;
            cv_fs["gain"] >> r_cam_params.gain_;
            cv_fs["frame_rate"] >> r_cam_params.frame_rate_;
            cv_fs["packet_size_"] >> r_cam_params.packet_size_;
            cv_fs["throughput_limit"] >> r_cam_params.throughput_limit_;
            cv_fs["trigger_type"] >> int_trigger_type;
            cv_fs["width"] >> r_cam_params.width_;
            cv_fs["height"] >> r_cam_params.height_;
            cv_fs["offset_x"] >> r_cam_params.offset_x_;
            cv_fs["offset_y"] >> r_cam_params.offset_y_;

            r_cam_params.cam_pos_ = (cameraPosition)int_cam_pos;
            r_cam_params.buffer_mode_ = (bufferMode)int_buffer_mode;
            r_cam_params.trigger_type_ = (triggerType)int_trigger_type;
        }
        else
        {
            SystemLog("The file couldn't be opened: " + QString::fromStdString(cam_setting_path));
            return false;
        }
    }
    catch (cv::Exception e) {
        SystemLog("FileStorage error: " + QString(e.what()));
        return false;
    }
    cv_fs.release();

    return true;

}
#endif

/*************************************************
 * Shape Setting Function
 *************************************************/
#ifdef ORIGINAL_SHAPE_ALIGN
void AlignSys::CloseShapeSettingDialog()
{
    subMenuActive = false;
    shapeSettingDialogIsShow = false;
}

void AlignSys::on_btn_shapeSetting_clicked()
{
    if (subMenuActive == false)
    {
        subMenuActive = true;

//        currentMode = _MODE_SHAPE_ONE_WAY_;
        imageprocSettingDialog->SetAdvanvedOption(true);
        SystemLog("### Start shap alignment setting ###");

//        /// JANG Modification
//        switch (currentMode){
//        case _MODE_SHAPE_ROI_:
//            shape_align_0.SetAlignMethod(ShapeAlignMethod::SELECT_ROI);
//            shape_align_1.SetAlignMethod(ShapeAlignMethod::SELECT_ROI);

//            break;
//        case _MODE_SHAPE_ONE_WAY_:
//            shape_align_0.SetAlignMethod(ShapeAlignMethod::SELECT_LINE_DIRECTION);
//            shape_align_1.SetAlignMethod(ShapeAlignMethod::SELECT_LINE_DIRECTION);
//            break;
//        }

        shape_align_0.SetAlignMethod(ShapeAlignMethod::SELECT_LINE_DIRECTION);
        shape_align_1.SetAlignMethod(ShapeAlignMethod::SELECT_LINE_DIRECTION);

    #ifdef IMAGE_FROM_PICTURE
//        camL->setSourceMode(_MODE_SHAPE_ONE_WAY_);
//        camR->setSourceMode(_MODE_SHAPE_ONE_WAY_);

//        usleep(500000);

    #endif
        func_mode = _MODE_NONE_;
        shapeSettingDialog->setWindowFlags(Qt::FramelessWindowHint);
    //#ifdef IMAGE_FROM_PICTURE
        //shapeSettingDialog->setImageL(cv::imread("./pic/camera_L.bmp"));
        //shapeSettingDialog->setImageR(cv::imread("./pic/camera_R.bmp"));
    //#else
        /// JANG Modification
        shapeSettingDialog->SetShapeAlignment(&this->shape_align_0, &this->shape_align_1);
        shapeSettingDialog->setImageL(frameL);
        shapeSettingDialog->setImageR(frameR);

        // set calib info   //garly
        align.SetDistanceCal(3);
        align.SetAngCal(2.5);
        align.SetTOL(0.5);
        shapeSettingDialog->SetDistanceCal(align.GetDistanceCal());
        shapeSettingDialog->SetAngCal(align.GetAngCal());
        shapeSettingDialog->SetTOL(align.GetTOL());
        shapeSettingDialog->InitUI();

    //#endif

//        ui->btn_shapeSetting->setStyleSheet("background-color: rgb(253, 185, 62);"
//                                             "color: rgb(46, 52, 54);");
//        ui->btn_markerSetting->setStyleSheet("");
//        ui->btn_cameraSetting->setStyleSheet("");

        SetBtnStyle("background-color: rgb(253, 185, 62);"
                    "color: rgb(46, 52, 54);",
                    ui->btn_shapeSetting);



        SystemLog("Auto change recipe");
        //WriteSystemConfig();
//        if (LoadLastRecipe())
//            LoadParameters(currentRecipeName);

        UpdateCurrentMatchMode(_MODE_SHAPE_ONE_WAY_);


        shapeSettingDialog->show();
        shapeSettingDialogIsShow = true;
    }
}
#endif
/*************************************************
 * Camera Setting Function
 * <camerasettingdialog.ui>
 * "Auto adjustment of brightness of the light device"
 *************************************************/
void AlignSys::CloseCameraSettingDialog()
{
#ifdef ENABLE_AUTO_ADJUST_BRIGHTNESS
    subMenuActive = false;
    cameraSettingDialogIsShow = false;
    // Re-connect
//    QObject::connect(camL, SIGNAL(ProcessedImgL(cv::Mat)), this, SLOT(UpdateFrameL(cv::Mat)));
//    QObject::connect(camR, SIGNAL(ProcessedImgR(cv::Mat)), this, SLOT(UpdateFrameR(cv::Mat)));

//    plc_device->Play(); //start plc thread
#endif
}

void AlignSys::on_btn_cameraSetting_clicked()
{
#ifdef ENABLE_AUTO_ADJUST_BRIGHTNESS
    if(cameraSettingDialogIsShow == false)
    {
        func_mode = _MODE_NONE_;
        cameraSettingDialog->setWindowFlags(Qt::FramelessWindowHint);

        /// JANG Modification
        ///
        // Need a thread to send current images
    #if 1
        cameraSettingDialog->setImageL(frameL); // camL->GetCurImg()
        cameraSettingDialog->setImageR(frameR); // camR->GetCurImg()
    #endif

    #if 0
        // TEST
        std::string l_img_path = "./sample/L/l_" + std::to_string(4) + ".bmp";
        std::string r_img_path = "./sample/R/r_" + std::to_string(4) + ".bmp";
        cv::Mat l_img_tmp = cv::imread(l_img_path, cv::IMREAD_COLOR);
        cv::Mat r_img_tmp = cv::imread(r_img_path, cv::IMREAD_COLOR);
        cameraSettingDialog->setImageL(l_img_tmp);
        cameraSettingDialog->setImageR(r_img_tmp);

    #endif

        cameraSettingDialog->InitUI();

//        ui->btn_cameraSetting->setStyleSheet("background-color: rgb(253, 185, 62);"
//                                             "color: rgb(46, 52, 54);");
//        ui->btn_markerSetting->setStyleSheet("");
//        ui->btn_shapeSetting->setStyleSheet("");
//        ui->btn_LightAdjustment->setStyleSheet("");

        SetBtnStyle("background-color: rgb(253, 185, 62);"
                    "color: rgb(46, 52, 54);",
                    ui->btn_cameraSetting);

//        currentMode = _MODE_SHAPE_ONE_WAY_;
//        SystemLog("Auto change recipe");
//        //WriteSystemConfig();
//        if (LoadLastRecipe())
//            LoadParameters(currentRecipeName);

        cameraSettingDialog->show();
        subMenuActive = true;
        cameraSettingDialogIsShow = true;

    // disconnect
//        camL->Stop(); // Make CPU usage problem
//        camR->Stop();
//        camL->blockSignals(true);
//        camR->blockSignals(true);
//        QObject::disconnect(camL, SIGNAL(ProcessedImgL(cv::Mat)), this, SLOT(UpdateFrameL(cv::Mat)));
//        QObject::disconnect(camR, SIGNAL(ProcessedImgR(cv::Mat)), this, SLOT(UpdateFrameR(cv::Mat)));

        cameraSettingDialog->SetCamThreads(camL, camR);

//        plc_device->Stop(); //stop plc thread
    }
#endif
}

void AlignSys::GetBrightnessResult()
{
#ifdef ENABLE_AUTO_ADJUST_BRIGHTNESS
    is_running_bright_adjust = false;
    cameraSettingDialog->do_obtain_img = false;

    cameraSettingDialog->SaveBrightnessInfo();

    if (msgDialogIsShow)
    {
        msgDialog->closeMsgDialog();
        msgDialogIsShow = false;
    }
    //ui->btn_LightAdjustment->setEnabled(true);
    //ui->btn_markerSetting->setEnabled(true);
    //ui->btn_shapeSetting->setEnabled(true);
    //ui->btn_shapeSetting->setEnabled(true);
    SetMainMenuBtnEnable(true);

    ResultStatus("OK");
    log("[Action] 光源調整已完成.");   // finish light source adjusting
#endif
}

void AlignSys::AutoAdjustLightBrightness()
{
#ifdef ENABLE_AUTO_ADJUST_BRIGHTNESS
    bool load_file_success = false;

    SetBtnStyle("background-color: rgb(253, 185, 62);"
                "color: rgb(46, 52, 54);",
                ui->btn_markerSetting);
    SetMainMenuBtnEnable(false);

    ResultStatus("Busy");
    ui->lbl_Mode->setText("Adjust light");
    log("[Action] 光源調整進行中...");     // adjusting the light source

    if (msgDialogIsShow)
    {
        msgDialog->closeMsgDialog();
        msgDialogIsShow = false;
    }

    msgDialog->setWindowFlags(Qt::FramelessWindowHint);
    msgDialog->InitUI();
    msgDialog->setButtonsType(0);   // no button
    msgDialog->setAnimated("Message", "Processing");
    msgDialog->show();
    msgDialogIsShow = true;

    // Load ROI
    cv::FileStorage cv_fs;
    try
    {
        if (cv_fs.open(brightness_setting_path + "/brightness_roi_cam_L.txt", cv::FileStorage::READ)) {
            cv_fs["roi_rect"] >>  roi_rect_;
            load_file_success = true;
        }
        else {
            std::cout << "The file cannot be opened: " << brightness_setting_path + "/brightness_roi_cam_L.txt" << std::endl;
            std::cout << "Please create a template first." << std::endl;
            load_file_success = false;
            //return;
        }
    }
    catch (cv::Exception e) {
        std::cout << "FileStorage error: " << e.what() << std::endl;
        load_file_success = false;
    }
    cv_fs.release();

    if (load_file_success)
    {
        param_L->ROI_lefttop_pos = roi_rect_.tl();
        param_L->ROI_rightbuttom_pos = roi_rect_.br();

        // Run the thread
        // Start the Thread
        is_running_bright_adjust = true;

        cameraSettingDialog->do_obtain_img = true;
        adjust_bright->Play(param_L);
    }
    else
    {
        WarningMsg("Warning Message", "找不到ROI資訊, 請重新圈選標記！", 1);

        ui->btn_markerSetting->setStyleSheet("background-color: rgb(253, 185, 62);"
                                             "color: rgb(46, 52, 54);");
        ui->btn_shapeSetting->setStyleSheet("");
        ui->btn_cameraSetting->setStyleSheet("");

        ui->btn_markerSetting->setEnabled(true);
        ui->btn_shapeSetting->setEnabled(true);
        ui->btn_cameraSetting->setEnabled(true);

        ResultStatus("NG");
        ui->lbl_Mode->setText("IDLE");
        log("[Action] 無法載入光源ROI資訊.");  //"Can't load ROI file!"
    }
#endif
}

void AlignSys::setCamLBrightness(int value)
{
    emit signalSetChannelBrightness((char)opt_channel_idx_L_, (char)value);

    opt_brightness_val_L_ = value;
    SaveOptBrightnessInfo(opt_recipe_path_, 0);
    ui->lbl_brightness_L->setText(QString("亮度 Brightness 1: %1").arg(value));
#ifdef ENABLE_AUTO_ADJUST_BRIGHTNESS
    cameraSettingDialog->ManualAdjustBrightness(value, param_L->brightness_opt_dev_id);
    cameraSettingDialog->SaveBrightnessInfo();
#endif
}

void AlignSys::setCamRBrightness(int value)
{
    emit signalSetChannelBrightness((char)opt_channel_idx_R_, (char)value);

    opt_brightness_val_R_ = value;
    SaveOptBrightnessInfo(opt_recipe_path_, 1);
    ui->lbl_brightness_R->setText(QString("亮度 Brightness 2: %1").arg(value));
#ifdef ENABLE_AUTO_ADJUST_BRIGHTNESS
    cameraSettingDialog->ManualAdjustBrightness(value, param_R->brightness_opt_dev_id);
    cameraSettingDialog->SaveBrightnessInfo();
#endif
}

bool AlignSys::SaveOptBrightnessInfo(const std::string& recipe_path, bool isCamR)
{
    std::string file_name = "";
    int* ptr_brightness_val = nullptr;

    if (!isCamR) {  // for camL brightness setting.
        file_name = "/brightness_cam_L.txt";
        ptr_brightness_val = &opt_brightness_val_L_;
    } else {        // for camR brightness setting.
        file_name = "/brightness_cam_R.txt";
        ptr_brightness_val = &opt_brightness_val_R_;
    }

    // FileIO. 20220420. Jimmy. #fio23
    fio::FileIO fs(recipe_path + file_name, fio::FileioFormat::W);
    if (fs.isOK()) {
        fs.Write("brightness_save", ": ", std::to_string(*ptr_brightness_val));
        fs.close();
        return true;
    } else {
        if (dm::log) std::cerr << "[OPT Save Error] The file couldn't be opened: " << recipe_path + file_name
                  << ", line " << __LINE__ << std::endl;
        fs.close();
        return false;
    }
    return false;
}

bool AlignSys::LoadOptBrightnessInfo(const string &recipe_path, bool isCamR)
{
    std::string file_name = "";
    int* ptr_brightness_val = nullptr;
    QLabel* ptr_lbl_brightness = nullptr;
    QString bightness_str = "\0";
    int channel_idx = 0;

    if (!isCamR) {  // for camL brightness setting.
        file_name = "/brightness_cam_L.txt";
        ptr_brightness_val = &opt_brightness_val_L_;
        ptr_lbl_brightness = ui->lbl_brightness_L;
        bightness_str = "亮度 Brightness 1";
        channel_idx = opt_channel_idx_L_;
    } else {        // for camR brightness setting.
        file_name = "/brightness_cam_R.txt";
        ptr_brightness_val = &opt_brightness_val_R_;
        ptr_lbl_brightness = ui->lbl_brightness_R;
        bightness_str = "亮度 Brightness 2";
        channel_idx = opt_channel_idx_R_;
    }

    // FileIO. 20220420. Jimmy. #fio24
    fio::FileIO fs(recipe_path + file_name, fio::FileioFormat::R);
    if (fs.isOK()) {
        *ptr_brightness_val = fs.ReadtoInt("brightness_save", ": ", *ptr_brightness_val);
        ptr_lbl_brightness->setText(QString("%1: %2")
                                    .arg(bightness_str)
                                    .arg(opt_brightness_val_L_));
        fs.close();

        // Fixed the package lost problem. Jimmy 20220627
        usleep(500000);
        if (isOptLightConnected)
            emit signalSetChannelBrightness(channel_idx, *ptr_brightness_val);

        return true;
    } else {
        if (dm::log) std::cerr << "[OPT Load Error] The file couldn't be opened: "
                               << recipe_path + file_name << ", line " << __LINE__ << std::endl;
        fs.close();
        return false;
    }
    return false;
}

void AlignSys::setCamOpenFailed(QString str)
{
    if(dm::log) {
        qDebug() << "[DEBUG] @setCamOpenFailed";
        log(str);
    }
    if (isCamOpenSuccess) {
        log(str);
    }
    isCamOpenSuccess = false;
    paramSetting->SetEnabledCameraSetting(false);
}

void AlignSys::AutoRestartSystem(const QString& str)
{
    if (!is_cam_connection_failed) {
        is_cam_connection_failed = true;

        log(str);
        lw << "[ERR] Camera might disconnection! Prepare to restart the system. ";
        lw << "Error message: " << str.toStdString() << lw.endl;

        if(dm::log) {
            qDebug() << "[DEBUG] Restart the system to fix problem.";
        }
        //QString error_code = str.mid(7, 4); //Get error code from error message
        //qDebug() << ">> Error code: " << error_code;
        //emit signalPlcWriteCurrentErrorCodeNum(error_code);

        WarningMsgDialog* wmsg = new WarningMsgDialog(this, "警告 ( Warning )",
                                    "偵測到相機連線問題，是否重新開機本系統，以排除問題？",
                                    Qt::WA_DeleteOnClose, Qt::FramelessWindowHint);
        QObject::connect(wmsg, &WarningMsgDialog::accepted, [this](){
            log("[Action] 使用者確認重新開機.");
            WriteSettingAndExit();
        });
        QObject::connect(wmsg, &WarningMsgDialog::rejected, [this](){
            log("[Action] 使用者停止重新開機.");
        });
    }
}

void AlignSys::removeCamImage(int cam_index){
    // Release image

//    mutex_reconnect_.lock();
    QImage image;
    // Left
    if(cam_index == 0){
        frameL.release();
        image = _Mat2QImg(frameL);
        ui->lbl_ImgL->setPixmap(QPixmap::fromImage(image));

        isCamStop = true;
        isCamOpenSuccess = false;
        isUpdateImgL = false;
        camL->Stop();
        usleep(100000);
        camL->Play();
    }
    // Right
    else{
        frameR.release();
        image = _Mat2QImg(frameR);
        ui->lbl_ImgR->setPixmap(QPixmap::fromImage(image));

        isCamStop = true;
        isCamOpenSuccess = false;
        isUpdateImgR = false;
        camR->Stop();
        usleep(100000);
        camR->Play();
    }
//    mutex_reconnect_.unlock();
}

//garly_20220120, add for turn on light source when alignment
void AlignSys::lightStatus(uint8_t status)
{
    if (status == _LightTurnOn_){
//        qDebug() << "opt_brightness_val_L_: " << opt_brightness_val_L_;
//        qDebug() << "opt_brightness_val_R_: " << opt_brightness_val_R_;
        emit signalSetChannelBrightness((char)opt_channel_idx_L_, (char)opt_brightness_val_L_/*36*/);
        emit signalSetChannelBrightness((char)opt_channel_idx_R_, (char)opt_brightness_val_R_/*34*/);
//        setCamLBrightness(opt_brightness_val_L_);
//        setCamRBrightness(opt_brightness_val_R_);
#ifdef ENABLE_AUTO_ADJUST_BRIGHTNESS
        emit signalSetChannelBrightness((char)1, (char)cameraSettingDialog->GetCurrentBrightness(1)/*36*/);
        emit signalSetChannelBrightness((char)2, (char)cameraSettingDialog->GetCurrentBrightness(2)/*34*/);
#endif
    }else {
        emit signalSetChannelBrightness((char)1, (char)0);
        emit signalSetChannelBrightness((char)2, (char)0);
//        setCamLBrightness(0);
//        setCamRBrightness(0);
    }

    lightCurrentStatus = status;
}

void AlignSys::on_btn_systemSetting_clicked()
{
    ol::WriteLog("[BUTTON] SystemSetting clicked.");
    systemSettingDialog->SetAuthority(this->authority_);

    if (subMenuActive == false)
    {
        if(*authority_ == _AUTH_OPERATOR_){
            WarningMsg("警告 ( Warning )", "您無權限執行此功能！\n請點擊左下角按鈕切換權限。", 1);
            return;
        }
        if(isRunCalib){
            WarningMsg("警告 ( Warning )", "The Calibration process is running. Changing options may occur critial problems. \n\nPlease DO NOT change any parameters.", 1);
        }
        else if(isRunAlign){
            WarningMsg("警告 ( Warning )", "The Alignment process is running. Changing options may occur critial problems. \n\nPlease DO NOT change any parameters.", 1);
        }

        systemSettingDialog->InitUI();
        systemSettingDialog->SetProjectPath(root_path_, recipe_dir);
        systemSettingDialog->SetSystemSetting(this->sys_set);
        systemSettingDialog->setWindowFlags(Qt::FramelessWindowHint);
#ifdef SYNC_TIME_FROM_PLC
        systemSettingDialog->setSyncTimeMode(syncTimeMode, autoSyncTimeUnit);
#endif
        //systemSettingDialog->move(100, 100);

        systemSettingDialog->setCurrentRecipe(currentRecipeName);
        systemSettingDialog->setCurrentLightTiming(lightSourceEnableTiming);

        SetBtnStyle("background-color: rgb(253, 185, 62);"
                    "color: rgb(46, 52, 54);",
                    ui->btn_systemSetting);

        systemSettingDialog->show();

        SetMainMenuBtnEnable(false);
        subMenuActive = true;
    }
}

void AlignSys::closeSettingDialog(uint8_t light_timing)
{
    SetMainMenuBtnEnable(true);
    subMenuActive = false;

    if (sys_set.cam_title_swap == false)//garly_20230206, cam title swap
    {
        ui->groupBox_Cam1->setTitle("相機1視野 Camera 1 FOV");
        ui->groupBox_Cam2->setTitle("相機2視野 Camera 2 FOV");
    }
    else if (sys_set.cam_title_swap == true)
    {
        ui->groupBox_Cam1->setTitle("相機2視野 Camera 2 FOV");
        ui->groupBox_Cam2->setTitle("相機1視野 Camera 1 FOV");
    }
}

void AlignSys::OpenImageProcessSettingDialog()
{
    if (isCamStop || !isCamOpenSuccess){
        WarningMsgDialog *warningMsg =
                new WarningMsgDialog(this, "警告 ( Warning )",
                                     QString("請先確認攝影機是否已開啟！\n"
                                             "The cameras are not connected."),
                                     Qt::WA_DeleteOnClose, Qt::FramelessWindowHint);
        Q_UNUSED(warningMsg);

        return;
    }
    if(imageprocSettingDialogIsShown == false)
    {
        systemSettingDialog->accept();
        func_mode = _MODE_NONE_;
        imageprocSettingDialog->setWindowFlags(Qt::FramelessWindowHint);

        /// JANG Modification
        // Need a thread to send current images

        imageprocSettingDialog->setImageL(camL->GetCurImg());
        imageprocSettingDialog->setImageR(camR->GetCurImg());
        // Jang 20220628
        imageprocSettingDialog->SetTemplates(RST_template, RST_template_R);

        imageprocSettingDialog->InitUI();
	// Jang 20220411
        imageprocSettingDialog->SetImageProcessing(shape_align_0.GetImageProc(), shape_align_1.GetImageProc());
        imageprocSettingDialog->show();
        subMenuActive = true;
        imageprocSettingDialogIsShown = true;
    }
}

void AlignSys::OpenParameterSettingDialog()
{
    if(parameterSettingDialogIsShown == false)
    {
        if (!isCamOpenSuccess) {
            WarningMsgDialog *wmsg =
                    new WarningMsgDialog(this, "警告 ( Warning )",
                                         "攝影機開啟異常，無法進行攝影機設定！\n"
                                         "Cameras are not connected, please check again!",
                                         Qt::WA_DeleteOnClose, Qt::FramelessWindowHint);
            Q_UNUSED(wmsg);
        }

        if(dm::log){
            qDebug() << "[DEBUG] Func " << __func__ << ", line " << __LINE__;
        }
        paramSetting->setWindowFlags(Qt::FramelessWindowHint);
        if(dm::log){
            qDebug() << "[DEBUG] Func " << __func__ << ", line " << __LINE__;
        }
        paramSetting->move((this->width() - paramSetting->width())/2, (this->height() - paramSetting->height())/2);
        if(dm::log){
            qDebug() << "[DEBUG] Func " << __func__ << ", line " << __LINE__;
        }
        paramSetting->SetSystemSetting(this->sys_set);

        paramSetting->show();
        subMenuActive = true;
        parameterSettingDialogIsShown = true;

        // Jang 20220510
        // Backup
        sys_set_backup = sys_set;
    }

}
// Jang 20220510
void AlignSys::RestoreSystemSetting(){
    SetSystemSetting(sys_set_backup);
    RestartCamera();
}
// Jang 20220511
void AlignSys::RestartCamera(){
    // Camera
    CloseCamera();
    QCoreApplication::processEvents(QEventLoop::AllEvents, 200);    // To release remaining signals
    camL->SelectCam(sys_set.camera_l_idx);
    camR->SelectCam(sys_set.camera_r_idx);
    camL->SetCameraRotation(sys_set.cam_rot_l);
    camR->SetCameraRotation(sys_set.cam_rot_r);
    camL->SetExposureGain(sys_set.cam_exposure, sys_set.cam_gain);
    camR->SetExposureGain(sys_set.cam_exposure, sys_set.cam_gain);
    camL->setVideoFormat(sys_set.crop_l.crop_x, sys_set.crop_l.crop_y, sys_set.crop_l.crop_w, sys_set.crop_l.crop_h, sys_set.cam_flip_l);
    camR->setVideoFormat(sys_set.crop_r.crop_x, sys_set.crop_r.crop_y, sys_set.crop_r.crop_w, sys_set.crop_r.crop_h, sys_set.cam_flip_r);
    OpenCamera();
    QCoreApplication::processEvents(QEventLoop::AllEvents, 200);
    isFirstSnap_l = true;
    isFirstSnap_r = true;
}
// Jang 20220511
void AlignSys::UpdateCamIndex(bool is_0l1r){
    if(is_0l1r == (sys_set.camera_l_idx == 0)){

    }
    else{
        int tmp_cam_rot = sys_set.cam_rot_l;
        sys_set.cam_rot_l = sys_set.cam_rot_r;
        sys_set.cam_rot_r = tmp_cam_rot;
        int tmp_idx = sys_set.camera_l_idx;
        sys_set.camera_l_idx = sys_set.camera_r_idx;
        sys_set.camera_r_idx = tmp_idx;
        CropInfo tmp_crop = sys_set.crop_l;
        sys_set.crop_l = sys_set.crop_r;
        sys_set.crop_r = tmp_crop;
        RestartCamera();
    }
}

void AlignSys::CloseParameterSettingDialog()
{
    subMenuActive = false;
    parameterSettingDialogIsShown = false;
    on_btn_systemSetting_clicked();
    log("[Action] 參數設定已完成.");
}

// Jang 20220411
void AlignSys::OpenOtherSettingDialog()
{
    if(otherSettingDialogIsShown == false)
    {
        otherSettingDialog->setWindowFlags(Qt::FramelessWindowHint);
        otherSettingDialog->move((this->width() - otherSettingDialog->width())/2, (this->height() - otherSettingDialog->height())/2);
        otherSettingDialog->LoadSetting();
        // Jang 20220712
        otherSettingDialog->SetAuthority(this->authority_);

        otherSettingDialog->show();
        subMenuActive = true;
        otherSettingDialogIsShown = true;

        systemSettingDialog->accept();
    }
}
void AlignSys::CloseOtherSettingDialog()
{
    subMenuActive = false;
    otherSettingDialogIsShown = false;
    on_btn_systemSetting_clicked();
}

void AlignSys::OpenCropRoiDialog(int cam_idx)   // Jimmy. 20211223
{
    if(dm::log) {
        qDebug() << "-------------------- " << __func__;
        qDebug() << "[DEBUG] Get into Crop ROI Process";
        qDebug() << "[DEBUG] crop_cam_idx_: " << cam_idx;
        qDebug() << "[DEBUG] camera_l_idx: " << sys_set.camera_l_idx;
        qDebug() << "[DEBUG] camera_r_idx: " << sys_set.camera_r_idx;
        qDebug() << "-------------------- ";
    }
    if (!cropRoiDialogIsShown) {
        crop_cam_idx_ = cam_idx;

        // Restore to full resolution
        CloseCamera();
        if(dm::log){
            qDebug() << "[DEBUG] Func " << __func__ << ", line " << __LINE__;
        }
        camL->setVideoFormat(0, 0, 0, 0, sys_set.cam_flip_l);
        camL->SetCameraRotation(0);
        camR->setVideoFormat(0, 0, 0, 0, sys_set.cam_flip_r);
        camR->SetCameraRotation(0);
        if(dm::log){
            qDebug() << "[DEBUG] Func " << __func__ << ", line " << __LINE__;
        }
        OpenCamera();
        if(dm::log){
            qDebug() << "[DEBUG] Func " << __func__ << ", line " << __LINE__;
        }
//        if (!isCropping)
//            (cam_idx) ?
//                QObject::connect(camR, SIGNAL(signalSendImgOnce(cv::Mat)), this, SLOT(RunCropRoiR(cv::Mat))) :
//                QObject::connect(camL, SIGNAL(signalSendImgOnce(cv::Mat)), this, SLOT(RunCropRoiL(cv::Mat)));
        // Jang 20220511
        if (!isCropping)
            (cam_idx == sys_set.camera_l_idx) ?
                QObject::connect(camL, SIGNAL(signalSendImgOnce(cv::Mat)), this, SLOT(RunCropRoiL(cv::Mat))) :
                QObject::connect(camR, SIGNAL(signalSendImgOnce(cv::Mat)), this, SLOT(RunCropRoiR(cv::Mat)));
    }
}

void AlignSys::CloseCropRoiDialog(CropInfo &crop_info)
{
    if(dm::log) {
        qDebug() << "The CropInfo Result: " << crop_info.do_crop_roi << " " << crop_info.crop_x << " "
                 << crop_info.crop_y << " " << crop_info.crop_w << " " << crop_info.crop_h;
        qDebug() << "crop_cam_idx_: " << crop_cam_idx_;
    }
    paramSetting->UpdateCropInfo(crop_cam_idx_, crop_info);
    isCropping = false;
    cropRoiDialogIsShown = false;

    // disconnect the signal
    QObject::disconnect(camR, SIGNAL(signalSendImgOnce(cv::Mat)), this, SLOT(RunCropRoiR(cv::Mat)));
    QObject::disconnect(camL, SIGNAL(signalSendImgOnce(cv::Mat)), this, SLOT(RunCropRoiL(cv::Mat)));

    // Restore to original resolution
    RestartCamera();    // Jang 20220511

    crop_cam_idx_ = -1;
    cropRoiDialog->close();
}
// Jang 20220511
void AlignSys::CancelCropRoi(){
    if(crop_cam_idx_ == sys_set.camera_l_idx)
        CloseCropRoiDialog(sys_set.crop_l);
    else
        CloseCropRoiDialog(sys_set.crop_r);
}
void AlignSys::RunCropRoiL(cv::Mat full_img_l)
{
    if(dm::log){
        qDebug() << "[DEBUG] Func " << __func__ << ", line " << __LINE__;
    }
    isCropping = true;
    cropRoiDialog->InitUI(full_img_l);
    cropRoiDialog->show();
    cropRoiDialogIsShown = true;
    if(dm::log){
        qDebug() << "[DEBUG] Func " << __func__ << ", line " << __LINE__;
    }
    full_img_l.release();
}

void AlignSys::RunCropRoiR(cv::Mat full_img_r)
{
    isCropping = true;
    cropRoiDialog->InitUI(full_img_r);
    cropRoiDialog->show();
    cropRoiDialogIsShown = true;

    full_img_r.release();
}

void AlignSys::CloseImageProcSettingDialog()
{
    subMenuActive = false;
    imageprocSettingDialogIsShown = false;

    //SetBtnEnable(true);
    //SetBtnStyle("");

    on_btn_systemSetting_clicked();
}

void AlignSys::on_pushButton_clicked()
{
    lightStatus(_LightTurnOff_);
}

void AlignSys::on_btn_Test_clicked()
{

//    lightStatus(_LightTurnOn_);
    while(true){
        QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
        std::string params_path = (root_path_ + "/Data/" + recipe_dir + "/parameters/matching_parameters.ini").toStdString();
        match.LoadParamsIniFile(params_path);
        lw_mem << "Using memory(" << std::to_string((int)getpid()) << "): " << (float)GetCurrentMem() / 1000
               << ", " << SaveMemInfo() << lw_mem.endl;
    }


}

void AlignSys::on_btn_calibration_clicked()
{

    ///home/contrel/AlignmentSys/Data/0006/calibration/4mm_1.5deg

    int num_calib = 11;
    if(sys_set.machine_type == _MACHINE_UVW_) num_calib = 11;
    else num_calib = 7;
    isCamStop = true;
    usleep(100000);
    std::string calib_img_path = "./Data/" + recipe_dir.toStdString() + "/calibration/sample/";
    qDebug() << "[DEBUG] Calib img path: " << QString::fromStdString(calib_img_path);
    for(int i = 0; i < num_calib; i ++){
        // Jang 20220517
        frameL.release();
        frameR.release();
        std::string img_path_l = "cal_" + std::to_string(i) + "_L.png";
        std::string img_path_r = "cal_" + std::to_string(i) + "_R.png";
        frameL = cv::imread(calib_img_path + img_path_l);
        frameR = cv::imread(calib_img_path + img_path_r);
        if(frameL.empty() || frameR.empty()){
            qDebug() << "[DEBUG] func: " << __func__ << ", line " << __LINE__;
            return;
        }
        ActionCalib();
        usleep(100000);
    }
    isCamStop = false;
}

void AlignSys::on_btn_alignment_clicked()
{
    // Add try/catch to handle error (Jimmy. 20211202)
    try {
        isCamStop = true;
        std::string calib_img_path = "./Data/" + recipe_dir.toStdString() + "/calibration/sample/";

        std::string img_path_l = "align_L_first.png";
        std::string img_path_r = "align_R_first.png";
        frameL = cv::imread(calib_img_path + img_path_l);
        frameR = cv::imread(calib_img_path + img_path_r);
        ActionAlign();
        usleep(100000);

        img_path_l = "align_L_second_alined.png";
        img_path_r = "align_R_second_alined.png";
        frameL = cv::imread(calib_img_path + img_path_l);
        frameR = cv::imread(calib_img_path + img_path_r);

        ActionAlign();

        isCamStop = false;

        time_log.SaveElapsedTime("Alignment Finished");
        time_log.FinishRecordingTime("Alignment Finished");
    }catch (cv::Exception e) {
        SystemLog(QString("[Action] Click Align error: %1").arg(e.what()));
        SystemLog(QString("\t@%1> func: %2").arg(__FILE__, __func__));
    }
}

void AlignSys::on_checkBox_guideline_clicked()
{
    ol::WriteLog("[BUTTON] Guideline clicked.");
    if(*authority_ == _AUTH_OPERATOR_){
        ui->checkBox_guideline->setChecked(false);
        showGuideline = false;        
        WarningMsg("警告 ( Warning )", "您無權限執行此功能！\n請點擊左下角按鈕切換權限。", 1);
        return;
    }
    if(isRunCalib){
        WarningMsg("警告 ( Warning )", "校正進行中，請勿更動任何參數或設定！\n"
                                     "The Calibration process is running. Changing options may occur critial problems. \n\n"
                                      "Please DO NOT change any parameters.", 1);
    }
    else if(isRunAlign){
        WarningMsg("警告 ( Warning )", "對位進行中，請勿更動任何參數或設定！\n"
                                     "The Alignment process is running. Changing options may occur critial problems. \n\n"
                                     "Please DO NOT change any parameters.", 1);
    }

    if(ui->checkBox_guideline->isChecked()){
        showGuideline = true;
    }
    else{
        showGuideline = false;
    }
}

void AlignSys::DisconnectCameras(){
    if (!isCamStop)
    {
        camL->Stop();
        camR->Stop();
    }

    if(camL != nullptr){
        delete camL;
        camL = nullptr;
    }

    if(camR != nullptr){
        delete camR;
        camR = nullptr;
    }
}

//garly_20211221, add for change customer/factory
bool AlignSys::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == ui->lab_customer)
    {
        if (event->type() == QEvent::MouseButtonPress)
        {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton)
            {
                //qDebug() << "Label receive left button";
                if (subMenuActive == false)
                {
                    if(*authority_ == _AUTH_OPERATOR_){
                        WarningMsg("警告 ( Warning )", "您無權限執行此功能！\n請點擊左下角按鈕切換權限。", 1);
                        return false;
                    }

                    customerDialog = new changeCustomerDialog(this);
                    customerDialog->setWindowFlags(Qt::FramelessWindowHint);
                    customerDialog->move((this->width() - customerDialog->width())/2, (this->height() - customerDialog->height())/2);
                    QString str = ui->lab_customer->text();

                    customerDialog->setDefaultText(str.split(", ").at(0), str.split(", ").at(1));

                    QObject::connect(customerDialog, SIGNAL(signalCustomerDialogBtnOK(QString, QString)), this, SLOT(updateCustomerLabel(QString, QString)));
                    QObject::connect(customerDialog, SIGNAL(signalCustomerDialogBtnCancel()), this, SLOT(closeChangeCustomerDialog()));

                    customerDialog->show();
                    subMenuActive = true;
                    customerDialogIsShown = true;
                    SetMainMenuBtnEnable(false);

                }
                return true;
            }
            else
                return false;
        }
        else
            return false;
    }
    else if (obj == ui->logo)
    {
        if (event->type() == QEvent::MouseButtonPress)
        {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton)
            {
                //qDebug() << "Label receive left button";
                if (subMenuActive == false)
                {
                    authorityDialog = new changeAuthorityDialog(this);
                    authorityDialog->move((this->width() - authorityDialog->width())/2, (this->height() - authorityDialog->height())/2);
                    authorityDialog->setSource(0);  //open dialog from logo
                    authorityDialog->setCurrentAuth(*authority_);	//garly_20220216

                    QObject::connect(authorityDialog, SIGNAL(signalAuthDialogBtnOK(int)), this, SLOT(ChangeAuthority(int)));
                    QObject::connect(authorityDialog, SIGNAL(signalAuthDialogBtnCancel()), this, SLOT(closeChangeAuthorityDialog()));

                    authorityDialog->show();
                    subMenuActive = true;
                    authorityDialogIsShown = true;
                    SetMainMenuBtnEnable(false);
                }
                return true;
            }
            else
                return false;
        }
        else
            return false;
    } else if (obj == ui->lab_systemVersion) {  // Showing the sub-version code. Jimmy, 20220509.
        if (event->type() == QEvent::MouseButtonPress) {
#if 0   //DEPRECATED: Jimmy. User already know this function.
            QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                ui->lab_systemVersion->setText(QString("v%1: %2").arg(SOFTWARE_VERSION).arg(SUB_VERSION));
                ui->lab_systemVersion->show();
            } else
                return false;
#endif
            return false;
        } else if (event->type() == QEvent::MouseButtonRelease) {
            ui->lab_systemVersion->setText(QString("v%1").arg(SOFTWARE_VERSION));
            ui->lab_systemVersion->show();
        } else
            return false;
    }
    return QWidget::eventFilter(obj, event);
}

void AlignSys::mouseDoubleClickEvent(QMouseEvent *e)
{
    if (e->button() == Qt::RightButton) {
        QPointer<QMessageBox> msgbox = new
        QMessageBox(QMessageBox::Information,
                    QString("About Contrel Alignment System"),
                    QString("\n****** About Contrel Alignment System ******\n\n"
                            "Project Leader: Longhao Lin\n\n"
                            "Author: SoonMyun Jang, Jimmy Shen, Garly Hsu, Apo Hsu\n\n"
                            "Copyright © 2018-2022 The Contrel Ltd. All rights reserved.\n"),
                    QMessageBox::Ok,
                    this);
        QPixmap contrel_img = QPixmap(":/Data/icon/logo_big.png");
        float scale_factor = 0.4;
        msgbox->setIconPixmap(contrel_img.scaled((float)contrel_img.width() * scale_factor,
                                                 (float)contrel_img.height() * scale_factor,
                                                 Qt::KeepAspectRatio));
        msgbox->setStyleSheet("background-color: rgb(247, 245, 243);\ncolor: rgb(0, 0, 0);");
        msgbox->setAttribute(Qt::WA_DeleteOnClose);
        msgbox->setWindowFlags(Qt::FramelessWindowHint);
        msgbox->show();
    }

    // You may have to call the parent's method for other cases
    QWidget::mouseDoubleClickEvent(e);
}


void AlignSys::closeChangeCustomerDialog()
{
    subMenuActive = false;
    customerDialogIsShown = false;
    SetMainMenuBtnEnable(true);
}

void AlignSys::updateCustomerLabel(const QString& str1, const QString& str2)
{
    ui->lab_customer->setText(QString("%1, %2").arg(str1).arg(str2));

    // FileIO. 20220419. Jimmy. #fio08
    fio::FileIO cus_file(QString(system_config_path_ + "customer_label.ini").toStdString(),
                         fio::FileioFormat::W);
    cus_file.Write("CustomerName", str1.toStdString());
    cus_file.Write("StationName", str2.toStdString());
    cus_file.close();

    subMenuActive = false;
    customerDialogIsShown = false;
    SetMainMenuBtnEnable(true);
}


void AlignSys::closeChangeAuthorityDialog()
{
    if(authorityDialogIsShown){
        if(authorityDialog != nullptr)
            delete authorityDialog;
        authorityDialog = nullptr;
    }
    subMenuActive = false;
    authorityDialogIsShown = false;
    SetMainMenuBtnEnable(true);
    if(dm::log) {
        qDebug() << "[DEBUG] Authority Dialog Closed";
    }
}

void AlignSys::on_btn_reboot_clicked()
{
    ol::WriteLog("[BUTTON] Reboot is clicked.");
    if(*authority_ == _AUTH_OPERATOR_){
        WarningMsg("警告 ( Warning )", "您無權限執行此功能！\n請點擊左下角按鈕切換權限。", 1);
        return;
    }
    WarningMsgDialog *warningMsg = new WarningMsgDialog(this);
    warningMsg->setAttribute(Qt::WA_DeleteOnClose);
    warningMsg->setWindowFlags(Qt::FramelessWindowHint);
    warningMsg->move((this->width() - warningMsg->width())/2, (this->height() - warningMsg->height())/2);
    warningMsg->setTitle("警告 ( Warning )");
    warningMsg->setContent("系統將自動重新啟動, 是否繼續？\nThe system will reboot(shutting down and restarting), do you want to continue?");

    QObject::connect(warningMsg, SIGNAL(signalWarningMsgBtnOK()), this, SLOT(WriteSettingAndExit()));
    warningMsg->show();
}


void AlignSys::WriteSettingAndExit()
{
    ol::WriteLog("[SYSTEM] Reboot.");
    RecordLastModifiedTime();   // Closed recipe time. Jimmy 20220518.
    int rt = system("echo 50511881635 | sudo -S reboot");
    Q_UNUSED(rt);
}

//garly_20220111, add for reset all parameters from plc
void AlignSys::ResetAllParameters()
{
    log("[Action] 重新啟動軟體中，請稍候片刻...");
    QCoreApplication::processEvents(QEventLoop::AllEvents);

    //WriteSystemConfig() to set is_reset_CCDsys, Jimmy 20220627
    WriteSystemConfig();
    //emit signalPlcWriteResult(0, 0, 0, 1, PlcWriteMode::kOthers);  //result=> 0: continue; 1: finish; 2: NG
    usleep(100000);
    //FinishPlcCmd();
    emit signalPlcDisconnect(false);
    emit signalOptDisconnect(false);
    usleep(500000);

    StopPlcThread();    // Jimmy. 20220119
    StopOptThread();
    usleep(100000);

    //choose the best one of methods
    //method 1 : reboot system
    //system("reboot");

    //method 2 : reset Alignment System
    qApp->exit(888);

    //method 3 : reset all parameters

}

QString AlignSys::GetMacAddress()
{
    QList<QNetworkInterface> NetList;
    int NetCount = 0;

    NetList = QNetworkInterface::allInterfaces();
    NetCount = NetList.count();
    for (int i = 0; i < NetCount; i++)
    {
        if (NetList[i].humanReadableName() == "eth0")
        {
            //qDebug() << "Name = " << NetList[i].humanReadableName() << ", Mac addr = " << NetList[i].hardwareAddress();
            return NetList[i].hardwareAddress();
        }
    }
    return QString("");
}
// Jang 20220106
void AlignSys::on_btn_cam_offline_clicked()
{
    CloseCamera();

    if (camL->GetOfflineMode()) {
        camL->UseOfflineImg(false);
        camR->UseOfflineImg(false);
    } else {
        camL->UseOfflineImg(true);
        camR->UseOfflineImg(true);

        camL->setOfflineSystemType(this->system_type);
        camR->setOfflineSystemType(this->system_type);

        isCamOpenSuccess = true;
        switch_cam = true;
    }

    paramSetting->SetEnabledCameraSetting(true);

    OpenCamera();

}

void AlignSys::on_btn_restart_clicked()
{
    ol::WriteLog("[SYSTEM] Restart.");
    WarningMsgDialog *wmsg =
            new WarningMsgDialog(this, "警示訊息",
                                 "系統即將重新啟動, 是否繼續？\n"
                                 "The system will restart, do you want to continue?",
                                 Qt::WA_DeleteOnClose,
                                 Qt::FramelessWindowHint);
    wmsg->move((this->width() - wmsg->width())/2, (this->height() - wmsg->height())/2);

    QObject::connect(wmsg, &WarningMsgDialog::accepted, this, [this](){
        ol::WriteLog("[BUTTON] on_btn_restart_clicked - Ok is clicked.");
        RecordLastModifiedTime();   // Record closed time. Jimmy 20220518.
        is_reset_CCDsys_ = true;
        ResetAllParameters();
    });
    QObject::connect(wmsg, &WarningMsgDialog::rejected, this, [this](){
        ol::WriteLog("[BUTTON] on_btn_restart_clicked - Cancel is clicked.");
    });

    wmsg->show();
}


void AlignSys::on_btn_auth_Operator_clicked()
{
    ol::WriteLog("[BUTTON] Autority Operator is clicked.");
    if (subMenuActive == false)
    {
        ChangeAuthority(_AUTH_OPERATOR_);
    }
}

void AlignSys::on_btn_auth_Engineer_clicked()
{
    ol::WriteLog("[BUTTON] Autority Engineer is clicked.");
    if (subMenuActive == false)
    {
        authorityDialog = new changeAuthorityDialog(this);
        authorityDialog->move((this->width() - authorityDialog->width())/2, (this->height() - authorityDialog->height())/2);
        authorityDialog->setSource(1);  //open dialog from button
        authorityDialog->setCurrentAuth(*authority_);	//garly_20220216

        QObject::connect(authorityDialog, SIGNAL(signalAuthDialogBtnOK(int)), this, SLOT(ChangeAuthority(int)));
        QObject::connect(authorityDialog, SIGNAL(signalAuthDialogBtnCancel()), this, SLOT(closeChangeAuthorityDialog()));

        authorityDialog->show();
        subMenuActive = true;
        authorityDialogIsShown = true;
        SetMainMenuBtnEnable(false);
    }
}


// Jang 20220411
void AlignSys::SetAlignOption(bool set_align_center){
    if(set_align_center){
        is_align_valid_method = _ALIGN_VALID_REF_CENTER_;
//        qDebug() << "[DEBUG] Align Option: _ALIGN_VALID_REF_CENTER_" ;
    }
    else{
        is_align_valid_method = _ALIGN_VALID_TWO_REF_PTS_;
//        qDebug() << "[DEBUG] Align Option: _ALIGN_VALID_TWO_REF_PTS_" ;
    }
}

// Jang 20220712
// Set parameters to manage system space
void AlignSys::SetSpaceManagementParams(int target_free_space, int maintain_days_txt, int maintain_days_img,
                                        int check_space_interval, bool check_space_after_align){
    if(dm::log){
        qDebug() << "===========================================";
        qDebug() << "[DEBUG] SetSpaceManagementParams.";
    }

    if(target_free_space > 0) target_space_gb_ = target_free_space;
    if(maintain_days_txt > 0) min_maintain_days_txt_ = maintain_days_txt;
    if(maintain_days_img > 0) min_maintain_days_img_ = maintain_days_img;
    if(check_space_interval > 0) check_space_interval_ = check_space_interval;
    check_space_after_align_ = check_space_after_align;

    TimerAutoDeleteLog();
}

//garly_20220224, add for time setting
#ifdef SYNC_TIME_FROM_PLC
void AlignSys::UpdateAdjustTimeSetting(bool state, uint8_t syncTimeIndex)
{
    syncTimeMode = state;
    autoSyncTimeUnit = syncTimeIndex;
    emit signalPlcSetAdjustTimeMode(syncTimeMode, autoSyncTimeUnit);
}
#endif

void AlignSys::UpdateCurrentTime()
{
    ui->lab_currentTime->setText(QDateTime::currentDateTime().toString("yyyy/MM/dd hh:mm:ss"));
}

void AlignSys::ConfigLightSourceNetParm(QString ip, int port, bool isModiy)
{
    this->sys_set.opt_light_IP = ip.toStdString();
    this->sys_set.opt_light_port = port;

    if (isModiy)
    {
        emit signalSetNetworkParameter(ip, "255.255.255.0", QString("%1").arg(port));
        log("[Action] 變更光源IP位址！");
    }
    SetSystemSetting(this->sys_set);
}

void AlignSys::UpdateLightSrcEnableTiming(uint8_t light_timing)
{
    lightSourceEnableTiming = light_timing;

    if (lightSourceEnableTiming == _ALWAYS_OPEN_) {
        lightStatus(_LightTurnOn_);
    } else {
        lightStatus(_LightTurnOff_);
    }

//    // Fix set opt brightness problem. Jimmy 20220412
//    if (LoadOptBrightnessInfo(opt_recipe_path_, 0) && LoadOptBrightnessInfo(opt_recipe_path_, 1)) {
//        systemSettingDialog->SetBrightnessVal(opt_brightness_val_L_, opt_brightness_val_R_);
//    }
}

#ifdef BARCODE_READER
void AlignSys::ActionBarcodeReader()
{
    ui->lbl_Mode->setText("Barcode");
    if(dm::log) {
        qDebug() << "=============== Do barcode func =================";
    }

    try {
        if (!isBarcodeDialogShown) {
            OpenBarcodeDialog();
            usleep(10000);
            emit signalBarcodeDetect();
        } else {
            emit signalBarcodeDetect();
        }

        SystemLog("Barcode reader finish !");

        usleep(10000);
        FinishPlcCmd();

        usleep(10000);
        ResultStatus("OK");
    } catch (std::exception e) {
        if(dm::log) {
            qDebug() << "[Barcode Error] Error occurred: " << e.what();
        }
        usleep(10000);
        ResultStatus("NG");
    }
}

void AlignSys::ReceiveBarcodeResult(const QString &info, int st)
{
    // Get the barcode detected result and send back to PLC
    // Send result back to PLC. result => barcode info; 0 => NG, 1 => OK
    emit signalPlcWriteBarcodeResult(info, st);
}

void AlignSys::OpenBarcodeDialog()
{
    if(isRunCalib){
        WarningMsg("警告 ( Warning )", "The Calibration process is running. Changing options may occur critial problems. \n\n"
                              "Please DO NOT change any parameters.", 1);
    }
    else if(isRunAlign){
        WarningMsg("警告 ( Warning )", "The Alignment process is running. Changing options may occur critial problems. \n\n"
                              "Please DO NOT change any parameters.", 1);
    }

    if (!isBarcodeDialogShown) {
        ui->label_systemType_cn->setText("條碼辨識系統");
        ui->label_systemType_en->setText("Barcode System");
        SystemLog("### Start barcode detecting setting ###");
        SystemLog("Init ...");
        func_mode = _MODE_NONE_;
        barcodeDialog->setWindowFlags(Qt::FramelessWindowHint);

        SystemLog("Open the barcode dialog...");

        if (barcodeDialog->GetDetCamIndex())
            barcodeDialog->SetCurrentImage(_Mat2QImg(camR->GetCurImg()));
        else
            barcodeDialog->SetCurrentImage(_Mat2QImg(camL->GetCurImg()));
        barcodeDialog->InitUI();
        barcodeDialog->setWindowFlag(Qt::WindowStaysOnBottomHint);
        barcodeDialog->show();
        isBarcodeDialogShown = true;
    }
}
#endif

void AlignSys::SystemLogfWithCodeLoc(const QString &msg, const QString &file, const QString &func)
{
    lw << QString(msg).toStdString() << QString(" @%1::%2()").arg(file).arg(func).toStdString() << lw.endl;
}

void AlignSys::WriteErrorCodeToPlc(QString error_code)
{
    emit signalPlcWriteCurrentErrorCodeNum(error_code);
}
