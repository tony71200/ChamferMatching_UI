#include "plc_communication.h"

PLC_Communication::PLC_Communication()
{

}

bool PLC_Communication::plc_connection(QString &ip, int &port)
{
    // [OPTIMIZED] Add robust error handling, logging, and return value
    int timeout = 1000;
    try {
        if (!m_socket) {
            qWarning() << "[PLC] Socket is null!";
            is_connected = false;
            return false;
        }
        m_socket->connectToHost(ip, port);
        if (m_socket->waitForConnected(timeout)) {
            qInfo() << "[PLC] Connected to" << ip << ":" << port;
            is_connected = true;
            return true;
        } else {
            qWarning() << "[PLC] Connection failed:" << m_socket->errorString();
            is_connected = false;
            return false;
        }
    } catch (const std::exception& e) {
        qCritical() << "[PLC] Exception in plc_connection:" << e.what();
        is_connected = false;
        return false;
    } catch (...) {
        qCritical() << "[PLC] Unknown exception in plc_connection.";
        is_connected = false;
        return false;
    }
}

void PLC_Communication::plc_disconnect()
{
    // [OPTIMIZED] Add null check and log
    if (m_socket) {
        m_socket->disconnectFromHost();
        is_connected = false;
        qInfo() << "[PLC] Disconnected from host.";
    } else {
        qWarning() << "[PLC] Attempted disconnect with null socket.";
    }
}

bool PLC_Communication::wait_for_PLC_ready(unsigned int timeout = 3000)
{
    // [OPTIMIZED] Use QElapsedTimer for accurate timeout, avoid busy-wait
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < (int)timeout) {
        if (is_ready()) return true;
        QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
    }
    qWarning() << "[PLC] wait_for_PLC_ready timeout after" << timeout << "ms.";
    return false;
}

void PLC_Communication::CMD_read_from_PLC(uint16_t mode, QString block_addr)
{
    QString str_send;
    QString block;
    QString addr;
    QString addr_tmp;

    unsigned char read_cmd[21] = {
        0x50, 0x00,                         //header
        0x00, 0xff, 0xff, 0x03, 0x00,       //fixed
        0x0C, 0x00,                         //length
        0x0A, 0x00,                         //return time
        0x01, 0x04,                         //read command
        0x00, 0x00,
        0xD8, 0x13, 0x00,                   //address, example: 005080(10) => 0x0013D8(16)
        0xA8,                               //memory block id : D
        0x01, 0x00                          //number of bytes which you wanna read
    };

    try{
        // [OPTIMIZED] Validate input and clear previous state
        if (block_addr.isEmpty()) {
            qWarning() << "[PLC] CMD_read_from_PLC: block_addr is empty!";
            return;
        }
        // Separate "Memory Block (eng. char.)" and "Address (digit)"
        for(int i=0; i<block_addr.length(); i++){
            if((block_addr[i] >= 'a' && block_addr[i] <= 'z') ||
               (block_addr[i] >= 'A' && block_addr[i] <= 'Z'))
                block.append(block_addr[i]);
            if(block_addr[i] >= '0' && block_addr[i] <= '9')
                addr_tmp.append(block_addr[i]);
        }
        addr.append(QString("00%1").arg(addr_tmp));
        m_plc_status = mode;

        if(m_format_PLC == _PLC_Format_::_PLC_Format_ASCII_){
            str_send = "500000FF03FF000018000A04010000";
            str_send.append(block);
            str_send.append("*");
            str_send.append(addr);
            str_send.append("0001");
        }
        else if(m_format_PLC == _PLC_Format_::_PLC_Format_Binary_){
            QString addr_conv = convert_dec_to_hex(addr.toInt(), 6);
            unsigned char temp[3] = {0};
            for(int i=0; i<addr_conv.size();i+=2)
                temp[i/2] = (addr_conv.mid(i,2).toInt(nullptr, 16)) & 0xff;
            read_cmd[15] = temp[0];
            read_cmd[16] = temp[1];
            read_cmd[17] = temp[2];
            // Memory block
            if (block == "D")
                read_cmd[18] = 0xA8;
            else if (block == "W")
                read_cmd[18] = 0xB4;
            else if (block == "B")
                read_cmd[18] = 0xA0;
            else if (block == "X")
                read_cmd[18] = 0xA0;
            else if (block == "Y")
                read_cmd[18] = 0x9C;
            else if (block == "M")
                read_cmd[18] = 0x9D;
            else if (block == "L")
                read_cmd[18] = 0x92;
            else if (block == "F")
                read_cmd[18] = 0x93;
            else if (block == "V")
                read_cmd[18] = 0x94;
        }

        if(is_connected && wait_for_PLC_ready(_WAIT_TIME_OUT_)){
            wait_lock();
            qint64 write_result = 0;
            if(m_format_PLC == _PLC_Format_::_PLC_Format_ASCII_){
                write_result = m_socket->write(str_send.toLatin1());
            }
            else if(m_format_PLC == _PLC_Format_::_PLC_Format_Binary_){
                QByteArray wr((char*)read_cmd, sizeof(read_cmd));
                write_result = m_socket->write(wr);
            }
            bool bflush = m_socket->waitForBytesWritten();
            if(write_result == -1 || !bflush){
                qWarning() << "[PLC] CMD_read_from_PLC: write failed.";
            }
        } else {
            qWarning() << "[PLC] CMD_read_from_PLC: Not connected or PLC not ready.";
        }
    }
    catch (const std::exception& e){
        qCritical() << "[PLC] Exception in CMD_read_from_PLC:" << e.what();
    } catch (...) {
        qCritical() << "[PLC] Unknown exception in CMD_read_from_PLC.";
    }
}

void PLC_Communication::CMD_write_to_PLC(uint16_t mode, QString block_addr)
{
    QString str_send;
    QString block;
    QString addr;
    QString addr_tmp;

    unsigned char write_cmd[45] = {
        0x50, 0x00,                     //header    [0][1]
        0x00, 0xff, 0xff, 0x03, 0x00,   //fixed     [2]~[6]
        0x0e, 0x00,                     //lenth     [7][8]
        0x10, 0x00,                     //return time   [9][10]
        0x01, 0x14,                     //write command  [11][12]
        0x00, 0x00,
        0xD8, 0x13, 0x00,               //address, example : 0x005080   [15][16][17]
        0xA8,                           //memory block id : D           [18]
        0x01, 0x00,                     //number of bytes which you wanna write (word) [19][20]
        0x00, 0x00,                     //data 1 : low word     [21][22]
        0x00, 0x00,                     //data 2 : high word    [23][24]
        0x00, 0x00,                     //data 3 : low word     [25][26]
        0x00, 0x00,                     //data 4 : high word    [27][28]
        0x00, 0x00,                     //data 5 : low word     [29][30]
        0x00, 0x00,                     //data 6 : high word    [31][32]
        0x00, 0x00,                     //data 7 : low word     [33][34]
        0x00, 0x00,                     //data 8 : high word    [35][36]

        0x00, 0x00,                     //data 9 : low word     [37][38]
        0x00, 0x00,                     //data 10 : high word    [39][40]
        0x00, 0x00,                     //data 11 : low word     [41][42]
        0x00, 0x00                      //data 12 : high word    [43][44]
    };

    try{
        for(int i=0; i<block_addr.length(); i++){
            if((block_addr[i] >= 'a' && block_addr[i] <= 'z') ||
               (block_addr[i] >= 'A' && block_addr[i] <= 'Z'))
                block.append(block_addr[i]);

            if(block_addr[i] >= '0' && block_addr[i] <= '9')
                addr_tmp.append(block_addr[i]);
        }
        addr.append(QString("00%1").arg(addr_tmp));

        //char write_format_D5190[47];
        //char *str_D5190Head = "5000 00 FF 03FF 00 001C 0010 1401 0000 D* 005190 0001";
        //char *str_D5100Head = "5000 00 FF 03FF 00 0028 000A 1401 0000 D* 005100 0004";
        m_plc_status = mode;

        if(m_format_PLC == _PLC_Format_::_PLC_Format_ASCII_){
            str_send = "500000FF03FF00";
            if(mode==_PLC_WRITE_CCD_TRIG_){
                str_send.append("001C");
                str_send.append("001014010000");
                str_send.append(block);
                str_send.append("*");
                str_send.append(addr);
                str_send.append("0001");

                char write_data_temp[5];
                snprintf(write_data_temp, sizeof(write_data_temp), "%04x", m_ccd_trigger);
                QString data(write_data_temp);
                str_send.append(data);
            }
            else if (mode == _PLC_WRITE_PLC_TRIG_){
                str_send.append("001C");    //request data length
                str_send.append("001014010000");
                str_send.append(block);     //M or D
                str_send.append("*");
                str_send.append(addr);
                str_send.append("0001");


                char write_data_temp[5];
                snprintf(write_data_temp,sizeof(write_data_temp),"%04x", m_plc_trigger);
                QString data(write_data_temp);
                str_send.append(data);
            }
            else if (mode == _PLC_WRITE_CCD_RESULT_){
                str_send.append("0038");    //request data length
                str_send.append("000A14010000");
                str_send.append(block);     //M or D
                str_send.append("*");
                str_send.append(addr);
                str_send.append("0008");

                char write_data_temp[33];
                snprintf(write_data_temp,sizeof(write_data_temp),"%04x%04x%04x%04x%04x%04x%04x%04x",
                         (m_wrt_result_data->value1 & 0x0000ffff),
                         ((m_wrt_result_data->value1 & 0xffff0000)>>16),

                         (m_wrt_result_data->value2 & 0x0000ffff),
                         ((m_wrt_result_data->value2 & 0xffff0000)>>16),

                         (m_wrt_result_data->value3 & 0x0000ffff),
                         ((m_wrt_result_data->value3 & 0xffff0000)>>16),

                         (m_wrt_result_data->result & 0x0000ffff),
                         ((m_wrt_result_data->result & 0xffff0000)>>16)
                            );
                QString data(write_data_temp);
                str_send.append(data);
            }
        }
        else if (m_format_PLC == _PLC_Format_Binary_){
            //Address
            QString addr_conv = convert_dec_to_hex(addr.toInt(), 6);
            unsigned char temp[3];
            for(int i=0; i<addr_conv.size(); i+=2){
                temp[i/2] = (addr_conv.mid(i,2).toInt(nullptr,16)) & 0xff;
            }
            write_cmd[15] = temp[0];
            write_cmd[16] = temp[1];
            write_cmd[17] = temp[2];

            // Memory Block
            if (block == "D")
                write_cmd[18] = 0xA8;
            else if (block == "W")
                write_cmd[18] = 0xB4;
            else if (block == "B")
                write_cmd[18] = 0xA0;
            else if (block == "X")
                write_cmd[18] = 0x9C;
            else if (block == "Y")
                write_cmd[18] = 0x9D;
            else if (block == "M")
                write_cmd[18] = 0x90;
            else if (block == "L")
                write_cmd[18] = 0x92;
            else if (block == "F")
                write_cmd[18] = 0x93;
            else if (block == "V")
                write_cmd[18] = 0x94;

            if (mode == _PLC_WRITE_CCD_TRIG_){
                //request data length
                write_cmd[7] = 0x0E;    //low byte
                write_cmd[8] = 0x00;    //high byte
                //return time
                write_cmd[9] = 0x10;    //low byte
                write_cmd[10] = 0x00;    //high byte
                //write lenth
                write_cmd[19] = 0x01;    //low byte
                write_cmd[20] = 0x00;    //high byte
                //data 1
                write_cmd[21] = (unsigned char)(m_ccd_trigger & 0xff);    //low byte
                write_cmd[22] = (unsigned char)((m_ccd_trigger & 0xff00)>>8);    //high byte
            }
            else if (mode == _PLC_WRITE_PLC_TRIG_){
                //request data length
                write_cmd[7] = 0x0E;    //low byte
                write_cmd[8] = 0x00;    //high byte
                //return time
                write_cmd[9] = 0x10;    //low byte
                write_cmd[10] = 0x00;    //high byte
                //write lenth
                write_cmd[19] = 0x01;    //low byte
                write_cmd[20] = 0x00;    //high byte
                //data 1
                write_cmd[21] = (unsigned char)(m_plc_trigger & 0xff);    //low byte
                write_cmd[22] = (unsigned char)((m_plc_trigger & 0xff00)>>8);    //high byte
            }
            else if(mode == _PLC_WRITE_CCD_RESULT_){
                //request data length
                write_cmd[7] = 0x1C;    //low byte
                write_cmd[8] = 0x00;    //high byte
                //return time
                write_cmd[9] = 0x0A;    //low byte
                write_cmd[10] = 0x00;    //high byte
                //write lenth
                write_cmd[19] = 0x08;    //low byte
                write_cmd[20] = 0x00;    //high byte
                //data
                for (uint8_t i = 0; i < 4; i++)
                {
                    int32_t value = 0;
                    if (i == 0)
                        value = m_wrt_result_data->result;
                    else if (i == 1)
                        value = m_wrt_result_data->value1;
                    else if (i == 2)
                        value = m_wrt_result_data->value2;
                    else if (i == 3)
                        value = m_wrt_result_data->value3;

                    //data : low word
                    unsigned char data_lo = (unsigned char)((value & 0x0000ffff) & 0x00ff);
                    unsigned char data_hi = (unsigned char)(((value & 0x0000ffff) & 0xff00)>>8);
                    write_cmd[21+(i*4)] = data_lo;    //low byte
                    write_cmd[22+(i*4)] = data_hi;    //high byte
                    //data : high word
                    data_lo = (unsigned char)(((value & 0xffff0000) >> 16) & 0x00ff);
                    data_hi = (unsigned char)((((value & 0xffff0000) >> 16) & 0xff00) >> 8);
                    write_cmd[23+(i*4)] = data_lo;    //low byte
                    write_cmd[24+(i*4)] = data_hi;    //high byte
                }
            }
            else if (mode == _PLC_WRITE_RECIPE_NUM_){
                //request data length
                write_cmd[7] = 0x0E;    //low byte
                write_cmd[8] = 0x00;    //high byte
                //return time
                write_cmd[9] = 0x10;    //low byte
                write_cmd[10] = 0x00;    //high byte
                //write lenth
                write_cmd[19] = 0x01;    //low byte
                write_cmd[20] = 0x00;    //high byte
                //data 1
                write_cmd[21] = (unsigned char)(m_current_recipe_num & 0xff);    //low byte
                write_cmd[22] = (unsigned char)((m_current_recipe_num & 0xff00)>>8);    //high byte
            }
        }
        if (is_connected && wait_for_PLC_ready(_WAIT_TIME_OUT_)){
            wait_lock();
            qint64 write_result = 0;
            if(m_format_PLC == _PLC_Format_ASCII_){
                write_result = m_socket->write(str_send.toLatin1());
            }
            else if (m_format_PLC == _PLC_Format_Binary_){
                uint8_t wr_len = 23;
                if (mode == _PLC_WRITE_CCD_RESULT_) wr_len = 37;
                QByteArray wr((char*)write_cmd, wr_len);
                write_result = m_socket->write(wr);

            }
            bool bFlush = m_socket->waitForBytesWritten();
            if (write_result != -1 && bFlush == 1){
                if(write_result == 0){
                    // Send a warning message
                }
            }
            else{
                //try to retry send CMD
            }
        }
    }
    catch (std::exception e){
        //throw the exception e.
    }
}

void PLC_Communication::process_data()
{
    // Read and process the data from PLC in buffer
    QByteArray buf;
    buf = m_socket->readAll();

    if(!buf.isEmpty())
        wait_unclock();
    else{
        wait_unclock();
        return;
    }

    uint16_t total_len = 0;
    uint16_t rcv_cmd = 0;
    uint16_t result_read_len = 0;
    uint16_t result_read_encode = 0;
    uint16_t result_read_data = 0;
    bool header_correct = false;

    if ((m_plc_status >= _PLC_READ_PLC_TRIG_) && (m_plc_status <= _PLC_READ_D5060_)){
        QByteArray tmp;
        if(m_format_PLC == _PLC_Format_ASCII_){
            for(int i=0; i<4; i++){
                tmp[i] = buf.at(i+14);
            }
            result_read_len = tmp.toUInt(Q_NULLPTR, 16);

            for(int i=0; i<4; i++) tmp[i] = buf.at(i+18);
            result_read_encode = tmp.toUInt(Q_NULLPTR, 16);

            switch(m_plc_status){
            case _PLC_READ_PLC_TRIG_:
            case _PLC_READ_PLC_CMD_:
            case _PLC_READ_RECIPE_NUM_:
            case _PLC_READ_CURRENT_TIME_:
            case _PLC_READ_D5060_:
                total_len = (WORD_BYTE_UNIT * READ_REQ_LENGTH_WORD)
                        + ENDCODE_BYTE_UNIT;
                break;
            default:
                break;
            }
            header_correct = (buf.at(0) == 'D') ? true : false;
        }
        else if(m_format_PLC == _PLC_Format_Binary_){
            result_read_len = (buf.at(8) << 8) + buf.at(7);
            result_read_encode = (buf.at(10) << 8) + buf.at(9);

            total_len = (WORD_BYTE_UNIT_BINARY * READ_REQ_LENGTH_WORD)
                    + ENDCODE_BYTE_UNIT_BINARY;
            header_correct = (buf.at(0) == 0xD0) ? true: false;
        }
        if ((header_correct) && (result_read_len == total_len) &&
                (ENDCODE_VALUE == result_read_encode)){
            //Prepare reading data
            if(m_format_PLC == _PLC_Format_ASCII_){
                for(int i=0; i<4; i++) tmp[i] = buf.at(i+22);
                result_read_data = tmp.toUInt(Q_NULLPTR, 16);
            }
            else if(m_format_PLC == _PLC_Format_Binary_)
                result_read_data = (buf.at(12) << 8) + buf.at(11);

            // Process depends on PLC status
            switch(m_plc_status){
            case _PLC_IDLE_:
                break;
            case _PLC_READ_PLC_CMD_:
                rcv_cmd = result_read_data;

                switch(rcv_cmd){
                case 0: //Calibration
                    m_plc_trigger = 0;
                    CMD_write_to_PLC(_PLC_WRITE_PLC_TRIG_, m_addr_plc_trigger);

                    emit signal_action_calibration();
                    break;
                case 1: //alignment
                    m_plc_trigger = 0;
                    CMD_write_to_PLC(_PLC_WRITE_PLC_TRIG_, m_addr_plc_trigger);

                    emit signal_action_alignment();
                    break;
                case 2: // set ref. point
                    m_plc_trigger = 0;
                    CMD_write_to_PLC(_PLC_WRITE_PLC_TRIG_, m_addr_plc_trigger);

                    emit signal_action_set_ref(2);
                    break;
                }
                break;
            case _PLC_READ_PLC_TRIG_:   //51200
                m_plc_trigger = result_read_data;
                if(m_plc_trigger == 1){
                    CMD_read_from_PLC(_PLC_READ_PLC_CMD_, m_addr_cmd);
                }
                else{
                    m_plc_status = _PLC_IDLE_;
                    m_is_waiting_plc_ack = false;
                }
                break;
            case _PLC_READ_RECIPE_NUM_: // read recipe number
            {
                uint16_t rcv_recipe_number = result_read_data;
                m_plc_trigger = 0;
                CMD_write_to_PLC(_PLC_WRITE_PLC_TRIG_, m_addr_plc_trigger);
                emit signal_change_recipe(QString("%1").arg(rcv_recipe_number), 1);
                break;
            }
            case _PLC_READ_D5060_:
            {
                m_is_waiting_plc_ack = false;
                m_plc_status = _PLC_IDLE_;
                break;
            }
            default:
            {
                m_plc_status = _PLC_IDLE_;
                m_is_waiting_plc_ack = false;
                break;
            }
        }
        }
        else{
            m_plc_status = _PLC_IDLE_;
            m_is_waiting_plc_ack = false;
        }
    }
    else if((m_plc_status >= _PLC_WRITE_CCD_TRIG_)){   // Writing command process
        QByteArray tmp;
        if(m_format_PLC == _PLC_Format_ASCII_){
            for(int i=0; i<4; i++) tmp[i] = buf.at(i+14);

            uint16_t result_write_len = tmp.toUInt(Q_NULLPTR, 16);
            for(int i=0; i<4; i++) tmp[i] = buf.at(i+18);

            uint16_t result_write_encode = tmp.toUInt(Q_NULLPTR, 16);
            if((buf.at(0) == 'D') && (result_write_len == 4) &&
                    (result_write_encode == 0)){
                m_is_waiting_plc_ack = false;
            }
            else{
                m_is_waiting_plc_ack =  false;
            }
        }
        else if(m_format_PLC == _PLC_Format_Binary_){
            uint16_t result_write_len = (buf[8] << 8) + buf[7];
            uint16_t result_write_encode = (buf[10] << 8) + buf[9];
            total_len = (WORD_BYTE_UNIT_BINARY * READ_REQ_LENGTH_WORD) +
                    ENDCODE_BYTE_UNIT_BINARY;

            header_correct = (buf.at(0) == 0xD0) ? true: false;
            if((header_correct) && (result_write_len == total_len) &&
                    (ENDCODE_VALUE == result_write_encode)){
                m_is_waiting_plc_ack = false;
            }
            else{ //
                m_is_waiting_plc_ack = false;

                time_retry_write_cmd++;
                if(time_retry_write_cmd > 3)
                    time_retry_write_cmd = 0;
                else
                    emit signal_write_retry();
            }
        }
    }
}

void PLC_Communication::update_state(QAbstractSocket::SocketState st)
{
    if(st == QAbstractSocket::UnconnectedState){
        is_connected = false;
        emit signal_state(is_connected);
    }
    if(st==QAbstractSocket::ConnectedState){
        is_connected = true;
        emit signal_state(is_connected);
    }
}

void PLC_Communication::write_result(int32_t arg1, int32_t arg2, int32_t arg3, int32_t result)
{
    m_wrt_result_data->value1 = arg1;
    m_wrt_result_data->value2 = arg2;
    m_wrt_result_data->value3 = arg3;
    m_wrt_result_data->result = result;

    CMD_write_to_PLC(_PLC_WRITE_CCD_RESULT_, m_addr_result);
}

void PLC_Communication::run()
{
    update_state(m_socket->state());
    if(is_connected){
        emit signal_read_plc_trig_cmd();
    }
    else if(!is_connected){
        sleep(2000);
        plc_connection(m_ip_address, m_port);
    }
}

QString PLC_Communication::convert_dec_to_hex(int dec, int bit)
{
    // sample: 12.4 -> 0C 00        7000.6 -> 58 1B 00
    QString str = QString::number(dec, 16).toUpper();

    int len = bit - str.length();
    for(int i=0; i<len; i++){
        str.push_front("0");
    }

    if(bit==4){
        return str.right(2) + str.left(2);
    } else if(bit==6){
        return str.right(2) + str.mid(2,2) + str.left(2);
    } else {
        return "";
    }
}

void PLC_Communication::set_ip_port(QString &szIp, int iPort)
{
    this->m_ip_address = szIp;
     this->m_port = iPort;
}

void PLC_Communication::initialize(QString &szIp, int iPort)
{
    set_ip_port(szIp, iPort);
    // Loading default setting parameters
    m_settings = new LoadingSettings(m_settings_path);
    m_settings->get_plc_addr();
    m_addr_plc_trigger = m_settings->m_addr_plc_trigger;
    m_addr_result = m_settings->m_addr_result;

//    m_settings->m_addr_result = "D1102254";
//    m_settings->m_addr_plc_trigger = "D8895642";
//    m_settings->set_plc_addr();

    // Building a TCP connection
    build_connection();
    delete m_settings;
}

void PLC_Communication::build_connection()
{
    m_socket = new QTcpSocket();
    m_timer = new QTimer();
    m_timer->start(_TIME_OUT_);
//#ifdef SYNC_TIME_FROM_PLC
//    timer_autoSyncTime = new QTimer();
//    timer_autoSyncTime.start(autoSyncTime_timeout);
//#endif

    qDebug() << "plc ip address: " << m_ip_address << endl;
    qDebug() << "plc port: " << m_port << endl;

    plc_connection(m_ip_address, m_port);
    if(!is_connected){
        //throw a message: connect failed
        //Retry connecting???
    }
    //QObject::connect(m_socket, &QTcpSocket::connected, this, [this](){});
    QObject::connect(m_socket, &QTcpSocket::readyRead, this, &PLC_Communication::process_data);
//    QObject::connect(m_socket, &PLC_Communication::signal_read_plc_trig_cmd, this, &PLC_Communication::read_PLC_trig);
    QObject::connect(m_timer, &QTimer::timeout, this, &PLC_Communication::run);

    m_plc_status = _PLC_IDLE_;
}
