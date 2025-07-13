#include "matchingui.h"
#include "ui_matchingui.h"

// C++ Standard Library
#include <cmath>

// Qt
#include <QFile>
#include <QXmlStreamWriter>
#include <QXmlStreamReader>
#include <QBuffer>
#include <QPainter>
#include <QFileDialog>
#include <QTimer>
#include <QDateTime>

// Project-specific
#include "Camera/Alignment.h"

#ifdef M_PI
#undef M_PI
#endif
#define M_PI 3.14159265358979323846

/**
 * @brief Converts a QPixmap to a cv::Mat.
 * @param pixmap The input QPixmap.
 * @return The converted cv::Mat in BGR format.
 */
cv::Mat QPixmapToCVMat(const QPixmap &pixmap) {
    QImage image = pixmap.toImage().convertToFormat(QImage::Format_RGB888);
    cv::Mat mat(image.height(), image.width(), CV_8UC3, (void*)image.bits(), image.bytesPerLine());
    cv::Mat bgrMat;
    cv::cvtColor(mat, bgrMat, cv::COLOR_RGB2BGR);
    return bgrMat;
}


/******************************************************************************
 *                            CONSTRUCTOR & DESTRUCTOR                          *
 ******************************************************************************/

MatchingUI::MatchingUI(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MatchingUI),
    m_container1(".", "template1"),
    m_container2(".", "template2")
{
    ui->setupUi(this);

    setupUI();
    setupActionsAndConnections();
    loadAppSettings();
    setupPLC();
    setupCamera();
    setupMatching();
    updateUIFromSettings();
}

MatchingUI::~MatchingUI()
{
    m_initSetup->SaveSettings("uiSetting.ini");
    delete m_initSetup;

    if (camL && camL->isRunning()){
        camL->Stop();
    }
    delete camL;

    if (thread_plc_ && thread_plc_->isRunning()) {
        emit signalPlcDisconnect(false);
        StopPlcThread();
    }

    delete ui;
}

/******************************************************************************
 *                               INITIALIZATION                               *
 ******************************************************************************/

/**
 * @brief Sets up UI components that are not created in Qt Designer.
 */
void MatchingUI::setupUI() {
    ui->menuBar->setNativeMenuBar(false);
    setMenuBar(ui->menuBar);

    mainPicturePanel = new CustomPicturePanel(this);
    mainPicturePanel->setMinimumWidth(400);
    mainPicturePanel->setSizePolicy(QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Preferred);
    ui->right_pane_layout->addWidget(mainPicturePanel, 1);
}

/**
 * @brief Sets up QAction groups and connects signals/slots for the application.
 */
void MatchingUI::setupActionsAndConnections()
{
    // --- Picture Panel -> UI ---
    connect(mainPicturePanel, &CustomPicturePanel::templatesChanged, this, &MatchingUI::onTemplatesChanged);
    connect(mainPicturePanel, &CustomPicturePanel::roisChanged, this, &MatchingUI::onRoisChanged);

    // --- Action Groups ---
    modeActionGroup = new QActionGroup(this);
    modeActionGroup->addAction(ui->actionTrain);
    modeActionGroup->addAction(ui->actionSetting);
    modeActionGroup->addAction(ui->actionTest);
    modeActionGroup->addAction(ui->actionMeasure);
    modeActionGroup->setExclusive(true);
    connect(modeActionGroup, &QActionGroup::triggered, this, &MatchingUI::onModeChanged);

    methodActionGroup = new QActionGroup(this);
    methodActionGroup->addAction(ui->actionChamfer);
    methodActionGroup->addAction(ui->actionFastRST);
    methodActionGroup->setExclusive(true);
    connect(methodActionGroup, &QActionGroup::triggered, this, &MatchingUI::onMethodChanged);

    // --- Buttons ---
    connect(ui->btn_capture, &QPushButton::clicked, this, &MatchingUI::on_btn_capture_clicked);
    connect(ui->btn_connect_plc, &QCheckBox::clicked, this, &MatchingUI::on_btn_connect_plc_clicked);
}

/**
 * @brief Loads application settings from the INI file.
 */
void MatchingUI::loadAppSettings() {
    m_initSetup = new InitSetup();
    m_initSetup->LoadSettings("uiSetting.ini");
}

/**
 * @brief Sets up the PLC connector and its thread.
 */
void MatchingUI::setupPLC() {
    plc_status_ = 0;
    isconnectPLC = false;
    connector_plc_ = new PlcConnector();
    thread_plc_ = new QThread();

    connector_plc_->moveToThread(thread_plc_);

    // --- UI -> PLC ---
    connect(this, &MatchingUI::signalPlcInit, connector_plc_, &PlcConnector::Initialize);
    connect(this, &MatchingUI::signalPlcSetPath, connector_plc_, &PlcConnector::SetProjectPath);
    connect(this, &MatchingUI::signalPlcDisconnect, connector_plc_, &PlcConnector::Disconnect);
    connect(this, &MatchingUI::signalPlcLoadDefaultSetting, connector_plc_, &PlcConnector::LoadDefaultSettingfromINIFile);
    connect(this, &MatchingUI::signalPlcWriteSetting, connector_plc_, &PlcConnector::WriteSetting2File);
    connect(this, &MatchingUI::signalPlcSetWaitingPlcAck, connector_plc_, &PlcConnector::SetWaitingPlcAck);
    connect(this, &MatchingUI::signalPlcWriteResult, connector_plc_, &PlcConnector::WriteResult, Qt::BlockingQueuedConnection);
    connect(this, &MatchingUI::signalPlcWriteCurrentRecipeNum, connector_plc_, &PlcConnector::WriteCurrentRecipeNum);
    connect(this, &MatchingUI::signalPlcWriteCurrentErrorCodeNum, connector_plc_, &PlcConnector::WriteCurrentErrorCodeNum);
    connect(this, &MatchingUI::signalPlcSetCCDTrig, connector_plc_, &PlcConnector::SetCCDTrig, Qt::BlockingQueuedConnection);
    connect(this, &MatchingUI::signalPlcSetStatus, connector_plc_, &PlcConnector::SetPlcStatus);
    connect(this, &MatchingUI::signalPlcSetCurrentRecipeNum, connector_plc_, &PlcConnector::SetCurrentRecipeNum);
    connect(this, &MatchingUI::signalPlcLoadSetting, connector_plc_, &PlcConnector::LoadSettingfromFile);
    connect(this, &MatchingUI::signalPlcGetStatus, connector_plc_, &PlcConnector::GetPLCStatus);

    // --- PLC -> UI ---
    connect(connector_plc_, &PlcConnector::signalActionAlignment, this, &MatchingUI::ActionAlign);
    connect(connector_plc_, &PlcConnector::signalState, this, &MatchingUI::UpdatePLCState);
    connect(connector_plc_, &PlcConnector::signalStatus, this, &MatchingUI::UpdatePlcStatus);
    connect(connector_plc_, &PlcConnector::signalLogs, this, &MatchingUI::PrintPlcLogs);

    // --- Thread Management ---
    connect(thread_plc_, &QThread::finished, connector_plc_, &QObject::deleteLater);
    connect(thread_plc_, &QThread::finished, thread_plc_, &QObject::deleteLater);
    connect(thread_plc_, &QObject::destroyed, this, [this](){ thread_plc_ = nullptr; });
    connect(connector_plc_, &QObject::destroyed, this,[this](){ connector_plc_ = nullptr;});

    emit signalPlcInit();
}

/**
 * @brief Sets up the camera capture thread.
 */
void MatchingUI::setupCamera() {
    camL = new CaptureThreadL(this);
    camL->setPathFolderSave(m_initSetup->getPathSaveCapture());
    camL->using_default_parameter = true;
    camL->mutex_shared = mutex_shared;
    connect(camL, &CaptureThreadL::frameReady, this, &MatchingUI::update_QImage);
}

/**
 * @brief Initializes matching-related components.
 */
void MatchingUI::setupMatching() {
    a = 100.0f; // mm

    calibration_ = new Calibration();
    if (calibration_->loadFromJson(m_initSetup->getPathCalibration())) {
        mmPerPixel = calibration_->getMmPerPixel();
        ui->log_text_edit->append(QString("[Calibration] distanceCamera: %1cm, mmPerPixel: %2")
                                  .arg(calibration_->getDistanceCamera()).arg(mmPerPixel));
    } else {
        mmPerPixel = 0.08;
        ui->log_text_edit->append("[Calibration] Load failed, using default values.");
    }

    m_container1.SetUseCudaForImageProcessing(true);
    m_container2.SetUseCudaForImageProcessing(true);
    loadTemplatesInfo();

    ui->lbl_ref_c1->setText(QString("%1, %2").arg(m_trained_template_roi1.center().x()).arg(m_trained_template_roi1.center().y()));
    ui->lbl_ref_c2->setText(QString("%1, %2").arg(m_trained_template_roi2.center().x()).arg(m_trained_template_roi2.center().y()));
    ui->lbl_ref_p->setText(QString("%1, %2").arg((m_trained_template_roi1.center().x() + m_trained_template_roi2.center().x())/2.0f, 0, 'f', 1).arg((m_trained_template_roi1.center().y() + m_trained_template_roi2.center().y())/2.0f, 0, 'f', 1));
    ui->lbl_ref_a->setText(QString("%1").arg(m_trained_angle, 0, 'f', 2));
}

/**
 * @brief Updates the UI state based on the loaded settings.
 */
void MatchingUI::updateUIFromSettings() {
    QString initialMode = m_initSetup->getMode();
    if (initialMode == ui->actionTrain->text()) {
        ui->actionTrain->setChecked(true);
    } else if (initialMode == ui->actionSetting->text()) {
        ui->actionSetting->setChecked(true);
    } else if (initialMode == ui->actionMeasure->text()) {
        ui->actionMeasure->setChecked(true);
    } else {
        ui->actionTest->setChecked(true);
    }
    onModeChanged(modeActionGroup->checkedAction());

    QString initialMethod = m_initSetup->getMethod();
    if (initialMethod == ui->actionFastRST->text()) {
        ui->actionFastRST->setChecked(true);
    } else {
        ui->actionChamfer->setChecked(true);
    }
    onMethodChanged(methodActionGroup->checkedAction());
}

void MatchingUI::resizeEvent(QResizeEvent *event) {
    Q_UNUSED(event);
    ui->lbl_display_1->update();
    ui->lbl_display_2->update();
    this->update();
}

/******************************************************************************
 *                                  UI SLOTS                                  *
 ******************************************************************************/

/**
 * @brief Handles mode changes from the UI (Train, Setting, Test, Measure).
 * @param action The triggered QAction.
 */
void MatchingUI::onModeChanged(QAction *action)
{
    if (!action) return;
    ui->lbl_mode_value->setText(action->text());
    m_initSetup->setMode(action->text());

    if (action == ui->actionTrain) {
        mainPicturePanel->setMode(CustomPictureBox::Training);
        current_mode = CustomPictureBox::Training;
        setTrainMode(true);
    } else if (action == ui->actionSetting) {
        mainPicturePanel->setMode(CustomPictureBox::Setting);
        current_mode = CustomPictureBox::Setting;
        setSettingMode(true);
    } else if (action == ui->actionTest) {
        mainPicturePanel->setMode(CustomPictureBox::Test);
        current_mode = CustomPictureBox::Test;
        setTrainMode(true);
    } else if (action == ui->actionMeasure) {
        mainPicturePanel->setMode(CustomPictureBox::Measure);
        current_mode = CustomPictureBox::Measure;
        setSettingMode(false);
    }
}

/**
 * @brief Handles matching method changes from the UI (Chamfer, FastRST).
 * @param action The triggered QAction.
 */
void MatchingUI::onMethodChanged(QAction *action)
{
    if (!action) return;
    ui->lbl_method_value->setText(action->text());
    m_initSetup->setMethod(action->text());

    MatchingMode mode = (action == ui->actionChamfer) ? MatchingMode::CHAMFER : MatchingMode::FASTRST;
    m_container1.SetMode(mode);
    m_container2.SetMode(mode);
    ui->lbl_display_1->clear();
    ui->lbl_display_2->clear();

    qDebug() << "Method changed to:" << action->text();
}

/**
 * @brief Connects/disconnects the camera.
 */
void MatchingUI::on_btn_connect_clicked()
{
    if (ui->btn_connect->isChecked()) {
        try {
            mutex_shared = new QMutex();
            camL->mutex_shared = mutex_shared;
            camL->Play();
            if (camL->IsConnected()){
                ui->statusBar->showMessage("Connected Camera", 5000);
                ui->btn_connect->setText("Connected Camera");
            } else {
                ui->statusBar->showMessage("Cannot Connect Camera", 5000);
                ui->btn_connect->setChecked(false);
            }
        }
        catch (const std::exception& e) {
            qDebug() << "Error in on_btn_connect_clicked:" << e.what();
            ui->statusBar->showMessage("Cannot Connect Camera", 5000);
            ui->btn_connect->setChecked(false);
        }
    } else {
        try {
            camL->Stop();
            if (camL->isStopped()){
                ui->statusBar->showMessage("Disconnect Camera", 5000);
                ui->btn_connect->setText("Connect Camera");
            }
        }
        catch (const std::exception& e) {
            qDebug() << "Error in on_btn_connect_clicked:" << e.what();
            ui->statusBar->showMessage("Cannot Connect Camera", 5000);
            ui->btn_connect->setChecked(false);
        }
    }
}

/**
 * @brief Connects/disconnects the PLC.
 * @param checked The checked state of the button.
 */
void MatchingUI::on_btn_connect_plc_clicked(bool checked)
{
    if (checked) {
        if (thread_plc_ && !thread_plc_->isRunning()) {
            thread_plc_->start();
            emit signalPlcInit();
        }
    } else {
        if (thread_plc_ && thread_plc_->isRunning()) {
            emit signalPlcDisconnect(false);
        }
    }
}

/**
 * @brief Initiates the training process for the defined templates.
 */
void MatchingUI::on_btn_train_data_clicked()
{
    this->setEnabled(false);
    ui->log_text_edit->append("Starting training process...");
    QCoreApplication::processEvents();

    if (!m_template_mat1.empty()) m_trained_template_roi1 = template_roi_1;
    if (!m_template_mat2.empty()) m_trained_template_roi2 = template_roi_2;
    saveTemplatesInfo();

    float total_time_exec = 0;
    bool trained_something = false;

    ui->log_text_edit->append("Processing Template 1...");
    float time1 = m_container1.Train(m_template_mat1);
    if (time1 > 0) {
        ui->log_text_edit->append(QString("Template 1 trained successfully in %1 ms").arg(time1 * 1000, 0, 'f', 1));
        total_time_exec += time1;
        QPixmap pix = QPixmap::fromImage(QImage(m_container1.GetTrainedTemplateImg().data,
                                         m_container1.GetTrainedTemplateImg().cols,
                                         m_container1.GetTrainedTemplateImg().rows,
                                         m_container1.GetTrainedTemplateImg().step,
                                         QImage::Format_RGB888).rgbSwapped());
        if (!pix.isNull()) {
                    ui->lbl_display_1->setPixmap(pix.scaled(ui->lbl_display_1->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
                }
        trained_something = true;
    } else {
        ui->log_text_edit->append("Failed to train Template 1.");
        trained_something = false;
    }

    ui->log_text_edit->append("Processing Template 2...");
    float time2 = m_container2.Train(m_template_mat2);
    if (time2 > 0) {
        ui->log_text_edit->append(QString("Template 2 trained successfully in %1 ms").arg(time2 * 1000, 0, 'f', 1));
        total_time_exec += time2;
        QPixmap pix = QPixmap::fromImage(QImage(m_container2.GetTrainedTemplateImg().data,
                                         m_container2.GetTrainedTemplateImg().cols,
                                         m_container2.GetTrainedTemplateImg().rows,
                                         m_container2.GetTrainedTemplateImg().step,
                                         QImage::Format_RGB888).rgbSwapped());
        if (!pix.isNull()) {
                    ui->lbl_display_2->setPixmap(pix.scaled(ui->lbl_display_2->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
                }
        trained_something = true;
    } else {
        ui->log_text_edit->append("Failed to train Template 2.");
        trained_something = false;
    }

    ui->log_text_edit->append(QString("Total execution time: %1 ms").arg(total_time_exec * 1000, 0, 'f', 1));

    this->setEnabled(true);
    ui->log_text_edit->append("Training process finished.");
}

/**
 * @brief Runs the matching algorithm on the current image.
 */
void MatchingUI::on_btn_run_clicked()
{
    if (lastpixmap.isNull()) {
        ui->log_text_edit->append("[ERROR] No image loaded to run test on.");
        return;
    }
    if (m_roi_rect1.isNull() || m_roi_rect2.isNull()) {
        ui->log_text_edit->append("[ERROR] Both ROIs must be defined for testing.");
        return;
    }
    if (!m_container1.IsTrainedOK() || !m_container2.IsTrainedOK()) {
        ui->log_text_edit->append("[ERROR] Templates are not trained. Please train first.");
        return;
    }

    mainPicturePanel->setImage(lastpixmap);
    ui->log_text_edit->append("Running inference...");
    QCoreApplication::processEvents();
    this->setEnabled(false);

    cv::Mat source_mat = QPixmapToCVMat(lastpixmap);
    source_mat = calibration_->undistortImage(source_mat);
    cv::Mat display_mat = source_mat.clone();

    // Run matching on both ROIs
    cv::Mat src_roi1 = source_mat(cv::Rect(m_roi_rect1.x(), m_roi_rect1.y(), m_roi_rect1.width(), m_roi_rect1.height()));
    float time1 = m_container1.Test(src_roi1);
    std::vector<cv::Point> result1 = m_container1.GetResult();

    cv::Mat src_roi2 = source_mat(cv::Rect(m_roi_rect2.x(), m_roi_rect2.y(), m_roi_rect2.width(), m_roi_rect2.height()));
    float time2 = m_container2.Test(src_roi2);
    std::vector<cv::Point> result2 = m_container2.GetResult();

    // Process and display results
    if (result1.empty() || result2.empty()) {
        ui->log_text_edit->append("Cannot detect one or both objects.");
        if (isconnectPLC) emit signalPlcWriteResult(0, 0, 0, 2, PlcWriteMode::kOthers);
    } else {
        drawResult(display_mat, result1, m_roi_rect1);
        drawResult(display_mat, result2, m_roi_rect2);

        // Analysis
        QPoint found_center1(result1[0].x + m_roi_rect1.x(), result1[0].y + m_roi_rect1.y());
        QPoint found_center2(result2[0].x + m_roi_rect2.x(), result2[0].y + m_roi_rect2.y());
        QPoint found_center((found_center1.x() + found_center2.x()) / 2.0f, (found_center1.y() + found_center2.y()) / 2.0f);

        float angle_rad = atan2(found_center1.y() - found_center2.y(), found_center1.x() - found_center2.x());
        float avg_angle_dev = (angle_rad + M_PI/2.0) * 180.0 / M_PI;

        if (current_mode == CustomPictureBox::Training) {
            m_trained_angle = avg_angle_dev;
            ui->lbl_ref_c1->setText(QString("%1, %2").arg(m_trained_template_roi1.center().x()).arg(m_trained_template_roi1.center().y()));
            ui->lbl_ref_c2->setText(QString("%1, %2").arg(m_trained_template_roi2.center().x()).arg(m_trained_template_roi2.center().y()));
            ui->lbl_ref_p->setText(QString("%1, %2").arg(found_center.x(), 0, 'f', 1).arg(found_center.y(), 0, 'f', 1));
            ui->lbl_ref_a->setText(QString("%1").arg(m_trained_angle, 0, 'f', 2));
            saveTemplatesInfo();
        } else { // Test mode
            ui->lbl_test_c1->setText(QString("%1, %2").arg(found_center1.x()).arg(found_center1.y()));
            ui->lbl_test_c2->setText(QString("%1, %2").arg(found_center2.x()).arg(found_center2.y()));
            ui->lbl_test_p->setText(QString("%1, %2").arg(found_center.x(), 0, 'f', 1).arg(found_center.y(), 0, 'f', 1));
            ui->lbl_test_a->setText(QString("%1").arg(avg_angle_dev, 0, 'f', 2));

            QPoint trained_center = QPoint((m_trained_template_roi1.center().x() + m_trained_template_roi2.center().x()) / 2.0f,
                                           (m_trained_template_roi1.center().y() + m_trained_template_roi2.center().y()) / 2.0f);
            float delta_angle = m_trained_angle - avg_angle_dev;
            AlignmentResult alignRes = Alignment::compute(trained_center, found_center, delta_angle, mmPerPixel, a);

            ui->lbl_offsetX->setText(QString("%1").arg(alignRes.dx_robot, 0, 'f', 2));
            ui->lbl_offsetY->setText(QString("%1").arg(alignRes.dy_robot, 0, 'f', 2));
            ui->lbl_offsetAngle->setText(QString("%1").arg(alignRes.angle_deg, 0, 'f', 2));

            double z_cam = 0.85;
            Pose robot_pose = transformer->img_2_robot(found_center.x(), found_center.y(), z_cam, -89.9);

            ui->le_pose_x->setText(QString("%1").arg(robot_pose.x, 0, 'f', 2));
            ui->le_pose_y->setText(QString("%1").arg(robot_pose.y, 0, 'f', 2));
            ui->le_pose_z->setText(QString("%1").arg(robot_pose.z, 0, 'f', 2));
            ui->le_pose_rx->setText(QString("%1").arg(robot_pose.rx, 0, 'f', 2));
            ui->le_pose_ry->setText(QString("%1").arg(robot_pose.ry, 0, 'f', 2));
            ui->le_pose_rz->setText(QString("%1").arg(robot_pose.rz, 0, 'f', 2));

            if (isconnectPLC) {
                emit signalPlcWriteResult(alignRes.dx_robot*100, alignRes.dy_robot*100, alignRes.angle_deg*100, 1, PlcWriteMode::kOthers);
            }
        }
    }

    ui->log_text_edit->append(QString("Inference Time: %1 ms").arg((time1 + time2) * 1000, 0, 'f', 2));
    QPixmap pix = QPixmap::fromImage(QImage(display_mat.data, display_mat.cols, display_mat.rows, display_mat.step, QImage::Format_RGB888).rgbSwapped());
    mainPicturePanel->setImage(pix);

    if (m_initSetup->getSaveResultEnabled()) {
        QString path = m_initSetup->getPathSaveResult();
        QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
        QString fileName = QString("result_%1.png").arg(timestamp);
        QString fullPath = path + "/" + fileName;
        if(pix.save(fullPath)) {
            ui->log_text_edit->append(QString("Saved result image to: %1").arg(fullPath));
        } else {
            ui->log_text_edit->append(QString("Failed to save result image to: %1").arg(fullPath));
        }
    }

    this->setEnabled(true);
}

/**
 * @brief Captures and saves the current frame from the camera.
 */
void MatchingUI::on_btn_capture_clicked()
{
    if (camL && camL->isRunning()) {
        camL->saveCurrentFrame();
        ui->log_text_edit->append("Frame saved.");
    } else {
        ui->log_text_edit->append("Camera not running. Please connect the camera first.");
    }
}

/**
 * @brief Loads an image from a file.
 */
void MatchingUI::on_btn_loadImage_clicked()
{
    if (camL && camL->isRunning()) {
        camL->Stop();
    }

    QString fileName = QFileDialog::getOpenFileName(this, tr("Open Image"), "", tr("Image Files (*.png *.jpg *.bmp)"));
    if (!fileName.isEmpty()) {
        QPixmap pix(fileName);
        lastpixmap = pix.copy();
        mainPicturePanel->setImage(lastpixmap);
        loadRois(lastpixmap.size());
    }
}

/******************************************************************************
 *                             CUSTOM WIDGET SLOTS                            *
 ******************************************************************************/

/**
 * @brief Handles updates when templates are created or changed in the picture box.
 * @param templates A map of template IDs to their data.
 */
void MatchingUI::onTemplatesChanged(const QMap<int, CustomPictureBox::TemplateData> &templates)
{
    ui->log_text_edit->append(QString("Template list updated. Count: %1").arg(templates.size()));
    ui->lbl_display_1->clear();
    ui->lbl_display_2->clear();

    if (templates.contains(0)) {
        m_template_pixmap1 = templates[0].image.copy();
        template_roi_1 = templates[0].roi;
        m_template_mat1 = QPixmapToCVMat(m_template_pixmap1);
        m_template_pixmap1.save("template_L.png");
        ui->lbl_display_1->setPixmap(templates[0].image.scaled(ui->lbl_display_1->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    } else {
        m_template_mat1.release();
    }

    if (templates.contains(1)) {
        m_template_pixmap2 = templates[1].image.copy();
        template_roi_2 = templates[1].roi;
        m_template_mat2 = QPixmapToCVMat(m_template_pixmap2);
        m_template_pixmap2.save("template_R.png");
        ui->lbl_display_2->setPixmap(templates[1].image.scaled(ui->lbl_display_2->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    } else {
        m_template_mat2.release();
    }
}

/**
 * @brief Handles updates when ROIs are created or changed in the picture box.
 * @param rois A map of ROI IDs to their data.
 */
void MatchingUI::onRoisChanged(const QMap<int, CustomPictureBox::TemplateData> &rois)
{
    ui->log_text_edit->append(QString("ROI list updated. Count: %1").arg(rois.size()));
    m_roi_rect1 = rois.value(0).roi;
    m_roi_rect2 = rois.value(1).roi;
    saveRois(rois);
}

/******************************************************************************
 *                                CAMERA SLOTS                                *
 ******************************************************************************/

/**
 * @brief Receives a new frame from the camera thread and displays it.
 * @param img The new frame as a QImage.
 */
void MatchingUI::update_QImage(QImage img) {
    if (camL && camL->isRunning()) {
        QPixmap pixmap = QPixmap::fromImage(img);
        if(!pixmap.isNull()) {
            mainPicturePanel->setImage(pixmap);
        }
        lastpixmap = pixmap.copy();
        camL->frame_in_process = false;
    }
}

/******************************************************************************
 *                                  PLC SLOTS                                 *
 ******************************************************************************/

/**
 * @brief Updates the internal PLC status variable.
 * @param st The new status code from the PLC.
 */
void MatchingUI::UpdatePlcStatus(uint16_t st) {
    plc_status_ = st;
}

/**
 * @brief Updates the UI and internal state based on PLC connection status.
 * @param state True if connected, false otherwise.
 */
void MatchingUI::UpdatePLCState(bool state)
{
    isconnectPLC = state;
    if (state){
        ui->btn_connect_plc->setStyleSheet("background-color: green");
    } else {
        ui->btn_connect_plc->setStyleSheet("");
    }
}

/**
 * @brief Prints log messages from the PLC to the UI.
 * @param msg The log message.
 */
void MatchingUI::PrintPlcLogs(QString msg) {
    ui->log_text_edit->append("[PLC] " + msg);
}

/**
 * @brief Handles the alignment command from the PLC.
 */
void MatchingUI::ActionAlign() {
    if (isconnectPLC && ui->cb_start->isChecked()) {
        ui->log_text_edit->append("Start Alignment triggered by PLC.");
        on_btn_connect_clicked();
        SleepByQtimer(100);
        on_btn_run_clicked();
    }
}

/**
 * @brief Safely stops the PLC thread.
 */
void MatchingUI::StopPlcThread(){
    if (thread_plc_ && thread_plc_->isRunning()) {
        thread_plc_->quit();
        thread_plc_->wait();
    }
}

/**
 * @brief Signals the PLC that the CCD command is finished.
 */
void MatchingUI::FinishPlcCmd(){
    emit signalPlcSetCCDTrig();
    emit signalPlcSetStatus(_PLC_IDLE_);
}

/******************************************************************************
 *                              CORE LOGIC & HELPERS                          *
 ******************************************************************************/

/**
 * @brief Enables or disables UI controls for training mode.
 * @param enabled True to enable, false to disable.
 */
void MatchingUI::setTrainMode(bool enabled){
    ui->btn_train_data->setEnabled(enabled);
    ui->btn_run->setEnabled(enabled);
}

/**
 * @brief Enables or disables UI controls for setting mode.
 * @param enabled True to enable, false to disable.
 */
void MatchingUI::setSettingMode(bool enabled){
    ui->btn_train_data->setEnabled(!enabled);
    ui->btn_run->setEnabled(!enabled);
}

/**
 * @brief Saves the current ROI definitions to an XML file.
 * @param rois A map of ROI IDs to their data.
 */
void MatchingUI::saveRois(const QMap<int, CustomPictureBox::TemplateData> &rois)
{
    QSize imageSize = getPictureBox() ? getPictureBox()->getOriginalImageSize() : QSize();
    if (imageSize.isEmpty()) {
        qDebug() << "Cannot save ROIs, original image size is invalid.";
        return;
    }

    QString roiFilePath = m_initSetup->getPathSaveRoi();
    QFile file(roiFilePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qDebug() << "Could not open ROI file for writing:" << roiFilePath;
        return;
    }

    QXmlStreamWriter xml(&file);
    xml.setAutoFormatting(true);
    xml.writeStartDocument();
    xml.writeStartElement("rois");

    for (const auto& roiData : rois.values()) {
        xml.writeStartElement("roi");
        xml.writeAttribute("id", QString::number(roiData.id));
        double relX = (double)roiData.roi.x() / imageSize.width();
        double relY = (double)roiData.roi.y() / imageSize.height();
        double relW = (double)roiData.roi.width() / imageSize.width();
        double relH = (double)roiData.roi.height() / imageSize.height();
        xml.writeAttribute("x", QString::number(relX, 'f', 6));
        xml.writeAttribute("y", QString::number(relY, 'f', 6));
        xml.writeAttribute("w", QString::number(relW, 'f', 6));
        xml.writeAttribute("h", QString::number(relH, 'f', 6));
        xml.writeEndElement(); // roi
    }

    xml.writeEndElement(); // rois
    xml.writeEndDocument();
    file.close();
}

/**
 * @brief Loads ROI definitions from an XML file.
 * @param imageSize The current size of the image to scale the ROIs to.
 */
void MatchingUI::loadRois(const QSize &imageSize)
{
    QMap<int, CustomPictureBox::TemplateData> loadedRois;
    QString roiFilePath = m_initSetup->getPathSaveRoi();
    QFile file(roiFilePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "Could not open ROI file for reading:" << roiFilePath;
        return;
    }

    QXmlStreamReader xml(&file);
    while (!xml.atEnd() && !xml.hasError()) {
        if (xml.readNext() == QXmlStreamReader::StartElement && xml.name().toString() == "roi") {
            int id = xml.attributes().value("id").toInt();
            double relX = xml.attributes().value("x").toDouble();
            double relY = xml.attributes().value("y").toDouble();
            double relW = xml.attributes().value("w").toDouble();
            double relH = xml.attributes().value("h").toDouble();

            QRect roiRect(qRound(relX * imageSize.width()), qRound(relY * imageSize.height()),
                          qRound(relW * imageSize.width()), qRound(relH * imageSize.height()));

            CustomPictureBox::TemplateData data;
            data.id = id;
            data.roi = roiRect;
            data.name = QString("ROI %1").arg(id + 1);
            loadedRois[id] = data;
        }
    }

    if (xml.hasError()) {
        qDebug() << "XML error while reading ROIs:" << xml.errorString();
    } else if (!loadedRois.isEmpty() && getPictureBox()) {
        getPictureBox()->setRois(loadedRois);
        mainPicturePanel->updateTable();
        ui->log_text_edit->append("Loaded ROIs from " + roiFilePath);
    }
    file.close();
}

/**
 * @brief Saves the trained template information (ROIs, angle) to an XML file.
 */
void MatchingUI::saveTemplatesInfo()
{
    QString path = m_initSetup->getPathSaveTemplate();
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qDebug() << "Could not open templates file for writing:" << path;
        return;
    }

    QXmlStreamWriter xml(&file);
    xml.setAutoFormatting(true);
    xml.writeStartDocument();
    xml.writeStartElement("templates");

    if (!m_trained_template_roi1.isNull()) {
        xml.writeStartElement("template");
        xml.writeAttribute("id", "0");
        xml.writeAttribute("x", QString::number(m_trained_template_roi1.x()));
        xml.writeAttribute("y", QString::number(m_trained_template_roi1.y()));
        xml.writeAttribute("w", QString::number(m_trained_template_roi1.width()));
        xml.writeAttribute("h", QString::number(m_trained_template_roi1.height()));
        xml.writeEndElement();
    }

    if (!m_trained_template_roi2.isNull()) {
        xml.writeStartElement("template");
        xml.writeAttribute("id", "1");
        xml.writeAttribute("x", QString::number(m_trained_template_roi2.x()));
        xml.writeAttribute("y", QString::number(m_trained_template_roi2.y()));
        xml.writeAttribute("w", QString::number(m_trained_template_roi2.width()));
        xml.writeAttribute("h", QString::number(m_trained_template_roi2.height()));
        xml.writeEndElement();
    }

    xml.writeStartElement("angle");
    xml.writeAttribute("angle", QString::number(m_trained_angle));
    xml.writeEndElement();

    xml.writeEndElement(); // templates
    xml.writeEndDocument();
    file.close();
    ui->log_text_edit->append("Saved template info to " + path);
}

/**
 * @brief Loads the trained template information from an XML file.
 */
void MatchingUI::loadTemplatesInfo()
{
    QString path = m_initSetup->getPathSaveTemplate();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "Could not open templates file for reading:" << path;
        return;
    }

    QXmlStreamReader xml(&file);
    while (!xml.atEnd() && !xml.hasError()) {
        if (xml.readNext() == QXmlStreamReader::StartElement) {
            if (xml.name().toString() == "template") {
                int id = xml.attributes().value("id").toInt();
                int x = xml.attributes().value("x").toInt();
                int y = xml.attributes().value("y").toInt();
                int w = xml.attributes().value("w").toInt();
                int h = xml.attributes().value("h").toInt();
                if (id == 0) m_trained_template_roi1.setRect(x, y, w, h);
                else if (id == 1) m_trained_template_roi2.setRect(x, y, w, h);
            } else if (xml.name().toString() == "angle") {
                m_trained_angle = xml.attributes().value("angle").toFloat();
            }
        }
    }

    if (xml.hasError()) {
        qDebug() << "XML error while reading templates:" << xml.errorString();
    } else {
        ui->log_text_edit->append("Loaded template info from " + path);
    }
    file.close();
}

/**
 * @brief Draws the matching result on the display image.
 * @param image The image to draw on.
 * @param points The result points from the matching algorithm.
 * @param roi The ROI where the matching was performed.
 */
void MatchingUI::drawResult(cv::Mat &image, const std::vector<cv::Point> &points, const QRect &roi)
{
    if (points.empty()) return;

    std::vector<cv::Point> global_points;
    for(const auto& pt : points) {
        global_points.push_back(cv::Point(pt.x + roi.x(), pt.y + roi.y()));
    }

    MatchingMode mode = m_container1.GetMode();
    switch(mode){
    case MatchingMode::CHAMFER:
        // Draw center cross
        cv::line(image, cv::Point(global_points[0].x - 30, global_points[0].y), cv::Point(global_points[0].x + 30, global_points[0].y), cv::Scalar(255, 0, 0), 5);
        cv::line(image, cv::Point(global_points[0].x, global_points[0].y - 30), cv::Point(global_points[0].x, global_points[0].y + 30), cv::Scalar(255, 0, 0), 5);
        cv::line(image, cv::Point(global_points[0].x, global_points[0].y), cv::Point(global_points[1].x, global_points[1].y), cv::Scalar(255, 255, 0), 5);
        // Draw bounding box
        if (global_points.size() >= 6) {
            for (size_t i = 2; i < 6; ++i) {
                cv::line(image, global_points[i], global_points[i % 5 + i / 5 + 1], cv::Scalar(0, 255, 255), 5);
            }
        }
        break;
    case MatchingMode::FASTRST:
        // Draw center cross
        cv::line(image, cv::Point(global_points[0].x - 30, global_points[0].y), cv::Point(global_points[0].x + 30, global_points[0].y), cv::Scalar(255, 0, 0), 5);
        cv::line(image, cv::Point(global_points[0].x, global_points[0].y - 30), cv::Point(global_points[0].x, global_points[0].y + 30), cv::Scalar(255, 0, 0), 5);
        // Draw bounding box
        cv::line(image, cv::Point(global_points[0].x, global_points[0].y), cv::Point(global_points[1].x, global_points[1].y), cv::Scalar(255, 255, 0), 5);
        if (global_points.size() >= 6) {
            for (size_t i = 2; i < 6; ++i) {
                cv::line(image, global_points[i], global_points[i % 5 + i / 5 + 1], cv::Scalar(0, 255, 255), 5);
            }
        }
        break;
    default:
        break;
    }
}

/**
 * @brief Pauses execution for a specified duration without freezing the UI.
 * @param timeout_ms The duration to sleep in milliseconds.
 */
void MatchingUI::SleepByQtimer(unsigned int timeout_ms)
{
    QEventLoop loop;
    QTimer::singleShot(timeout_ms, &loop, &QEventLoop::quit);
    loop.exec();
}
