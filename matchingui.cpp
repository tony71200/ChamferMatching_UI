#include "matchingui.h"
#include "ui_matchingui.h"
#include <QFile>
#include <QXmlStreamWriter>
#include <QXmlStreamReader>
#include <QBuffer>
#include <QPainter>
#include <QFileDialog>
#include <cmath>
#include "Camera/Alignment.h"

#ifdef M_PI
#define M_PI 3.14159265358979323846
#endif


cv::Mat QPixmapToCVMat(const QPixmap &pixmap) {
    QImage image = pixmap.toImage().convertToFormat(QImage::Format_RGB888);
    cv::Mat mat(image.height(), image.width(), CV_8UC3, (void*)image.bits(), image.bytesPerLine());
    cv::Mat bgrMat;
    cv::cvtColor(mat, bgrMat, cv::COLOR_RGB2BGR);
    return bgrMat;
}

MatchingUI::MatchingUI(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MatchingUI),
    m_container1(".", "template1"),
    m_container2(".", "template2")
{
    ui->setupUi(this);
    ui->menuBar->setNativeMenuBar(false);
    setMenuBar(ui->menuBar);
    // Create and add the custom widget
    mainPictureBox = new CustomPictureBox(this);
    ui->right_pane_layout->insertWidget(1, mainPictureBox);

    setupConnections();
    focusx = 12.5f; // mm
    focusy = 12.5f; // mm
    a = 100.0f; // mm
    // ================PLC Communication===================================
    // PLC connection by Jimmy. 20220119
    plc_status_ = 0;
    connector_plc_ = new PlcConnector();
    thread_plc_ = new QThread();

    connector_plc_->moveToThread(thread_plc_);
    QObject::connect(ui->btn_connect_plc, &QPushButton::clicked, this, &MatchingUI::on_btn_connect_plc_clicked);

    //QObject::connect(this, &MatchingUI::signalPlcSetPath, connector_plc_, &PlcConnector::SetProjectPath);
    QObject::connect(this, &MatchingUI::signalPlcInit, connector_plc_, &PlcConnector::Initialize);
    QObject::connect(this, &MatchingUI::signalPlcDisconnect, connector_plc_, &PlcConnector::Disconnect);
    QObject::connect(this, &MatchingUI::signalPlcLoadDefaultSetting, connector_plc_, &PlcConnector::LoadDefaultSettingfromINIFile);
    QObject::connect(this, &MatchingUI::signalPlcWriteSetting, connector_plc_, &PlcConnector::WriteSetting2File);
    QObject::connect(this, &MatchingUI::signalPlcSetWaitingPlcAck, connector_plc_, &PlcConnector::SetWaitingPlcAck);
    QObject::connect(this, &MatchingUI::signalPlcWriteResult, connector_plc_, &PlcConnector::WriteResult, Qt::BlockingQueuedConnection);
    QObject::connect(this, &MatchingUI::signalPlcWriteCurrentRecipeNum, connector_plc_, &PlcConnector::WriteCurrentRecipeNum);
    QObject::connect(this, &MatchingUI::signalPlcWriteCurrentErrorCodeNum, connector_plc_, &PlcConnector::WriteCurrentErrorCodeNum); //garly_20220617

    QObject::connect(this, &MatchingUI::signalPlcSetCCDTrig, connector_plc_, &PlcConnector::SetCCDTrig, Qt::BlockingQueuedConnection);
    QObject::connect(this, &MatchingUI::signalPlcSetStatus, connector_plc_, &PlcConnector::SetPlcStatus);
    QObject::connect(this, &MatchingUI::signalPlcSetCurrentRecipeNum, connector_plc_, &PlcConnector::SetCurrentRecipeNum);
    QObject::connect(this, &MatchingUI::signalPlcLoadSetting, connector_plc_, &PlcConnector::LoadSettingfromFile);
    QObject::connect(this, &MatchingUI::signalPlcGetStatus, connector_plc_, &PlcConnector::GetPLCStatus);

//    QObject::connect(connector_plc_, &PlcConnector::signalErrorLoadingParmSetting, this, [this](){
//        WarningMsgDialog* wmsg = new WarningMsgDialog(this, "警告 ( Warning )",
//                                    "PLC參數設定讀取錯誤，且無法取得備份參數，將PLC參數回歸為預設值\n"
//                                    "請重新設定PLC參數設定!",
//                                    Qt::WA_DeleteOnClose, Qt::FramelessWindowHint);
//        wmsg->move((this->width()/* - wmsg->width()*/)/2, (this->height() - wmsg->height())/2);
//    });

    QObject::connect(thread_plc_, &QThread::finished, connector_plc_, &QObject::deleteLater);
    QObject::connect(thread_plc_, &QThread::finished, thread_plc_, &QObject::deleteLater);
    /// Set wild pointer to nullptr
    QObject::connect(thread_plc_, &QObject::destroyed, this, [this](){ thread_plc_ = nullptr; });
    QObject::connect(connector_plc_, &QObject::destroyed, this,[this](){ connector_plc_ = nullptr;});

    QObject::connect(connector_plc_, &PlcConnector::signalActionAlignment, this, &MatchingUI::ActionAlign);
//    QObject::connect(connector_plc_, &PlcConnector::signalActionSetRef, this, &MatchingUI::ActionSetRef);
    QObject::connect(connector_plc_, &PlcConnector::signalState, this, &MatchingUI::UpdatePLCState);
//    QObject::connect(connector_plc_, &PlcConnector::signalResetAllParameters, this, [this](){
//        RecordLastModifiedTime();   // Record closed time. Jimmy 20220518.
//        is_reset_CCDsys_ = true;
//        ResetAllParameters();
//    });
    //QObject::connect(connector_plc_, &PlcConnector::signalActionAlignment, this, &MatchingUI::CheckLightSourceEnableTiming);
    //QObject::connect(connector_plc_, &PlcConnector::signalChangeRecipe, this, &MatchingUI::ChangeRecipe);
    QObject::connect(connector_plc_, &PlcConnector::signalStatus, this, &MatchingUI::UpdatePlcStatus);
    QObject::connect(connector_plc_, &PlcConnector::signalLogs, this, &MatchingUI::PrintPlcLogs);
    // connect(plcConnector, &PLCConnector::plcError, this, &MatchingUI::on_plc_error);
    // connect(plcConnector, &PLCConnector::plcDataReceived, this, &MatchingUI::on_plc_data_received);
    // connect(plcConnector, &PLCConnector::plcDataSent, this, &MatchingUI::on_plc_data_sent);
    // connect(plcConnector, &PLCConnector::plcDataReceived, this, &MatchingUI::on_plc_data_received);
//    QObject::connect(thread_plc_, &QThread::finished, connector_plc_, &QObject::deleteLater);
//    QObject::connect(thread_plc_, &QThread::finished, thread_plc_, &QObject::deleteLater);
//    QObject::connect(thread_plc_, &QObject::destroyed, this, [this](){ thread_plc_ = nullptr; });
//    QObject::connect(connector_plc_, &QObject::destroyed, this,[this](){ connector_plc_ = nullptr;});
//    QObject::connect(connector_plc_, &PlcConnector::signalState, this, &MatchingUI::on_plc_connected);
//    QObject::connect(this, &MatchingUI::signalPlcInit, connector_plc_, &PlcConnector::Initialize);
//    QObject::connect(this, &MatchingUI::signalPlcDisconnect, connector_plc_, &PlcConnector::Disconnect);


//    QObject::connect(connector_plc_, &PlcConnector::signalActionAlignment, this, &MatchingUI::on_plc_actionAlignment);
//    QObject::connect(this, &MatchingUI::signalPlcWriteResult, connector_plc_, &PlcConnector::WriteResult, Qt::BlockingQueuedConnection);


    //=====================================================================

    // Set initial mode and method
    updateModeLabel(ui->actionTrain->text());
    mainPictureBox->setMode(CustomPictureBox::Training);
    updateMethodLabel(ui->actionChamfer->text());

    camL = new CaptureThreadL(this);
    camL->using_default_parameter = true;
//    qRegisterMetaType<cv::Mat>("cv::Mat");
//    QObject::connect(camL, SIGNAL(ProcessedImgL(cv::Mat)), this, SLOT(update_image_mat(cv::Mat)));
    QObject::connect(camL, &CaptureThreadL::frameReady, this, &MatchingUI::update_QImage);
    m_container1.SetUseCudaForImageProcessing(true);
    m_container2.SetUseCudaForImageProcessing(true);
    loadTemplatesInfo();
    ui->lbl_ref_c1->setText(QString("%1-%2").arg(m_trained_template_roi1.center().x()).arg(m_trained_template_roi1.center().y()));
    ui->lbl_ref_c2->setText(QString("%1-%2").arg(m_trained_template_roi2.center().x()).arg(m_trained_template_roi2.center().y()));
    ui->lbl_ref_p->setText(QString("%1-%2").arg((m_trained_template_roi1.center().x() + m_trained_template_roi2.center().x())/2).arg((m_trained_template_roi1.center().y() + m_trained_template_roi2.center().y())/2));
    ui->lbl_ref_a->setText(QString("%1").arg(m_trained_angle));
}

MatchingUI::~MatchingUI()
{
    delete ui;
    if (camL->isRunning()){
        camL->Stop();
    }
    delete camL;
    mutex_shared->unlock();
    delete mutex_shared;    
    emit signalPlcDisconnect(false);
    thread_plc_->quit();
    thread_plc_->wait();
}

void MatchingUI::setupConnections()
{
    // Connect signals from mainPictureBox to slots in MatchingUI
    connect(mainPictureBox, &CustomPictureBox::templatesChanged, this, &MatchingUI::onTemplatesChanged);
    connect(mainPictureBox, &CustomPictureBox::roisChanged, this, &MatchingUI::onRoisChanged);

    // Group actions for Mode
    modeActionGroup = new QActionGroup(this);
    modeActionGroup->addAction(ui->actionTrain);
    modeActionGroup->addAction(ui->actionSetting);
    modeActionGroup->addAction(ui->actionTest);
    modeActionGroup->setExclusive(true);
    connect(modeActionGroup, &QActionGroup::triggered, this, &MatchingUI::onModeChanged);

    // Group actions for Method
    methodActionGroup = new QActionGroup(this);
    methodActionGroup->addAction(ui->actionChamfer);
    methodActionGroup->addAction(ui->actionFastRST);
    methodActionGroup->setExclusive(true);
    connect(methodActionGroup, &QActionGroup::triggered, this, &MatchingUI::onMethodChanged);
    ui->btn_train_data->setEnabled(false);
    ui->btn_load_img->setEnabled(true);
    ui->btn_run->setEnabled(false);

}

void MatchingUI::on_btn_load_img_clicked()
{
//    QString fileName = QFileDialog::getOpenFileName(this,
//                                                    tr("Open Image"), "", tr("Image Files (*.png *.jpg *.bmp)"));
//    if (!fileName.isEmpty()) {
//        QPixmap pixmap(fileName);
//        if(!pixmap.isNull()) {
//            mainPictureBox->setImage(pixmap);

//            ui->lbl_display_1->clear();
//            ui->lbl_display_2->clear();

//            // Load ROIs for the new image
//            loadRois(pixmap.size());
//        }
//    }
    if (camL->isRunning()){

        camL->Stop();

    }
    else {
        QString fileName = QFileDialog::getOpenFileName(this,
                                                       tr("Open Image"), "", tr("Image Files (*.png *.jpg *.bmp)"));
        if (!fileName.isEmpty()) {
            QPixmap pix(fileName);
            lastpixmap = pix.copy();
        }
    }
    setCapture(false);

    if(!lastpixmap.isNull() && camL->isStopped()) {
        mainPictureBox->setImage(lastpixmap);
//        ui->lbl_display_1->clear();
//        ui->lbl_display_2->clear();
        loadRois(lastpixmap.size());
    }
    setTrain(true);

}

void MatchingUI::onModeChanged(QAction *action)
{
    updateModeLabel(action->text());
    if (action == ui->actionTrain) {
        mainPictureBox->setMode(CustomPictureBox::Training);
        setTrain(true);
    } else if (action == ui->actionSetting) {
        mainPictureBox->setMode(CustomPictureBox::Setting);
        setSetting(true);
    } else if (action == ui->actionTest) {
        mainPictureBox->setMode(CustomPictureBox::Test);
        setTrain(true);
    }
}

void MatchingUI::onMethodChanged(QAction *action)
{
    updateMethodLabel(action->text());
    // Set MatchingModel for m_container1 and m_container2 based on selected action
    if (action == ui->actionChamfer) {
        m_container1.SetMode(MatchingMode::CHAMFER);
        m_container2.SetMode(MatchingMode::CHAMFER);
    } else if (action == ui->actionFastRST) {
        m_container1.SetMode(MatchingMode::FASTRST);
        m_container2.SetMode(MatchingMode::FASTRST);
    }
    // Add logic for method change here
    qDebug() << "Method changed to:" << action->text();
}

void MatchingUI::updateModeLabel(const QString &mode)
{
    ui->lbl_mode_value->setText(mode);
}

void MatchingUI::updateMethodLabel(const QString &method)
{
    ui->lbl_method_value->setText(method);
}

void MatchingUI::onTemplatesChanged(const QMap<int, CustomPictureBox::TemplateData> &templates)
{
    ui->log_text_edit->append(QString("Template list updated. Count: %1").arg(templates.size()));
    // The labels are only cleared when a new image is loaded, preserving them across mode changes.
    ui->lbl_display_1->clear();
    ui->lbl_display_2->clear();

    bool has_template = false;

    if (templates.contains(0)) {
        m_template_pixmap1 = templates[0].image.copy();
        template_roi_1 = templates[0].roi;
        m_template_mat1 = QPixmapToCVMat(m_template_pixmap1);
        m_template_pixmap1.save("template_L.png");
        ui->lbl_display_1->setPixmap(templates[0].image.scaled(
            ui->lbl_display_1->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        has_template = true;
    }
    else{
        m_template_mat1.release();
    }
    if (templates.contains(1)) {
        m_template_pixmap2 = templates[1].image.copy();
        template_roi_2 = templates[1].roi;
        m_template_mat2 = QPixmapToCVMat(m_template_pixmap2);
        m_template_pixmap2.save("template_R.png");
        ui->lbl_display_2->setPixmap(templates[1].image.scaled(
            ui->lbl_display_2->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        has_template = true;
    }
    else {
        m_template_mat2.release();
    }
    if (has_template) {
        setTrain(true);
    }
}

void MatchingUI::onRoisChanged(const QMap<int, CustomPictureBox::TemplateData> &rois)
{
    ui->log_text_edit->append(QString("ROI list updated. Count: %1").arg(rois.size()));
    m_roi_rect1 = QRect();
    m_roi_rect2 = QRect();
    for (const auto& roiData : rois.values()) {
        ui->log_text_edit->append(QString(" - ROI %1: [%2, %3, %4x%5]")
                                    .arg(roiData.id + 1)
                                    .arg(roiData.roi.x())
                                    .arg(roiData.roi.y())
                                    .arg(roiData.roi.width())
                                    .arg(roiData.roi.height()));
        if (roiData.id == 0) {
            //ROIpx1 = roiData.image.copy();
            //roi_rect_1 = roiData.roi;
            m_roi_rect1 = roiData.roi;
        }
        if (roiData.id == 1) {
            //ROIpx2 = roiData.image.copy();
            //roi_rect_2 = roiData.roi;
            m_roi_rect2 = roiData.roi;
        }
    }
    // Save ROIs whenever they are changed
    saveRois(rois);
}

void MatchingUI::saveRois(const QMap<int, CustomPictureBox::TemplateData> &rois)
{
    QSize imageSize = mainPictureBox->getOriginalImageSize();
    if (imageSize.isEmpty()) {
        qDebug() << "Cannot save ROIs, original image size is invalid.";
        return;
    }

    QString roiFilePath = "rois.xml";
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

void MatchingUI::loadRois(const QSize &imageSize)
{
    QMap<int, CustomPictureBox::TemplateData> loadedRois;
    QString roiFilePath = "rois.xml";
    QFile file(roiFilePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "Could not open ROI file for reading:" << roiFilePath;
        return;
    }

    QXmlStreamReader xml(&file);
    while (!xml.atEnd() && !xml.hasError()) {
        QXmlStreamReader::TokenType token = xml.readNext();
        if (token == QXmlStreamReader::StartElement) {
            if (xml.name().toString() == "roi") {
                int id = xml.attributes().value("id").toInt();
                double relX = xml.attributes().value("x").toDouble();
                double relY = xml.attributes().value("y").toDouble();
                double relW = xml.attributes().value("w").toDouble();
                double relH = xml.attributes().value("h").toDouble();

                QRect roiRect(
                    qRound(relX * imageSize.width()),
                    qRound(relY * imageSize.height()),
                    qRound(relW * imageSize.width()),
                    qRound(relH * imageSize.height())
                );

                CustomPictureBox::TemplateData data;
                data.id = id;
                data.roi = roiRect;
                data.name = QString("ROI %1").arg(id + 1);

                loadedRois[id] = data;
            }
        }
    }

    if (xml.hasError()) {
        qDebug() << "XML error while reading ROIs:" << xml.errorString();
    }

    if (!loadedRois.isEmpty()) {
        mainPictureBox->setRois(loadedRois);
        ui->log_text_edit->append("Loaded ROIs from " + roiFilePath);
    }
}

void MatchingUI::on_btn_connect_clicked()
{
    try {
        mutex_shared = new QMutex();
        camL->mutex_shared = mutex_shared;
        camL->Play();
        setCapture(true);
        ui->statusBar->showMessage("Connect Camera", 5000);
    }
    catch (const std::exception& e) {
        qDebug() << "Error in on_btn_connect_clicked:" << e.what();
    }
}

void MatchingUI::update_image_mat(cv::Mat img) {
    if (camL->isRunning()){
        QImage qimg = QImage((const uchar*)img.data, img.cols, img.rows, img.step, QImage::Format_RGB888).rgbSwapped();
    //    if(!qimg.isNull()) {
    //        mainPictureBox->setImage(QPixmap::fromImage(qimg));
    //    }

        QPixmap pixmap = QPixmap::fromImage(qimg);
        if(!pixmap.isNull()) {
            mainPictureBox->setImage(pixmap);
    //        loadRois(pixmap.size());
        }

        lastpixmap = pixmap.copy();
    }

}

void MatchingUI::update_QImage(QImage img) {
    if (camL->isRunning()) {
        QPixmap pixmap = QPixmap::fromImage(img);
        if(!pixmap.isNull()) {
            mainPictureBox->setImage(pixmap);
        }
        lastpixmap = pixmap.copy();
        camL->frame_in_process = false;
    }
}

void MatchingUI::setTrain(const bool &train){
    ui->btn_train_data->setEnabled(train);
    ui->btn_run->setEnabled(train);
}

void MatchingUI::setSetting(const bool &setting){
    ui->btn_train_data->setEnabled(!setting);
    ui->btn_run->setEnabled(!setting);
}

void MatchingUI::setCapture(const bool &capture){
    ui->btn_connect->setEnabled(!capture);
    //ui->btn_load_img->setEnabled(capture);
    setTrain(ui->actionTrain->isChecked());
}

void MatchingUI::on_btn_train_data_clicked()
{
    this->setEnabled(false);
    ui->log_text_edit->append("Starting training process...");
    QCoreApplication::processEvents();

    // Store the template ROIs right before training and save them
    if (!m_template_mat1.empty()) {
        m_trained_template_roi1 = template_roi_1;
    }
    if (!m_template_mat2.empty()) {
        m_trained_template_roi2 = template_roi_2;
    }
    saveTemplatesInfo();

    bool trained_something = false;
    float total_time_exec = 0;

    ui->log_text_edit->append("Processing Template 1 ...");
    QCoreApplication::processEvents();
    float time1 = m_container1.Train(m_template_mat1);
    if (time1 > 0) {
        ui->log_text_edit->append(QString("Template1 trained/loaded successfully in %1 ms").arg(time1 * 1000, 0, 'f', 1));
        QPixmap pix = QPixmap::fromImage(QImage(m_container1.GetTrainedTemplateImg().data,
                                                m_container1.GetTrainedTemplateImg().cols,
                                                m_container1.GetTrainedTemplateImg().rows,
                                                m_container1.GetTrainedTemplateImg().step,
                                                QImage::Format_RGB888).rgbSwapped());
        if (!pix.isNull()) {
            ui->lbl_display_1->setPixmap(pix.scaled(ui->lbl_display_1->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        }
        trained_something = true;
        total_time_exec += time1;
    } else {
        ui->log_text_edit->append("Failed to train/ load data from Template 1");
    }

    // Train Template 2
    ui->log_text_edit->append("Processing Template 2 ...");
    QCoreApplication::processEvents();
    float time2 = m_container2.Train(m_template_mat2);
    if (time2 > 0) {
        ui->log_text_edit->append(QString("Template2 trained/loaded successfully in %1 ms").arg(time2 * 1000, 0, 'f', 1));
        QPixmap pix = QPixmap::fromImage(QImage(m_container2.GetTrainedTemplateImg().data,
                                                m_container2.GetTrainedTemplateImg().cols,
                                                m_container2.GetTrainedTemplateImg().rows,
                                                m_container2.GetTrainedTemplateImg().step,
                                                QImage::Format_RGB888).rgbSwapped());
        if (!pix.isNull()) {
            ui->lbl_display_2->setPixmap(pix.scaled(ui->lbl_display_2->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        }
        trained_something = true;
        total_time_exec += time2;
    } else {
        ui->log_text_edit->append("Failed to train/ load data from Template 2");
    }

    ui->log_text_edit->append(QString("Total execution time: %1 ms").arg(total_time_exec * 1000, 0, 'f', 1));
    //Check if both models are ready
    ui->log_text_edit->append(QString("Total execution time: %1 ms").arg(total_time_exec * 1000, 0, 'f', 1));

    // if (m_container1.IsTrainedOK() && m_container2.IsTrainedOK()) {
    //     ui->log_text_edit->append("Both models are ready for testing");
    //     setTrain(false);
    // } else {
    //     ui->log_text_edit->append("One or both models are not ready. Please define templates and train again.");
    //     setTrain(true);
    // }

    this->setEnabled(true);
    ui->log_text_edit->append("Training process finished");

}

void MatchingUI::on_btn_run_clicked()
{
    if (mainPictureBox->getOriginalImageSize().isEmpty()) {
        ui->log_text_edit->append("[ERROR] No image load to run test on");
        return;
    }
    if (m_roi_rect1.isNull() && m_roi_rect2.isNull()) {
        ui->log_text_edit->append("[ERROR] No ROIs defined for tesing.");
        return;
    }
    mainPictureBox->setImage(lastpixmap);
    ui->log_text_edit->append("Running inference...");
    QCoreApplication::processEvents();

    this->setEnabled(false);
    cv::Mat source_mat = QPixmapToCVMat(lastpixmap);
    cv::Mat display_mat = source_mat.clone();
    // cv::Mat src_roi1 = source_mat(cv::Rect(m_roi_rect1.x(), m_roi_rect1.y(), m_roi_rect1.width(), m_roi_rect1.height()));
    // cv::Mat src_roi2 = source_mat(cv::Rect(m_roi_rect2.x(), m_roi_rect2.y(), m_roi_rect2.width(), m_roi_rect2.height()));
    float time1 = 0;
    float time2 = 0;
    //Test ROI1
    // Test ROI1
    cv::Mat src_roi1 = source_mat(cv::Rect(m_roi_rect1.x(), m_roi_rect1.y(), m_roi_rect1.width(), m_roi_rect1.height()));
    time1 = m_container1.Test(src_roi1);
    std::vector<cv::Point> result1 = m_container1.GetResult();

    // Test ROI2
    cv::Mat src_roi2 = source_mat(cv::Rect(m_roi_rect2.x(), m_roi_rect2.y(), m_roi_rect2.width(), m_roi_rect2.height()));
    time2 = m_container2.Test(src_roi2);
    std::vector<cv::Point> result2 = m_container2.GetResult();

    // --- Process and draw results ---
    bool result1_ok = !result1.empty();
    bool result2_ok = !result2.empty();

    if (result1_ok) {
        drawResult(display_mat, result1, m_roi_rect1);
    }
    if (result2_ok) {
        drawResult(display_mat, result2, m_roi_rect2);
    }

    if (!result1_ok || !result2_ok) {
        ui->log_text_edit->append("Cannot detect 1 or 2 object");
        emit signalPlcWriteResult(0, 0, 0, 2, PlcWriteMode::kOthers);
    } else {
        ui->log_text_edit->append("Analysis the result ....");
        cv::Point center1 = result1.at(0);
        cv::Point center2 = result2.at(0);
        QPoint trained_center1 = m_trained_template_roi1.center();
        QPoint trained_center2 = m_trained_template_roi2.center();
        QPoint trained_center = QPoint((trained_center1.x() + trained_center2.x()) / 2.0f, (trained_center1.y() + trained_center2.y()) / 2.0f);

        // Calculate the center of the found object
        QPoint found_center1(center1.x + m_roi_rect1.x(), center1.y + m_roi_rect1.y());
        QPoint found_center2(center2.x + m_roi_rect2.x(), center2.y + m_roi_rect2.y());
        QPoint found_center = QPoint((found_center1.x() + found_center2.x()) / 2.0f, (found_center1.y() + found_center2.y()) / 2.0f);


        QPoint delta = found_center - trained_center;
        float avg_dx = delta.x();
        float avg_dy = delta.y();

        float angle_rad = atan2(found_center1.y() - found_center2.y(), found_center1.x() - found_center2.x());
        float avg_angle_dev = (angle_rad + M_PI/2.0) * 180.0 / M_PI;
        
        ui->log_text_edit->append(QString(">> mean Transit: (dx: %1, dy: %2)").arg(avg_dx, 0, 'f', 2).arg(avg_dy, 0, 'f', 2));
        ui->log_text_edit->append(QString(">> Average deviation angle: %1 dgree").arg(avg_angle_dev, 0, 'f', 2));
        QCoreApplication::processEvents();
        // Draw the avg result
        
        cv::Point avg_trained_center_cv(trained_center.x(), trained_center.y());

        // Draw arrow
        cv::Point offset_dest_point_cv(avg_trained_center_cv.x + avg_dx,
                                       avg_trained_center_cv.y + avg_dy);
        cv::arrowedLine(display_mat, avg_trained_center_cv, offset_dest_point_cv, cv::Scalar(0, 255, 0), 10, cv::LINE_AA);

        cv::line(display_mat, cv::Point(offset_dest_point_cv.x - 30, offset_dest_point_cv.y), cv::Point(offset_dest_point_cv.x + 30, offset_dest_point_cv.y), cv::Scalar(255, 0, 0), 10);
        cv::line(display_mat, cv::Point(offset_dest_point_cv.x, offset_dest_point_cv.y - 30), cv::Point(offset_dest_point_cv.x, offset_dest_point_cv.y + 30), cv::Scalar(255, 0, 0), 10);

        // float final_angle_rad = (-M_PI/2.0) + (avg_angle_dev * M_PI / 180.0);
        // int line_length = 100;
        // cv::Point angle_dest_point_cv(
        //             offset_dest_point_cv.x + line_length * std::cos(final_angle_rad),
        //             offset_dest_point_cv.y + line_length + std::sin(final_angle_rad));
        //cv::line(display_mat, offset_dest_point_cv, angle_dest_point_cv, cv::Scalar(128, 255, 255), 10, cv::LINE_AA);
        
        if(ui->actionTrain->isChecked()){
            m_trained_angle = avg_angle_dev;
            ui->lbl_ref_c1->setText(QString("%1-%2").arg(m_trained_template_roi1.center().x()).arg(m_trained_template_roi1.center().y()));
            ui->lbl_ref_c2->setText(QString("%1-%2").arg(m_trained_template_roi2.center().x()).arg(m_trained_template_roi2.center().y()));
            ui->lbl_ref_p->setText(QString("%1-%2").arg((m_trained_template_roi1.center().x() + m_trained_template_roi2.center().x())/2.0f).arg((m_trained_template_roi1.center().y() + m_trained_template_roi2.center().y())/2.0f));
            ui->lbl_ref_a->setText(QString("%1").arg(m_trained_angle, 0, 'f', 2));
            saveTemplatesInfo();
        }
        else {
            ui->lbl_test_c1->setText(QString("%1-%2").arg(found_center1.x()).arg(found_center1.y()));
            ui->lbl_test_c2->setText(QString("%1-%2").arg(found_center2.x()).arg(found_center2.y()));
            ui->lbl_test_p->setText(QString("%1-%2").arg((found_center1.x() + found_center2.x())/2.0f, 0, 'f', 2).arg((found_center1.y() + found_center2.y())/2.0f, 0, 'f', 2));
            ui->lbl_test_a->setText(QString("%1").arg(avg_angle_dev, 0, 'f', 2));
            float delta_angle = m_trained_angle - avg_angle_dev;

            AlignmentResult alignRes = Alignment::compute(trained_center, found_center, delta_angle, focusx, focusy, a);
            float avg_dx = alignRes.dx_robot;
            float avg_dy = alignRes.dy_robot;
            delta_angle = alignRes.angle_deg;

            ui->lbl_offsetX->setText(QString("%1").arg(avg_dx, 0, 'f', 2));
            ui->lbl_offsetY->setText(QString("%1").arg(avg_dy, 0, 'f', 2));
            
            ui->lbl_offsetAngle->setText(QString("%1").arg(delta_angle, 0, 'f', 2));


            if (isconnectPLC) {
                int32_t int_dx = 0, int_dy=0, int_dev = 0;
                int_dx = (int32_t)avg_dx;
                int_dy = (int32_t)avg_dy;
                int_dev = (int32_t)avg_angle_dev;
                emit signalPlcWriteResult(avg_dx*100, avg_dy*100, delta_angle*100, 1, PlcWriteMode::kOthers);
    //            emit signalPlcWriteResult(int_dx, int_dy, int_dev, 1, PlcWriteMode::kPulMovUVW);
            }
        }

    }
    ui->log_text_edit->append(QString("Inference Time: %1 ms").arg((time1 + time2)* 1000, 0, 'f', 2));

    QPixmap pix = QPixmap::fromImage(QImage(display_mat.data, display_mat.cols, display_mat.rows, display_mat.step, QImage::Format_RGB888).rgbSwapped());
    mainPictureBox->setImage(pix);
    //emit signalPlcWriteResult(avg_dx, avg_dy, avg_angle_dev, 1, PlcWriteMode::kOthers);

    this->setEnabled(true);
    ui->btn_connect->setEnabled(true);

}

void MatchingUI::drawResult(cv::Mat &image, const std::vector<cv::Point> &points, const QRect &roi)
{
    if (points.empty()) return;

    std::vector<cv::Point> global_points;
    for(const auto& pt : points) {
        global_points.push_back(cv::Point(pt.x + roi.x(), pt.y + roi.y()));
    }

    // Draw using the same logic as before
    //cv::circle(image, global_points.at(0), 5, cv::Scalar(0, 0, 255), -1);
    // Draw center cross
    MatchingMode mode = m_container1.GetMode();
    switch(mode){
    case MatchingMode::CHAMFER:
    {
        cv::line(image, global_points.at(0), global_points.at(1), cv::Scalar(0, 255, 0), 5);
        cv::line(image, cv::Point(global_points.at(0).x - 30, global_points.at(0).y), cv::Point(global_points.at(0).x + 30, global_points.at(0).y), cv::Scalar(255, 0, 0), 5);
        cv::line(image, cv::Point(global_points.at(0).x, global_points.at(0).y - 30), cv::Point(global_points.at(0).x, global_points.at(0).y + 30), cv::Scalar(255, 0, 0), 5);
        if (global_points.size() >= 6) { // Ensure there are enough points for the rectangle
            cv::line(image, global_points.at(2), global_points.at(3), cv::Scalar(0, 255, 255), 5);
            cv::line(image, global_points.at(3), global_points.at(4), cv::Scalar(0, 255, 255), 5);
            cv::line(image, global_points.at(4), global_points.at(5), cv::Scalar(0, 255, 255), 5);
            cv::line(image, global_points.at(5), global_points.at(2), cv::Scalar(0, 255, 255), 5);
        }
        break;
    }
    case MatchingMode::FASTRST: {
        cv::line(image, cv::Point(global_points.at(0).x - 30, global_points.at(0).y), cv::Point(global_points.at(0).x + 30, global_points.at(0).y), cv::Scalar(255, 0, 0), 5);
        cv::line(image, cv::Point(global_points.at(0).x, global_points.at(0).y - 30), cv::Point(global_points.at(0).x, global_points.at(0).y + 30), cv::Scalar(255, 0, 0), 5);
        if (global_points.size() >= 6) { // Ensure there are enough points for the rectangle
            //cv::line(image, global_points.at(1), global_points.at(2), cv::Scalar(0, 255, 255), 5);
            //cv::line(image, global_points.at(2), global_points.at(4), cv::Scalar(0, 255, 255), 5);
            //cv::line(image, global_points.at(4), global_points.at(3), cv::Scalar(0, 255, 255), 5);
            //cv::line(image, global_points.at(3), global_points.at(1), cv::Scalar(0, 255, 255), 5);
			cv::line(image, global_points.at(1), global_points.at(2), cv::Scalar(0, 255, 255), 5);
            cv::line(image, global_points.at(2), global_points.at(3), cv::Scalar(0, 255, 255), 5);
            cv::line(image, global_points.at(3), global_points.at(4), cv::Scalar(0, 255, 255), 5);
            cv::line(image, global_points.at(4), global_points.at(1), cv::Scalar(0, 255, 255), 5);
        }
        break;
    }
    }

}

void MatchingUI::saveTemplatesInfo()
{
    QString path = "templates.xml";
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
    ui->log_text_edit->append("Saved template positions to " + path);
}

void MatchingUI::loadTemplatesInfo()
{
    QString path = "templates.xml";
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "Could not open templates file for reading:" << path;
        return;
    }

    QXmlStreamReader xml(&file);
    while (!xml.atEnd() && !xml.hasError()) {
        QXmlStreamReader::TokenType token = xml.readNext();
        if (token == QXmlStreamReader::StartElement) {
            if (xml.name().toString() == "template") {
                int id = xml.attributes().value("id").toInt();
                int x = xml.attributes().value("x").toInt();
                int y = xml.attributes().value("y").toInt();
                int w = xml.attributes().value("w").toInt();
                int h = xml.attributes().value("h").toInt();

                if (id == 0) {
                    m_trained_template_roi1.setRect(x, y, w, h);
                } else if (id == 1) {
                    m_trained_template_roi2.setRect(x, y, w, h);
                }
            } else if (xml.name().toString() == "angle") {
                m_trained_angle = xml.attributes().value("angle").toFloat();
            }
        }
    }

    if (xml.hasError()) {
        qDebug() << "XML error while reading templates:" << xml.errorString();
    } else {
        if (!m_trained_template_roi1.isNull() || !m_trained_template_roi2.isNull())
            ui->log_text_edit->append("Loaded template positions from " + path);
    }
    file.close();
}

void MatchingUI::on_btn_connect_plc_clicked()
{
    // Add PLC connection logic here
//    QString szIp = ui->le_ip->text();
//    int szPort = (ui->le_port->text()).toInt();
    // QString szRack = ui->le_rack->text();
    // QString szSlot = ui->le_slot->text();
    // QString szName = ui->le_plc_name->text();
    // QString szPassword = ui->le_plc_password->text();
    
    //connector_plc_->SetPlcIpPort(szIp, szPort);

//    m_plc_thread->start();
    thread_plc_->start();
    emit signalPlcInit();
}

void MatchingUI::UpdatePLCState(bool state)
{
    isconnectPLC = state;
    if (state){
        ui->btn_connect_plc->setStyleSheet("background-color: green");
    }
    else {
        ui->btn_connect_plc->setStyleSheet("background-color: rgb(52, 67, 104);color: rgb(255, 0, 0);");
    }
}
void MatchingUI::ActionAlign() {
    if (isconnectPLC && ui->cb_start->isChecked()) {
        ui->log_text_edit->append("Start Alignment");
        on_btn_connect_clicked();
        SleepByQtimer(100);
        on_btn_load_img_clicked();
        SleepByQtimer(100);
        on_btn_run_clicked();
        //std::cout << "Start Alignment" << std::endl;
    }
    return;
}

void MatchingUI::SleepByQtimer(unsigned int timeout_ms)
{
    bool isTimeup = false;
    QTimer *timer = new QTimer(this);
    timer->setSingleShot(true);
    timer->setInterval(timeout_ms);

    QObject::connect(timer, &QTimer::timeout, [&isTimeup, &timer]() {
        isTimeup = true;
    } );
    timer->start();
    while (!isTimeup){
        QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
    }

    timer->deleteLater();
    return;
}

void MatchingUI::on_btn_send_clicked()
{
    QString adrress = ui->le_address->text();
    int32_t value1 = ui->le_value->text().toInt();
    int32_t value2 = ui->le_type_plc->text().toInt();
    int32_t value3 = ui->le_plc_status->text().toInt();
    emit signalPlcWriteResult(value1, value2, value3, 1, PlcWriteMode::kOthers);

    //connector_plc_->WriteResult(value1, value2, value3, 1, PlcWriteMode::kOthers);

}
