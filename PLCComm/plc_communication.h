#ifndef PLC_COMMUNICATION_H
#define PLC_COMMUNICATION_H

#include <QTcpServer>
#include <QTcpSocket>
#include <QDebug>
#include <QCoreApplication>
#include <QMutex>
#include <QTimer>
#include <QThread>

#include <ctime>

#include "globalparms.h"
#include "Functions/loadingsettings.h"

using namespace std;

//Definition
#define PACKET_SIZE 1024
#define READ_REQ_LENGTH_WORD        1
#define READ_REQ_6_WORD             6
#define WORD_BYTE_UNIT              4
#define ENDCODE_BYTE_UNIT           4
#define ENDCODE_VALUE               0
#define WRT_CORRECT_RETURN_LENGTH   4
#define WRT_RETRY_TIMES             10

#define ENDCODE_BYTE_UNIT_BINARY    2
#define WORD_BYTE_UNIT_BINARY       2

enum _PLC_Status_
{
    _PLC_IDLE_ = 0,
    _PLC_BUSY_,

    _PLC_READ_PLC_TRIG_,
    _PLC_READ_PLC_CMD_,
    _PLC_READ_RECIPE_NUM_,
    _PLC_READ_CURRENT_TIME_,
    _PLC_READ_D5060_,

    _PLC_WRITE_CCD_TRIG_,
    _PLC_WRITE_CCD_RESULT_, //8
    _PLC_WRITE_PLC_TRIG_,   //9
    _PLC_WRITE_RECIPE_NUM_,
    _PLC_WRITE_BARCODE_RESULT_,	//garly_20220309
    _PLC_WRITE_ERROR_CODE_   //garly_20220617
};
typedef struct
{
    int32_t value1;
    int32_t value2;
    int32_t value3;
    int32_t result;
}WRT_RESULT_DATA;

class PLC_Communication : public QObject
{
    Q_OBJECT
public:
    PLC_Communication();
private:    //variable
    const QString m_settings_path = "./Config/settings.ini";
    QTcpSocket *m_socket;
    QTimer *m_timer;

    LoadingSettings *m_settings;

    const int _WAIT_TIME_OUT_ = 3000;
    const int _TIME_OUT_ = 50;

    uint16_t m_plc_status;
    uint8_t m_format_PLC = _PLC_Format_::_PLC_Format_Binary_;

    QString m_addr_plc_trigger;
    QString m_addr_ccd_trigger;
    QString m_addr_cmd;
    QString m_addr_recipe_num_read;
    QString m_addr_recipe_num_write;
    QString m_addr_result;
    QString m_addr_current_time;
    QString m_addr_error_code_write;

    uint16_t m_plc_trigger = 0;
    uint16_t m_ccd_trigger = 0;
    uint16_t m_current_recipe_num = 0;
    uint16_t m_current_write_cmd_mode = _PLC_IDLE_;

    QMutex m_lock;
    bool m_is_waiting_plc_ack;
    int time_retry_write_cmd = 0;

    WRT_RESULT_DATA * m_wrt_result_data = new WRT_RESULT_DATA();

    QString m_ip_address;
    int m_port;

private:    //functions
    bool wait_for_PLC_ready(unsigned int timeout);
    bool is_ready() const {return !m_is_waiting_plc_ack;}
    bool wait_lock() {m_lock.lock(); m_is_waiting_plc_ack = true;}
    bool wait_unclock() {m_is_waiting_plc_ack = false; m_lock.unlock();}
    void read_PLC_trig();
    void process_PLC_data(QByteArray);

    QString convert_dec_to_hex(int dec, int bit);
    void sleep(int ms){QThread::currentThread()->msleep(ms);}

    void set_ip_port(QString &ip, int m_port);
signals:
    void signal_receive_plc_data(QByteArray);
    void signal_action_calibration();
    void signal_action_alignment();
    void signal_action_set_ref(int);
    void signal_read_plc_trig_cmd();
    void signal_reset_all_parameters();
    void signal_state(bool);
    void signal_change_recipe(QString, int);
    void signal_status(uint16_t);
    void signal_write_retry();
public:


    bool is_connected = false;

public slots:
    void initialize(QString &ip, int m_port);
    void build_connection();

    bool plc_connection(QString &ip, int &m_port);
    void plc_disconnect();

    void CMD_read_from_PLC(uint16_t mode, QString block_addr);
    void CMD_write_to_PLC(uint16_t mode, QString block_addr);

    void process_data();
    void run();
    void update_state(QAbstractSocket::SocketState st);
    void write_result(int32_t arg1, int32_t arg2, int32_t arg3, int32_t result);

};

#endif // PLC_COMMUNICATION_H
