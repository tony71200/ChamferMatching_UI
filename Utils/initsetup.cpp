#include "initsetup.h"
#include <QDir>
#include <QDebug>
#include <QFileInfo>

InitSetup::InitSetup()
{
    // Default values
    m_mode = "Test";
    m_method = "Chamfer";
    m_path_save_capture = "Capture/";
    m_path_save_template = "CaptureSetting/templates.xml";
    m_path_save_roi = "CaptureSetting/rois.xml";
    m_path_save_result = "Result/";
    m_path_save_log = "Log/";
    m_path_calibration = "CameraCalibration.json";
    m_save_log = true;
    m_save_result = true;
}

void InitSetup::checkAndCreateDirectory(const QString& path)
{
    QFileInfo fileInfo(path);
    QDir dir = fileInfo.dir(); // Gets the directory containing the file
    if (dir.path() == ".") return; // Don't create for files in the root

    if (!dir.exists()) {
        if (dir.mkpath(".")) {
            qDebug() << "Created directory:" << dir.absolutePath();
        } else {
            qDebug() << "Failed to create directory:" << dir.absolutePath();
        }
    }
}


void InitSetup::LoadSettings(const QString& path)
{
    m_filePath = path;
    if (!fio::exists(path.toStdString())) {
        qDebug() << "UI settings file not found, creating with default values.";
        SaveSettings(path); // Save defaults if file doesn't exist
        return;
    }

    fio::FileIO fs(path.toStdString(), fio::FileioFormat::R);
    if (fs.isEmpty()) {
        qDebug() << "UI settings file is empty, using default values.";
        fs.close();
        SaveSettings(path);
        return;
    }

    std::string found_data;
    int found_num;

    found_data = fs.Read("UIMode");
    if(fs.isOK()) m_mode = QString::fromStdString(found_data);

    found_data = fs.Read("UIMethod");
    if(fs.isOK()) m_method = QString::fromStdString(found_data);

    found_data = fs.Read("UIPathSaveCapture");
    if(fs.isOK()) m_path_save_capture = QString::fromStdString(found_data);

    found_data = fs.Read("UIPathSaveTemplate");
    if(fs.isOK()) m_path_save_template = QString::fromStdString(found_data);

    found_data = fs.Read("UIPathSaveRoi");
    if(fs.isOK()) m_path_save_roi = QString::fromStdString(found_data);

    found_data = fs.Read("UIPathSaveResult");
    if(fs.isOK()) m_path_save_result = QString::fromStdString(found_data);

    found_data = fs.Read("UIPathSaveLog");
    if(fs.isOK()) m_path_save_log = QString::fromStdString(found_data);

    found_data = fs.Read("UIPathCalibration");
    if(fs.isOK()) m_path_calibration = QString::fromStdString(found_data);

    found_num = fs.ReadtoInt("UISaveLog");
    if(fs.isOK()) m_save_log = static_cast<bool>(found_num);

    found_num = fs.ReadtoInt("UISaveResult");
    if(fs.isOK()) m_save_result = static_cast<bool>(found_num);

    fs.close();

    // Check and create directories
    checkAndCreateDirectory(m_path_save_capture);
    checkAndCreateDirectory(m_path_save_template);
    checkAndCreateDirectory(m_path_save_roi);
    checkAndCreateDirectory(m_path_save_result);
    checkAndCreateDirectory(m_path_save_log);
}

void InitSetup::SaveSettings(const QString& path)
{
    m_filePath = path;
    fio::FileIO fs(path.toStdString(), fio::FileioFormat::W);

    fs.Write("UIMode", m_mode.toStdString());
    fs.Write("UIMethod", m_method.toStdString());
    fs.Write("UIPathSaveCapture", m_path_save_capture.toStdString());
    fs.Write("UIPathSaveTemplate", m_path_save_template.toStdString());
    fs.Write("UIPathSaveRoi", m_path_save_roi.toStdString());
    fs.Write("UIPathSaveResult", m_path_save_result.toStdString());
    fs.Write("UIPathSaveLog", m_path_save_log.toStdString());
    fs.Write("UIPathCalibration", m_path_calibration.toStdString());
    fs.Write("UISaveLog", std::to_string(m_save_log));
    fs.Write("UISaveResult", std::to_string(m_save_result));

    fs.close();
    qDebug() << "UI settings saved to" << path;
}
