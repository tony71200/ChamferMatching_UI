#ifndef PLCCONNECTOR_H
#define PLCCONNECTOR_H

#include <iostream>
#include <fstream>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>
#include <cmath>
#include <ctime>

#include <unistd.h> // UNIX
#include <sys/sysinfo.h>

#include <QThread>
#include <QMutex>

#include <QTcpServer>
#include <QTcpSocket>
#include <QAbstractSocket>
#include <QCoreApplication>

#include <QObject>
#include <QDebug>
#include <QString>
#include <QMetaType>

#include <QFileDialog>
#include <QFileInfo>
#include <QDir>

#include <QTimer>
#include <QTime>

#include <opencv2/highgui/highgui.hpp>
#include <opencv2/core.hpp>

#include "./Log/logwriter.h"
#include "./FileIO/fileiomanager.h"
#include "./FileIO/fileio.h"
#include "./FileIO/inimanager.h"
#include "globalparms.h"

// DEFINITION
#define PACKET_SIZE 1024
#define READ_REQ_LENGTH_WORD       1
#define READ_REQ_6_WORD            6	//garly_20220224, add for auto update time from plc
#define WORD_BYTE_UNIT             4
#define ENDCODE_BYTE_UNIT          4
#define ENDCODE_VALUE              0
#define WRT_CORECT_RETURN_LENGTH   4
#define WRT_RETRY_TIMES            10

#define ENDCODE_BYTE_UNIT_BINARY   2
#define WORD_BYTE_UNIT_BINARY      2

namespace fm = file_io_manager;
namespace im = ini_manager;
namespace fio = file_io;

enum _PLC_Status_
{
    _PLC_IDLE_ = 0,
    _PLC_BUSY_,

    _PLC_READ_PLC_TRIG_,
    _PLC_READ_PLC_CMD_,
    _PLC_READ_RECIPE_NUM_,
    _PLC_READ_CURRENT_TIME_,
    _PLC_READ_PANEL_ID_,    //garly_20240930, add panel id
    _PLC_READ_D5060_,

    _PLC_WRITE_CCD_TRIG_,
    _PLC_WRITE_CCD_RESULT_, //8
    _PLC_WRITE_PLC_TRIG_,   //9
    _PLC_WRITE_RECIPE_NUM_,
    _PLC_WRITE_BARCODE_RESULT_,	//garly_20220309
    _PLC_WRITE_ERROR_CODE_,   //garly_20220617
    _PLC_WRITE_DEFECT_DETECTION_
};

// For PLC move direction. Jimmy, 20220504
enum class PlcWriteMode {
    kPulMovYT = 0,
    kPulMovUVW,
    kPulRotYT,
    kPulRotUVW,
    kMmDegMov,
    kMmDegRot,
    kOthers
};

typedef struct
{
    int32_t value1;
    int32_t value2;
    int32_t value3;
#ifdef BARCODE_READER	//garly_20220309
    int32_t value4;
    int32_t value5;
#endif
    int32_t result;
}WRT_RESULT_DATA;

typedef struct
{
    QString result_l;
    int defect_num_l;

    QString result_r;
    int defect_num_r;
}DDT_RESULT_DATA;

class PlcConnector : public QObject
{
    Q_OBJECT
public:
    explicit PlcConnector(QObject *parent = nullptr);

private:
    QString proj_path_;
    QString recipe_path_;

    // connection
    QTcpSocket *socket_;
    QString ip_;
    int port_;
    QString addr_plc_trigger_;
    QString addr_ccd_trigger_;
    QString addr_cmd_;
    QString addr_recipe_num_read_;
    QString addr_recipe_num_write_;
    QString addr_result_;
    QString addr_current_time_;
    QString addr_error_code_write_;  //garly_20220617
    QString addr_panelId_;  //garly_20240930, add for panel id
    QString addr_defect_detection_write_;

    // PLC direction. Jimmy, 20220503.
    bool is_plc_swap_xy_;
    int plc_sign_x_;
    int plc_sign_y_;
    int plc_sign_theta_;

#ifdef BARCODE_READER   //garly_20220309
    QString addr_barcode_result_;
#endif

    // Timer
    QTimer *timer_;
    const int kTimeout_ = 50;   //ms
#ifdef SYNC_TIME_FROM_PLC
    QTimer *timer_autoSyncTime_;  //auto sync current time from plc
    const int autoSyncTime_timeout = 60*1000;  //ms
#endif

    // PLC status
    uint16_t st_plc_;
    bool isWaiting_plc_ack;
    const int kWaitTimeout = 3000;  //ms

    uint8_t format_PLC_ = _PLC_Format_Binary_;   //add plc format change
    uint8_t time_retryWriteCommand = 0; //garly_20220808, add for retry write when receive write fail from plc
    uint16_t plc_trigger_ = 0;
    uint16_t ccd_trigger_ = 0;   // default : d5190
    uint16_t CurrentRecipeNum = 0;
    uint16_t CurrentWriteCommandMode = _PLC_IDLE_;  //garly_20220808, add for retry write when receive write fail from plc
    QString CurrentErrorCode = "\0";  //garly_20220617
    WRT_RESULT_DATA * wrtResultData = new WRT_RESULT_DATA();
#ifdef DEFECT_DETECTION
    DDT_RESULT_DATA * ddtResultData = new DDT_RESULT_DATA();
#endif

    bool isConnected;
    bool isReconnected;
#ifdef SYNC_TIME_FROM_PLC
    bool enableSyncTime;
    int syncTimeUnit = 0;    //default : disbale
    int autoUpdateCounter = 0;
#endif

    QMutex mlock_;
    logwriter::LogWriter lw;

    // for debug
    int db_skip_msg_read;

    // dm::plc_log
    bool isBufLogCanUpdate = false;
    QByteArray log_buf_;
    QByteArray log_req_;

    // Functions
    void Connect();     // Do connect to plc
    void Sleep(int ms) { QThread::currentThread()->msleep(ms); }

    void PLCSystemLog(QString str) { if (lw.IsReady()) lw << str.toStdString() << lw.endl; }
    void WaitLock() { mlock_.lock(); isWaiting_plc_ack = true; }
    void WaitUnlock() { isWaiting_plc_ack = false; mlock_.unlock(); }
    bool IsReady() const { return !isWaiting_plc_ack; }
    bool WaitForPlcReady(unsigned int timeout);
    void ReadPlcTrig();
#ifdef SYNC_TIME_FROM_PLC
    void ReadCurrentTime();
#endif
    void ReadPanelId(); //garly_20240930, add for panel id
    void ProcessPlcData(QByteArray);
    void PlcReadCommand(uint16_t mode, QString block_addr);
    void PlcWriteCommand(uint16_t mode, QString block_addr);

    QString ConvertDec2Hex(int dec, int bit);

private slots:
    void Run(); // Keep running to listen and act
#ifdef SYNC_TIME_FROM_PLC
    void RunSyncCurrentTime();
#endif
    void UpdateState(QAbstractSocket::SocketState st);
    void ProcessData();
    void ReconnectAfterDisconnect() { Disconnect(true); }

signals:
    void signalReceivePlcData(QByteArray);
    void signalActionCalibration(); // add by garly
    void signalActionAlignment();   // add by garly
    void signalActionSetRef(int);
    void signalReadPlcTrigCmd();
    void signalResetAllParameters();
    void signalState(bool);        // connection status, true for connected
    void signalChangeRecipe(QString, int);
    void signalStatus(uint16_t);
    void signalLogs(QString);
    void signalWriteRetry();    //garly_20220808, add for retry write when receive write fail from plc
#ifdef SYNC_TIME_FROM_PLC
    void signalReadCurrentTimeCmd();	//garly_20220224, add for auto update time from plc
#endif
#ifdef BARCODE_READER   //garly_20220308
    void signalActionBarcodeReader();
#endif
#ifdef PRE_DETECT  //garly_20221007
    void signalPreDetect();
#endif
    void signalSaveImage(QString); //garly_20240716, add for save image from plc trig
    void signalErrorLoadingParmSetting();   // Jimmy 20240409
#ifdef DEFECT_DETECTION
    void signalDefectDetection(int);
#endif

public slots:
    void Initialize();      // init. the connection paras.
    void Disconnect(bool need_reconnect);
    void BuildConnection();

    void SetProjectPath(const QString &proj_path, const QString &recipe_path);
    void SetCCDTrig() { ccd_trigger_ = 1;
                        PlcWriteCommand(_PLC_WRITE_CCD_TRIG_, addr_ccd_trigger_); }
    void SetWaitingPlcAck(bool b) { isWaiting_plc_ack = b; }
    void SetPlcStatus(uint16_t st) { st_plc_ = st; }
    void SetCurrentRecipeNum(uint16_t num) { CurrentRecipeNum = num; }

    int GetPLCFormat() const { return (int)format_PLC_; }
    void GetPLCStatus() { emit signalStatus(this->st_plc_); }

    bool LoadSettingfromFile();
    bool LoadSettingFromBackupFile();   // Jimmy 20240409
    bool LoadDefaultSettingfromINIFile(const std::string file_path, const std::string &section_name);
    void WriteSetting2File();
    void WriteResult(int32_t v1, int32_t v2, int32_t v3, int32_t result, PlcWriteMode wm);
    void WriteCurrentRecipeNum() { PlcWriteCommand(_PLC_WRITE_RECIPE_NUM_, addr_recipe_num_write_); }
    void WriteCurrentErrorCodeNum(QString erroeCode_num);  //garly_20220617
    void WriteCommandRetry();           //garly_20220808, add for retry write when receive write fail from plc
#ifdef DEFECT_DETECTION
    void WriteDefectDetectionResult(QString result1, int defectNum1, QString result2, int defectNum2);
#endif
#ifdef SYNC_TIME_FROM_PLC
    void setCurrentSystemTime(uint16_t yy, uint16_t mm, uint16_t dd, uint16_t h, uint16_t m, uint16_t s);
    void setSyncTimeMode(bool status, uint8_t index);
#endif
#ifdef BARCODE_READER   //garly_20220309
    void WriteBarcodeResult(QString num, int result);
#endif
};

#endif // PLCCONNECTOR_H
