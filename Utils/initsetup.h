#ifndef INITSETUP_H
#define INITSETUP_H

#include <QString>
#include "../FileIO/fileio.h" // Re-use existing file IO

namespace fio = file_io;

/**
 * @brief Manages loading and saving of UI and application settings from/to an INI file.
 *
 * This class handles all configuration paths, modes, and settings for the application.
 * It provides default values and ensures that necessary directories exist.
 */
class InitSetup
{
public:
    InitSetup();

    /**
     * @brief Loads settings from the specified INI file.
     * @param path The path to the settings file. Defaults to "uiSetting.ini".
     *
     * If the file does not exist, it will be created with default values.
     * It also checks and creates necessary directories.
     */
    void LoadSettings(const QString& path = "uiSetting.ini");

    /**
     * @brief Saves the current settings to the specified INI file.
     * @param path The path to the settings file. Defaults to "uiSetting.ini".
     */
    void SaveSettings(const QString& path = "uiSetting.ini");

    // --- Getters ---
    QString getMode() const { return m_mode; }
    QString getMethod() const { return m_method; }
    QString getPathSaveCapture() const { return m_path_save_capture; }
    QString getPathSaveTemplate() const { return m_path_save_template; }
    QString getPathSaveRoi() const { return m_path_save_roi; }
    QString getPathSaveResult() const { return m_path_save_result; }
    QString getPathSaveLog() const { return m_path_save_log; }
    QString getPathCalibration() const { return m_path_calibration; }
    bool getSaveLogEnabled() const { return m_save_log; }
    bool getSaveResultEnabled() const { return m_save_result; }

    // --- Setters ---
    void setMode(const QString& mode) { m_mode = mode; }
    void setMethod(const QString& method) { m_method = method; }

private:
    /**
     * @brief Checks if a directory exists and creates it if it doesn't.
     * @param path The full path of the directory to check.
     */
    void checkAndCreateDirectory(const QString& path);

    QString m_filePath;

    // Settings with default values
    QString m_mode;
    QString m_method;
    QString m_path_save_capture;
    QString m_path_save_template;
    QString m_path_save_roi;
    QString m_path_save_result;
    QString m_path_save_log;
    QString m_path_calibration;
    bool m_save_log;
    bool m_save_result;
};

#endif // INITSETUP_H 