#include "plcconnector.h"

PlcConnector::PlcConnector(QObject *parent) : QObject(parent)
{
    // Initialize
    proj_path_ = "./";
    st_plc_ = _PLC_IDLE_;

    ip_ = "0.0.0.0";
    port_ = 0;
    isConnected = false;
    isReconnected = false;  //garly_20220122
    isWaiting_plc_ack = false;

    db_skip_msg_read = 5;

    qRegisterMetaType<int32_t>("int32_t");
    qRegisterMetaType<uint16_t>("uint16_t");
    qRegisterMetaType<std::string>("std::string");
}

void PlcConnector::SetProjectPath(const QString &proj_path, const QString &recipe_path)
{
    if(dm::plc_log){
        qDebug() << "[PLC] Setting Project Path" ;
    }
    this->proj_path_ = proj_path;
    this->recipe_path_ = recipe_path;

    lw.SetDirPath(proj_path.toStdString() + "/Log");
    lw.SetShowDate(true);
    lw.SetLogName("plclog");
}

void PlcConnector::WriteResult(int32_t v1, int32_t v2, int32_t v3, int32_t result, PlcWriteMode wm)
{
    if(dm::plc_log){
        PLCSystemLog(QString("[PLC Action] PLC Write Result: %1, %2, %3, %4")
                     .arg(v1, v2, v3, result));
    }

    int32_t val_x = 0;
    int32_t val_y = 0;
    int32_t val_deg = 0;

    int32_t val_x1 = 0;
    int32_t val_x2 = 0;

    switch (wm) {
    case PlcWriteMode::kMmDegMov:
    case PlcWriteMode::kMmDegRot:
    case PlcWriteMode::kPulMovYT:
    case PlcWriteMode::kPulRotYT:
        // Command(val_X, val_Y, val_Deg)
        val_x = plc_sign_x_;
        val_y = plc_sign_y_;
        val_deg = plc_sign_theta_ * v3;
        if (is_plc_swap_xy_) {
            val_x *= v2;    // val_x <- y val. in system alg.
            val_y *= v1;    // val_y <- x val. in system alg.
        } else {
            val_x *= v1;
            val_y *= v2;
        }

        wrtResultData->value1 = val_x;
        wrtResultData->value2 = val_y;
        wrtResultData->value3 = val_deg;
        wrtResultData->result = result;
        break;

    case PlcWriteMode::kPulMovUVW:
        // Command(val_X1, val_X2, val_Y)
        val_x1 = plc_sign_x_;
        val_x2 = plc_sign_x_;
        val_y  = plc_sign_y_;
        if (is_plc_swap_xy_) {
            // x1 -> y
            // y -> x1; -y -> x2
            val_x1 *= v3;
            val_x2 *= -v3;
            val_y  *= v1;
        } else {
            val_x1 *= v1;
            val_x2 *= v2;
            val_y  *= v3;
        }

        wrtResultData->value1 = val_x1;
        wrtResultData->value2 = val_x2;
        wrtResultData->value3 = val_y;
        wrtResultData->result = result;
        break;

    case PlcWriteMode::kPulRotUVW:
        // Command(val_X1, val_X2, val_Y)
        val_x1 = plc_sign_theta_ * v1;
        val_x2 = plc_sign_theta_ * v2;
        val_y  = plc_sign_theta_ * v3;

        wrtResultData->value1 = val_x1;
        wrtResultData->value2 = val_x2;
        wrtResultData->value3 = val_y;
        wrtResultData->result = result;
        break;

    case PlcWriteMode::kOthers:
    default:
        wrtResultData->value1 = v1;
        wrtResultData->value2 = v2;
        wrtResultData->value3 = v3;
        wrtResultData->result = result;
        break;
    }

    if(dm::plc_log){
        qDebug() << "======= PLC Recieved Data ========";
        qDebug() << "> v1: " << wrtResultData->value1;
        qDebug() << "> v2: " << wrtResultData->value2;
        qDebug() << "> v3: " << wrtResultData->value3;
        qDebug() << "> rs: " << wrtResultData->result;
        qDebug() << "----------------------------------\n";
    }

    PlcWriteCommand(_PLC_WRITE_CCD_RESULT_, addr_result_);
}

void PlcConnector::WriteCurrentErrorCodeNum(QString erroeCode_num) {
    CurrentErrorCode = erroeCode_num;
    PlcWriteCommand(_PLC_WRITE_ERROR_CODE_, addr_error_code_write_);
}

#ifdef DEFECT_DETECTION
void PlcConnector::WriteDefectDetectionResult(QString result1, int defectNum1, QString result2, int defectNum2) {
    ddtResultData->result_l = result1; //OK or NG
    ddtResultData->defect_num_l = defectNum1;

    ddtResultData->result_r = result2; //OK or NG
    ddtResultData->defect_num_r = defectNum2;
    PlcWriteCommand(_PLC_WRITE_DEFECT_DETECTION_, addr_defect_detection_write_);
}
#endif

#ifdef BARCODE_READER	//garly_20220309
void PlcConnector::WriteBarcodeResult(QString serial_num, int result)
{
    PLCSystemLog("Write Barcode Result !");

    //default
    wrtResultData->value1 = 0x0020; //" " (space)
    wrtResultData->value2 = 0x0020;
    wrtResultData->value3 = 0x0020;
    wrtResultData->value4 = 0x0020;
    wrtResultData->value5 = 0x0020;

    if (serial_num != "No_Result")
    {
        QByteArray bytes = serial_num.toUtf8();
        if(dm::plc_log){
            for (int i = 0; i<bytes.size();i++)
            {
                qDebug() << bytes.at(i) << "(" << QString("%1").arg(bytes.at(i), 0, 16) << "),";
            }
        }

        for (int i = 0; i < serial_num.size(); i+=2)
        {
            int value = (bytes.at(i) << 8) + bytes.at(i+1);
            //qDebug() << "value = " << value << QString("%1").arg(value, 0, 16);
            if (i == 0)
                wrtResultData->value1 = value;
            else if (i == 2)
                wrtResultData->value2 = value;
            else if (i == 4)
                wrtResultData->value3 = value;
            else if (i == 6)
                wrtResultData->value4 = value;
            else if (i == 8)
                wrtResultData->value5 = value;
        }
    }

    wrtResultData->result = result;
    PlcWriteCommand(_PLC_WRITE_BARCODE_RESULT_, addr_barcode_result_);
}
#endif

void PlcConnector::Connect()
{

    int timeout = 1000;

    socket_->connectToHost(ip_, port_);
    if (!socket_->waitForConnected(timeout)){
        if(dm::plc_log){
            qDebug() << "[PLC Fail] Failed connection to PLC!" ;
        }
        PLCSystemLog("[Fail] Failed connection to PLC!");
        isConnected = false;
        isReconnected = true;
        st_plc_ = _PLC_IDLE_;
        emit signalState(isConnected);
        return;
    }
    // PLC connect successfully
    PLCSystemLog("[OK] Connect PLC success!");
    if(dm::plc_log){
        qDebug() << "[PLC OK] Connect PLC success!";
    }

    isReconnected = false;  //garly_20220122
    isConnected = true;
    st_plc_ = _PLC_IDLE_;

    // Reset the PLC trigger, and test on communication
    WaitUnlock();
    plc_trigger_ = 0;

    emit signalLogs("[Action] PLC已成功連線.");
    emit signalState(isConnected);
}

void PlcConnector::Run()
{
    // Here, keep listening and acting.
    UpdateState(socket_->state());

    if (isConnected){

        emit signalReadPlcTrigCmd();

    } else if (!isConnected && isReconnected){

        if (dm::plc_log) {
            PLCSystemLog("[Action] Try reconnecting...");
        }
        Sleep(2000);
        isReconnected = false;  //garly_20220122
        Connect();              //garly_20220122
    #ifdef SYNC_TIME_FROM_PLC
        if (isConnected)	//garly_20220224, for auto update time form plc
            emit signalReadCurrentTimeCmd();
    #endif
    }
}

#ifdef SYNC_TIME_FROM_PLC
void PlcConnector::RunSyncCurrentTime()   //call every 1 min
{
    if ((isConnected) && (enableSyncTime) && (syncTimeUnit > 0))
    {
        if (autoUpdateCounter >= (syncTimeUnit - 1))
        {
            emit signalReadCurrentTimeCmd();
            autoUpdateCounter = 0;
        }
        else
            autoUpdateCounter++;
    }
}
#endif

void PlcConnector::UpdateState(QAbstractSocket::SocketState st)
{
    //qDebug() << ">> SocketState changed: " << st;
    if (st == QAbstractSocket::UnconnectedState){
        isConnected = false;
        emit signalState(isConnected);
    }
    if (st == QAbstractSocket::ConnectedState){
        isConnected = true;
        emit signalState(isConnected);
    }
}

void PlcConnector::Disconnect(bool need_reconnect = true)
{
    // Go to disconnect process
    /// I am willing to disconnect
    if (socket_ && isConnected && !isReconnected){
        if(dm::plc_log){
            qDebug("[PLC] Disconnceting from PLC...");
        }
        emit signalLogs("[Error p001] Connection to PLC failed.");

        socket_->disconnectFromHost();
        if (socket_->state() != QAbstractSocket::UnconnectedState)
            socket_->waitForDisconnected();
        Sleep(200);

        st_plc_ = _PLC_IDLE_;
        isConnected = false;
        isReconnected = need_reconnect;   //garly_20220122

        PLCSystemLog(QString("[Action] Disconnected from PLC! "
                             "Reconnection need: %1").arg(need_reconnect));
    }
}

void PlcConnector::ProcessData()
{
    // Read and process the data from PLC in buffer
    QByteArray buf;
    buf = socket_->readAll();
    // if readAll() is not empty => no error and have data
    if (!buf.isEmpty())
        WaitUnlock();
    else{
        WaitUnlock();
        if(dm::plc_log){
            qDebug() << "[DB PLC] None in buffer!!! ";
        }
        return;
    }

    if(dm::plc_log){
        if (isBufLogCanUpdate){
            qDebug() << "[DB PLC] PLC ACK: " << buf;
            log_buf_ = buf;
        }
    }

    uint16_t total_len = 0;
    uint16_t rcv_command = 0;
    uint16_t result_read_len = 0;
    uint16_t result_read_encode = 0;
    uint16_t result_read_data = 0;
    bool header_correct = false;

    if ((st_plc_ >= _PLC_READ_PLC_TRIG_) && (st_plc_ <= _PLC_READ_D5060_)){ // Process Reading Command
        QByteArray tmp;
        if (format_PLC_ == _PLC_Format_ASCII_){
            for (int i=0; i<4; i++)
                tmp[i] = buf.at(i+14);
            result_read_len = tmp.toUInt(Q_NULLPTR, 16);

            for (int i=0; i<4; i++)
                tmp[i] = buf.at(i+18);
            result_read_encode = tmp.toUInt(Q_NULLPTR, 16);

            switch (st_plc_) {
            case _PLC_READ_PLC_TRIG_:
            case _PLC_READ_PLC_CMD_:
            case _PLC_READ_RECIPE_NUM_:
            case _PLC_READ_CURRENT_TIME_://garly_20220224, for auto update time form plc
            case _PLC_READ_D5060_:
                total_len = (WORD_BYTE_UNIT * READ_REQ_LENGTH_WORD) + ENDCODE_BYTE_UNIT;
                break;
            default:
                break;
            }
            header_correct = (buf.at(0) == 'D') ? true: false;

        }else if (format_PLC_ == _PLC_Format_Binary_){
            result_read_len = (buf.at(8) << 8) + buf.at(7);
            result_read_encode = (buf.at(10) << 8) + buf.at(9);
    #ifdef SYNC_TIME_FROM_PLC
	    //garly_20220224, for auto update time form plc
            if (st_plc_ == _PLC_READ_CURRENT_TIME_)
                total_len = (WORD_BYTE_UNIT_BINARY * READ_REQ_6_WORD) + ENDCODE_BYTE_UNIT_BINARY;
            else
	#endif
            if (st_plc_ == _PLC_READ_PANEL_ID_) //garly_20240930, add for panel id
                total_len = (WORD_BYTE_UNIT_BINARY * READ_REQ_6_WORD) + ENDCODE_BYTE_UNIT_BINARY;
            else
                total_len = (WORD_BYTE_UNIT_BINARY * READ_REQ_LENGTH_WORD) + ENDCODE_BYTE_UNIT_BINARY;

            header_correct = (buf.at(0) == 0xD0) ? true: false;
        }

        if ((header_correct) && (result_read_len == total_len) && (ENDCODE_VALUE == result_read_encode)){
            // Prepare reading data
            if (format_PLC_ == _PLC_Format_ASCII_){
                for (int i=0; i<4; i++)
                    tmp[i] = buf.at(i+22);

                result_read_data = tmp.toUInt(Q_NULLPTR, 16);
            }else if (format_PLC_ == _PLC_Format_Binary_)
                result_read_data = (buf.at(12) << 8) + buf.at(11);

            // Process depends on PLC Status
            switch (st_plc_) {
            case _PLC_IDLE_:
                PLCSystemLog("[Status] IDLE.");
                break;
            case _PLC_READ_PLC_CMD_:
                if(dm::plc_log){
                    qDebug() << "[PLC_CMD ACK] Command = " << result_read_data;
                    PLCSystemLog(QString("[PLC_CMD ACK] Command = %1").arg(result_read_data));
                }
                rcv_command = result_read_data;

                switch (rcv_command) {
                case 0: // Calibration
                    if(dm::plc_log){
                        qDebug() << "================= {Calibration} =================";
                    }
                    plc_trigger_ = 0;
                    PlcWriteCommand(_PLC_WRITE_PLC_TRIG_, addr_plc_trigger_);
                    PLCSystemLog("[Write] Calib. trigger OK.");
                    if(dm::plc_log){
                        qDebug() << "[Write] Calib. trigger OK.";
                    }

                    emit signalActionCalibration();
                    break;
                case 2: //set ref. point
                    if(dm::plc_log){
                        qDebug() << "================= {Set Ref. Point} =================";
                    }
                    plc_trigger_ = 0;
                    PlcWriteCommand(_PLC_WRITE_PLC_TRIG_, addr_plc_trigger_);
                    PLCSystemLog("[Write] Set Ref. trigger OK.");
                    if(dm::plc_log){
                        qDebug() << "[Write] Set Ref. trigger OK.";
                    }

                    emit signalActionSetRef(2);
                    break;
                case 1: //alignment
                    if(dm::plc_log){
                        qDebug() << "================= {Alignment} =================";
                    }
                    plc_trigger_ = 0;
                    PlcWriteCommand(_PLC_WRITE_PLC_TRIG_, addr_plc_trigger_);
                    PLCSystemLog("[Write] Align. trigger OK.");
                    if(dm::plc_log){
                        qDebug() << "[Write] Align. trigger OK.";
                    }

                    emit signalActionAlignment();
                    break;
#ifdef PRE_DETECT  //garly_20221007
                case 5: //init detect
                    if(dm::plc_log){
                        qDebug() << "================= {Pre-detect} =================";
                    }
                    plc_trigger_ = 0;
                    PlcWriteCommand(_PLC_WRITE_PLC_TRIG_, addr_plc_trigger_);
                    PLCSystemLog("[Write] Pre-detect trigger OK.");
                    if(dm::plc_log) qDebug() << "<<=== Pre detect";
                    emit signalPreDetect();
                    break;
#endif
#ifdef BARCODE_READER   //garly_20220308
                case 6: //barcode reader
                    if(dm::plc_log){
                        qDebug() << "================= {Barcode reader} =================";
                    }
                    plc_trigger_ = 0;
                    PlcWriteCommand(_PLC_WRITE_PLC_TRIG_, addr_plc_trigger_);
                    PLCSystemLog("Write PLC trig finish");

                    emit signalActionBarcodeReader();
                    break;
#endif  //end of BARCODE_READER
                case 7: //reset all parameters of alignment system
                    if(dm::plc_log){
                        qDebug() << "================= {Reset} =================";
                    }
                    plc_trigger_ = 0;
                    PlcWriteCommand(_PLC_WRITE_PLC_TRIG_, addr_plc_trigger_);
                    PLCSystemLog("[Write] Reset CCD trigger OK.");
                    if(dm::plc_log){
                        qDebug() << "[Write] Reset CCD trigger OK.";
                    }

                    emit signalLogs("[Action] 進行重啟.");
                    emit signalResetAllParameters();
                    break;
                case 9: //receipe switch
                    PLCSystemLog("[Write] Receipe switch trigger OK.");
                    if(dm::plc_log){
                        qDebug() << "================= {Receipe Switch} =================";
                        qDebug() << "Receive receipe switch request ... ";
                    }

                    PlcReadCommand(_PLC_READ_RECIPE_NUM_, addr_recipe_num_read_);
                    break;
                case 10: //save image   //garly_20240716, add for save image from plc trig
                    if(dm::plc_log){
                        qDebug() << "================= {Save-Img} =================";
                    }

                    ReadPanelId();
                    break;
	#ifdef DEFECT_DETECTION
                case 11:    //defect detection - all    //garly_20241024
                case 12:    //defect detection - left ccd
                case 13:    //defect detection - right ccd
                    if(dm::plc_log){
                        qDebug() << "================= {Defect Detection} =================";
                    }
                    plc_trigger_ = 0;
                    PlcWriteCommand(_PLC_WRITE_PLC_TRIG_, addr_plc_trigger_);
                    PLCSystemLog("[Write] Defect Detection trigger OK.");
                    if(dm::plc_log) qDebug() << "<<=== Defect Detection : cmd(" << rcv_command <<")";
                    emit signalDefectDetection((rcv_command-11));
                    break;
	#endif
                default:
                    if(dm::plc_log){
                        qDebug() << "================= {Idle} =================";
                    }
                    isWaiting_plc_ack = false;
                    st_plc_ = _PLC_IDLE_;
                    break;
                }
                break;
            case _PLC_READ_PLC_TRIG_:   // 51200
                if(dm::log) {
                    if(dm::plc_log){
                        db_skip_msg_read--;
                        if(db_skip_msg_read  <= 0){
                            qDebug() << "[PLC ACK] Trigger = " << result_read_data;
                            db_skip_msg_read = 5;
                        }
                    }
                    PLCSystemLog(QString("[PLC ACK] Trigger = %1").arg(result_read_data));
                }

                plc_trigger_ = result_read_data;

                if (plc_trigger_ == 1)
                {
                    if(dm::plc_log){
                        qDebug() << "[PLC] Read PLC command ... ";
                    }
                    PlcReadCommand(_PLC_READ_PLC_CMD_, addr_cmd_);
                    if(dm::plc_log){
                        PLCSystemLog("[OK] Read PLC command finish!");
                    }
                }
                else
                {
                    st_plc_ = _PLC_IDLE_;
                    isWaiting_plc_ack = false;
                }
                break;
            case _PLC_READ_RECIPE_NUM_:     //read recipe number
            {
                PLCSystemLog(QString("[Read ACK] Recipe number = %1").arg(result_read_data));
                if(dm::plc_log){
                    qDebug() << QString("[Read ACK] Recipe number = %1").arg(result_read_data);
                }
                uint16_t rcvRecipeNumber = result_read_data;

                plc_trigger_ = 0;
                PlcWriteCommand(_PLC_WRITE_PLC_TRIG_, addr_plc_trigger_);
                if(dm::plc_log){
                    PLCSystemLog("[Write] Read recipe trigger OK.");
                }

                emit signalChangeRecipe(QString("%1").arg(rcvRecipeNumber), 1);
                break;
            }

        #ifdef SYNC_TIME_FROM_PLC
            case _PLC_READ_CURRENT_TIME_:	//garly_20220224, for auto update time form plc
            {
                uint16_t data1 = (buf.at(12) << 8) + buf.at(11);
                uint16_t data2 = (buf.at(14) << 8) + buf.at(13);
                uint16_t data3 = (buf.at(16) << 8) + buf.at(15);
                uint16_t data4 = (buf.at(18) << 8) + buf.at(17);
                uint16_t data5 = (buf.at(20) << 8) + buf.at(19);
                uint16_t data6 = (buf.at(22) << 8) + buf.at(21);

                qDebug() << QString("[PLC time ACK] Current time => %1/%2/%3 %4:%5:%6")
                            .arg(data1).arg(data2).arg(data3).arg(data4).arg(data5).arg(data6) << endl;
                if ( ((data1 > 0) && (data1 < 3000)) &&
                     ((data2 > 0) && (data2 <= 12)) &&
                     ((data3 > 0) && (data3 <= 31)) )
                    setCurrentSystemTime(data1, data2, data3, data4, data5, data6);
                isWaiting_plc_ack = false;
                st_plc_ = _PLC_IDLE_;
                break;
            }
        #endif
            case _PLC_READ_PANEL_ID_:   //garly_20240930, add for panel id
            {
                QByteArray subArray = buf.mid(11, 12);
                QString panelId = QString::fromLatin1(subArray);
                if(dm::plc_log) qDebug() << "Panel ID : " << panelId << endl;

                plc_trigger_ = 0;
                PlcWriteCommand(_PLC_WRITE_PLC_TRIG_, addr_plc_trigger_);
                PLCSystemLog("[Write] Save-Img trigger OK.");
                if(dm::plc_log) qDebug() << "<<=== Save Image";
                emit signalSaveImage(panelId);
                break;
            }
            case _PLC_READ_D5060_:
            {
                //qDebug() << "[D5060 ACK] Pos 1/2 = " << result_read_data;
                isWaiting_plc_ack = false;
                st_plc_ = _PLC_IDLE_;
                break;
            }
            default:
            {
                if(dm::plc_log){
                    qDebug() << "[PLC Warning] PLC status is out of range!";
                }
                PLCSystemLog("[Warning] PLC status is out of range!");
                st_plc_ = _PLC_IDLE_;
                isWaiting_plc_ack = false;
                break;
            }
            }
        }else {
            if (buf.at(0) != 'D')
                PLCSystemLog("[Rcv Error] Header is incorrect !!");
            if (result_read_len != total_len)
                PLCSystemLog(QString("[Rcv Error] Total length is incorrect !! %1, %2").arg(result_read_len).arg(total_len));
            if (ENDCODE_VALUE != result_read_encode)
                PLCSystemLog(QString("[Rcv Error] Encode value is incorrect !! %1, %2").arg(result_read_encode).arg(ENDCODE_VALUE));

            PLCSystemLog(QString("[Rcv Error] Data has incorrect need to resend!! plcStatus = %1").arg(st_plc_));

            st_plc_ = _PLC_IDLE_;
            isWaiting_plc_ack = false;
        }
    }else if ((st_plc_ >= _PLC_WRITE_CCD_TRIG_) && (st_plc_ <= _PLC_WRITE_BARCODE_RESULT_)){  // Process Writting Command
        QByteArray tmp;
        //qDebug() << "Write command ack : " << plcStatus << endl;
        if(dm::plc_log){
            PLCSystemLog(QString("[Write] Write command ack : %1").arg(st_plc_));
        }
        if (format_PLC_ == _PLC_Format_ASCII_)
        {
            for (int i = 0; i < 4 ; i++)
            {
                tmp[i] = buf.at(i+14);
            }

            uint16_t resultWriteLenth = tmp.toUInt(Q_NULLPTR, 16);

            for (int i = 0; i < 4 ; i++)
            {
                tmp[i] = buf.at(i+18);
            }

            uint16_t resultWriteEncode = tmp.toUInt(Q_NULLPTR, 16);

            if ((buf.at(0) == 'D') && (resultWriteLenth == 4) && (resultWriteEncode == 0))
            {
                //qDebug() << "Write success !!";
                isWaiting_plc_ack = false;
            }
            else
            {
                //qDebug() << "Write fail, retry again !!";
                PLCSystemLog("[Fail] Write fail, retry again!");
                isWaiting_plc_ack = false;
            }
        }
        else if (format_PLC_ == _PLC_Format_Binary_)
        {
            uint16_t resultWriteLenth = (buf[8] << 8) + buf[7];
            uint16_t resultWriteEncode = (buf[10] << 8) + buf[9];
            total_len = (WORD_BYTE_UNIT_BINARY * READ_REQ_LENGTH_WORD) + ENDCODE_BYTE_UNIT_BINARY;

            header_correct = (buf.at(0) == 0xD0)? true: false;

            if ((header_correct) && (resultWriteLenth == total_len) && (ENDCODE_VALUE == resultWriteEncode))
            {
                //qDebug() << "Write success !!";
                isWaiting_plc_ack = false;

                //garly_20220808, add for retry write when receive write fail from plc
                CurrentWriteCommandMode = _PLC_IDLE_;
                time_retryWriteCommand = 0;
                //garly_20220808, add for retry write when receive write fail from plc
            }
            else
            {
                //qDebug() << "Write fail, retry again !!";
                PLCSystemLog("[Fail] Write fail, retry again! Current cmd (" + QString("%1").arg(CurrentWriteCommandMode) + ")");
                isWaiting_plc_ack = false;

                //garly_20220808, add for retry write when receive write fail from plc
                time_retryWriteCommand++;
                if (time_retryWriteCommand > 3)
                {
                    PLCSystemLog("[Fail] Write fail, retry over 3 time! Current cmd (" + QString("%1").arg(CurrentWriteCommandMode) + ")");
                    time_retryWriteCommand = 0;
                }
                else
                    emit signalWriteRetry();
                //garly_20220808, add for retry write when receive write fail from plc
            }
        }
    }
}

void PlcConnector::Initialize()
{
    if(dm::plc_log){
        qDebug() << "[PLC]PLC Initilize";
    }
    if (!LoadSettingfromFile())
    {
        if(dm::plc_log){
            qDebug() << "[PLC]Failed loading params from file!";
            qDebug() << "[PLC]Go create a new file...";
        }
        ip_  = "192.168.1.200";  //default
        port_  = 5031;   //5031:shap alignment; 5032:marker alignment
        addr_plc_trigger_ = "D15000";
        addr_ccd_trigger_ = "D15002";
        addr_cmd_ = "D15004";
        addr_result_ = "D15108";
        addr_recipe_num_read_ = "D15034";
        addr_recipe_num_write_ = "D15134";
        addr_current_time_ = "D15036";
        addr_error_code_write_ = "D15136";  //garly_20220617

#ifdef BARCODE_READER	//garly_20220309
        addr_barcode_result_ = "D15138";
#endif
#ifdef DEFECT_DETECTION
        addr_defect_detection_write_ = "D15855";
#endif
        addr_panelId_          = "D15870"; //garly_20240930, add for panel id
        // For PLC direction. Jimmy, 20220503.
        is_plc_swap_xy_ = false;
        plc_sign_x_ = 1;
        plc_sign_y_ = 1;
        plc_sign_theta_ = 1;

        WriteSetting2File();
    }
    if(dm::plc_log){
        qDebug() << "[DB PLC] ========= Init =========";
        qDebug() << "IP: " << ip_;
        qDebug() << "Port: " << port_;
        qDebug() << "PLC Trigger: " << addr_plc_trigger_;
        qDebug() << "CCD Trigger: " << addr_ccd_trigger_;
        qDebug() << "Command: " << addr_cmd_;
        qDebug() << "Result: " << addr_result_;
        qDebug() << "Current time: " << addr_current_time_;
        qDebug() << "Panel ID: " << addr_panelId_;//garly_20240930, add for panel id
    #ifdef BARCODE_READER	//garly_20220309
        qDebug() << "Barcode: " << addr_barcode_result_;
    #endif
    #ifdef DEFECT_DETECTION
        qDebug() << "Defect Det. ID: " << addr_defect_detection_write_;
    #endif
        qDebug() << "=================================";
    }

    BuildConnection();

    emit signalState(isConnected);
}

bool PlcConnector::WaitForPlcReady(unsigned int timeout = 3000)
{
    // Wait for PLC Ready. if Ready return true; return false until timeout(ms).
    clock_t endwait;
    endwait = clock() + timeout * CLOCKS_PER_SEC;
    while (clock() < endwait){
        if (IsReady()){

            return true;
        }
        QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
    }
    return false;
}

void PlcConnector::ReadPlcTrig()
{
    // Require the command/ack from PLC. Call on time.
    if(dm::log) {
        if(dm::plc_log){
            db_skip_msg_read--;
            if (db_skip_msg_read  <= 0){
                qDebug() << "Read PLC trigger ... ";
                db_skip_msg_read = 5;
            }
        }
    }
    if (IsReady()){
        PlcReadCommand(_PLC_READ_PLC_TRIG_, addr_plc_trigger_);
        if(dm::plc_log){
            PLCSystemLog("[OK] Trigger reading OK.");
        }
    }
}
#ifdef SYNC_TIME_FROM_PLC
void PlcConnector::ReadCurrentTime()//garly_20220224, for auto update time form plc
{
    if (IsReady()){
        PlcReadCommand(_PLC_READ_CURRENT_TIME_, addr_current_time_);
        PLCSystemLog("Read Current time finish");
    }
}
#endif

void PlcConnector::ReadPanelId()//garly_20240930, add for panel id
{
    PlcReadCommand(_PLC_READ_PANEL_ID_, addr_panelId_);
    PLCSystemLog("Read Panel ID finish");
}

void PlcConnector::PlcReadCommand(uint16_t mode, QString block_addr)
{
    // Require the command/ack from PLC
    QString str_send;
    QString block;
    QString addr;
    QString addr_tmp;

    unsigned char read_cmd[21] = {
        0x50, 0x00,                     //header [0][1]
        0x00, 0xff, 0xff, 0x03, 0x00,   //fixed  [2][3][4][5][6]
        0x0C, 0x00,                     //lenth  [7][8]
        0x0A, 0x00,                     //return time   [9][10]
        0x01, 0x04,                     //read command  [11][12]
        0x00, 0x00,                     //              [13][14]
        0xD8, 0x13, 0x00,               //address, example : 005080(10) => 0x0013D8(16) [15][16][17]
        0xA8,                           //memory block id : D   [18]
        0x01, 0x00                      //number of bytes which you wanna read  [19][20]
    };

    try
    {
        // Separate "Memory Block (eng. char.)" and "Address (digit)"
        for (int i = 0; i< block_addr.length(); i++){
            if ((block_addr[i] >= 'a' && block_addr[i] <= 'z') || (block_addr[i] >= 'A' && block_addr[i] <= 'Z'))
                block.append(block_addr[i]);

            if (block_addr[i] >= '0' && block_addr[i] <= '9')
                addr_tmp.append(block_addr[i]);
        }
        addr.append(QString("00%1").arg(addr_tmp));
        st_plc_ = mode;

        if (format_PLC_ == _PLC_Format_ASCII_){
            //#define READ_D5080 "5000 00FF 03FF 00 0018 000A 0401 0000 D* 005080 0001"
            //#define READ_D5000 "5000 00FF 03FF 00 0018 000A 0401 0000 D* 005000 0001"
            str_send = "500000FF03FF000018000A04010000";
            str_send.append(block);
            str_send.append("*");
            str_send.append(addr);
            str_send.append("0001");
        }
        else if (format_PLC_ == _PLC_Format_Binary_){
            // Address
            QString addr_conv = ConvertDec2Hex(addr.toInt(), 6);
            unsigned char temp[3];
            for (int i = 0; i < addr_conv.size(); i+=2)
                temp[i/2] = (addr_conv.mid(i, 2).toInt(nullptr,16)) & 0xff;

            read_cmd[15] = temp[0];
            read_cmd[16] = temp[1];
            read_cmd[17] = temp[2];

            // Memory Block
            if (block == "D")
               read_cmd[18] = 0xA8;
            else if (block == "W")
                read_cmd[18] = 0xB4;
            else if (block == "B")
                read_cmd[18] = 0xA0;
            else if (block == "X")
               read_cmd[18] = 0x9C;
            else if (block == "Y")
                read_cmd[18] = 0x9D;
            else if (block == "M")
                read_cmd[18] = 0x90;
            else if (block == "L")
               read_cmd[18] = 0x92;
            else if (block == "F")
                read_cmd[18] = 0x93;
            else if (block == "V")
                read_cmd[18] = 0x94;
	#ifdef SYNC_TIME_FROM_PLC
            if (st_plc_ == _PLC_READ_CURRENT_TIME_)
                read_cmd[19] = 0x06;
	#endif
            if (st_plc_ == _PLC_READ_PANEL_ID_)
                read_cmd[19] = 0x06;
        }

        if (isConnected && WaitForPlcReady(kWaitTimeout)){
            WaitLock();
            qint64 writeResult = 0;

            if (format_PLC_ == _PLC_Format_ASCII_){
                writeResult = socket_->write(str_send.toLatin1());
            }
            else if (format_PLC_ == _PLC_Format_Binary_){
                QByteArray wr((char*)read_cmd, sizeof(read_cmd));
                if(dm::plc_log){
                    if (log_req_ != wr){
                        isBufLogCanUpdate = true;
                        qDebug() << "[DB PLC] Require (R): " << wr;
                        log_req_ = wr;
                    }else
                        isBufLogCanUpdate = false;
                }
                writeResult = socket_->write(wr);// will return the bytes num actually written
            }

            bool BoolFlush = socket_->waitForBytesWritten(); // flush()
            if (writeResult != -1 && BoolFlush){// Success
                if (writeResult == 0){
                    PLCSystemLog("[Warning] Write result = 0! @PlcReadCommand().");
                }
                if(dm::plc_log){
                    PLCSystemLog("[OK] Send data success! @PlcReadCommand().");
                }
            }else {
                PLCSystemLog("[Fail] Send data failed! @PlcReadCommand().");
            }
        }
        else {
            if(dm::plc_log){
                std::cout << "[PLC Read CMD] Waiting PLC ready is timeout!" << std::endl;
            }
            PLCSystemLog("[PLC Read CMD] Waiting PLC ready is timeout!");
            emit signalLogs("[Error p002] 等待超時，PLC無回應.");
        }
    }
    catch (std::exception e) {
        std::cout << "[PLC Read CMD] PLC read cmd error: " << e.what() << std::endl;
        PLCSystemLog(QString("[PLC Read CMD] PLC read cmd error: %1").arg(e.what()));
    }
}

void PlcConnector::PlcWriteCommand(uint16_t mode, QString block_addr)
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

    try
    {
        for (int i = 0; i< block_addr.length(); i++){
            if ((block_addr[i] >= 'a' && block_addr[i] <= 'z') || (block_addr[i] >= 'A' && block_addr[i] <= 'Z'))
                block.append(block_addr[i]);

            if (block_addr[i] >= '0' && block_addr[i] <= '9')
                addr_tmp.append(block_addr[i]);
        }
        addr.append(QString("00%1").arg(addr_tmp));

        //char write_format_D5190[47];
        //char *str_D5190Head = "5000 00 FF 03FF 00 001C 0010 1401 0000 D* 005190 0001";
        //char *str_D5100Head = "5000 00 FF 03FF 00 0028 000A 1401 0000 D* 005100 0004";
        st_plc_ = mode;

        if (format_PLC_ == _PLC_Format_ASCII_){

            str_send = "500000FF03FF00";
            if (mode == _PLC_WRITE_CCD_TRIG_){
                if(dm::plc_log){
                    PLCSystemLog(QString("[Write] CCD trig: %1").arg(ccd_trigger_));
                }
                str_send.append("001C");    //request data length
                str_send.append("001014010000");
                str_send.append(block);     //M or D
                str_send.append("*");
                str_send.append(addr);
                str_send.append("0001");


                char write_data_temp[5];
                snprintf(write_data_temp,sizeof(write_data_temp),"%04x", ccd_trigger_);
                QString data(write_data_temp);
                str_send.append(data);

            }
            else if (mode == _PLC_WRITE_PLC_TRIG_){
                if(dm::plc_log){
                    PLCSystemLog(QString("[Write] PLC trig: %1").arg(plc_trigger_));
                }
                str_send.append("001C");    //request data length
                str_send.append("001014010000");
                str_send.append(block);     //M or D
                str_send.append("*");
                str_send.append(addr);
                str_send.append("0001");


                char write_data_temp[5];
                snprintf(write_data_temp,sizeof(write_data_temp),"%04x", plc_trigger_);
                QString data(write_data_temp);
                str_send.append(data);

            }
            else if (mode == _PLC_WRITE_CCD_RESULT_){
                if(dm::plc_log){
                    PLCSystemLog("[Write] CCD result...");
                }
                str_send.append("0038");    //request data length
                str_send.append("000A14010000");
                str_send.append(block);     //M or D
                str_send.append("*");
                str_send.append(addr);
                str_send.append("0008");

                char write_data_temp[33];
                snprintf(write_data_temp,sizeof(write_data_temp),"%04x%04x%04x%04x%04x%04x%04x%04x",
                         (wrtResultData->value1 & 0x0000ffff),
                         ((wrtResultData->value1 & 0xffff0000)>>16),

                         (wrtResultData->value2 & 0x0000ffff),
                         ((wrtResultData->value2 & 0xffff0000)>>16),

                         (wrtResultData->value3 & 0x0000ffff),
                         ((wrtResultData->value3 & 0xffff0000)>>16),

                         (wrtResultData->result & 0x0000ffff),
                         ((wrtResultData->result & 0xffff0000)>>16)
                            );
                QString data(write_data_temp);
                str_send.append(data);
            }
        }
        else if (format_PLC_ == _PLC_Format_Binary_){
            // Address
            QString addr_conv = ConvertDec2Hex(addr.toInt(), 6);
            unsigned char temp[3];
            for (int i = 0; i < addr_conv.size(); i+=2)
            {
                temp[i/2] = (addr_conv.mid(i, 2).toInt(nullptr,16)) & 0xff;
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
                if(dm::plc_log){
                    qDebug() << "Write CCD trigger = " << ccd_trigger_;
                    PLCSystemLog(QString("[Write] CCD trig: %1").arg(ccd_trigger_));
                }

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
                write_cmd[21] = (unsigned char)(ccd_trigger_ & 0xff);    //low byte
                write_cmd[22] = (unsigned char)((ccd_trigger_ & 0xff00)>>8);    //high byte
            }
            else if (mode == _PLC_WRITE_PLC_TRIG_){
                if(dm::plc_log){
                    qDebug() << "PLC write plc_trig : plc trigger = " << plc_trigger_;
                    PLCSystemLog(QString("[Write] PLC trig: %1").arg(plc_trigger_));
                }

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
                write_cmd[21] = (unsigned char)(plc_trigger_ & 0xff);    //low byte
                write_cmd[22] = (unsigned char)((plc_trigger_ & 0xff00)>>8);    //high byte
            }
            else if (mode == _PLC_WRITE_CCD_RESULT_){
                if(dm::plc_log){
                    qDebug() << "[Write] CCD result...";
                    PLCSystemLog("[Write] CCD result...");
                }

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
                        value = wrtResultData->result;
                    else if (i == 1)
                        value = wrtResultData->value1;
                    else if (i == 2)
                        value = wrtResultData->value2;
                    else if (i == 3)
                        value = wrtResultData->value3;

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
                if(dm::plc_log){
                    qDebug() << "[PLC Write] Recipe number: " << CurrentRecipeNum;
                    PLCSystemLog(QString("[Write] Recipe number: %1").arg(CurrentRecipeNum));
                }

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
                write_cmd[21] = (unsigned char)(CurrentRecipeNum & 0xff);    //low byte
                write_cmd[22] = (unsigned char)((CurrentRecipeNum & 0xff00)>>8);    //high byte
            }
            else if (mode == _PLC_WRITE_ERROR_CODE_){    //garly_20220617
                if(dm::plc_log){
                    qDebug() << "[PLC Write] Error_code number: " << CurrentErrorCode;
                    PLCSystemLog(QString("[PLC Write] ErrorCode number: %1").arg(CurrentErrorCode));
                }

                QByteArray error_ascii = CurrentErrorCode.toUtf8();
                QString tmp = CurrentErrorCode.mid(1, CurrentErrorCode.size());

                //request data length
                write_cmd[7] = 0x10;    //low byte
                write_cmd[8] = 0x00;    //high byte
                //return time
                write_cmd[9] = 0x10;    //low byte
                write_cmd[10] = 0x00;    //high byte
                //write lenth
                write_cmd[19] = 0x02;    //low byte
                write_cmd[20] = 0x00;    //high byte
                //data 1
                write_cmd[21] = (unsigned char)(int(error_ascii.at(0)) & 0xff);    //low byte
                write_cmd[22] = (unsigned char)((int(error_ascii.at(0)) & 0xff00)>>8);    //high byte
                //data 2
                write_cmd[23] = (unsigned char)(tmp.toUInt() & 0xff);    //low byte
                write_cmd[24] = (unsigned char)((tmp.toUInt() & 0xff00)>>8);    //high byte
            }
#ifdef DEFECT_DETECTION
            else if (mode == _PLC_WRITE_DEFECT_DETECTION_)
            {
                if(dm::plc_log){
                    qDebug() << QString("[PLC Write] Defect detection Info: left ccd [%1, %2], right ccd [%3, %4]")
                                .arg(ddtResultData->result_l).arg(ddtResultData->defect_num_l)
                                .arg(ddtResultData->result_r).arg(ddtResultData->defect_num_r);
                    PLCSystemLog(QString("[PLC Write] Defect detection Info: left ccd [%1, %2], right ccd [%3, %4]")
                                 .arg(ddtResultData->result_l).arg(ddtResultData->defect_num_l)
                                 .arg(ddtResultData->result_r).arg(ddtResultData->defect_num_r));
                }

                uint16_t tmp1 = (ddtResultData->result_l == "--")? 0 : ((ddtResultData->result_l == "OK")? 1:2);    //1: OK; 2: NG; 0:disable
                uint16_t tmp2 = (ddtResultData->result_r == "--")? 0 : ((ddtResultData->result_r == "OK")? 1:2);    //1: OK; 2: NG; 0:disable

                //request data length
                write_cmd[7] = 0x10;    //low byte
                write_cmd[8] = 0x00;    //high byte
                //return time
                write_cmd[9] = 0x10;    //low byte
                write_cmd[10] = 0x00;    //high byte
                //write lenth
                write_cmd[19] = 0x04;    //low byte
                write_cmd[20] = 0x00;    //high byte
                //data 1
                write_cmd[21] = (unsigned char)(tmp1 & 0xff);    //low byte
                write_cmd[22] = (unsigned char)((tmp1 & 0xff00)>>8);    //high byte
                //data 2
                write_cmd[23] = (unsigned char)(ddtResultData->defect_num_l & 0xff);    //low byte
                write_cmd[24] = (unsigned char)((ddtResultData->defect_num_l & 0xff00)>>8);    //high byte
                //data 3
                write_cmd[25] = (unsigned char)(tmp2 & 0xff);    //low byte
                write_cmd[26] = (unsigned char)((tmp2 & 0xff00)>>8);    //high byte
                //data 4
                write_cmd[27] = (unsigned char)(ddtResultData->defect_num_r & 0xff);    //low byte
                write_cmd[28] = (unsigned char)((ddtResultData->defect_num_r & 0xff00)>>8);    //high byte

                qDebug() << QString("L : (%1, %2), R : (%3, %4)").arg(ddtResultData->result_l)
                                                                 .arg(ddtResultData->defect_num_l)
                                                                 .arg(ddtResultData->result_r)
                                                                 .arg(ddtResultData->defect_num_r) << endl;
            }
#endif
#ifdef BARCODE_READER	//garly_20220309
            else if (mode == _PLC_WRITE_BARCODE_RESULT_){
                if(dm::plc_log){
                    qDebug() << "Write barcode to plc";
                }
                PLCSystemLog("Write barcode to plc");

                //request data length
                write_cmd[7] = 0x1C;    //low byte
                write_cmd[8] = 0x00;    //high byte
                //return time
                write_cmd[9] = 0x0A;    //low byte
                write_cmd[10] = 0x00;    //high byte
                //write lenth
                write_cmd[19] = 0x0C;    //low byte
                write_cmd[20] = 0x00;    //high byte
                //data
                for (uint8_t i = 0; i < 6 ; i++)
                {
                    int32_t value = 0;
                    if (i == 0)
                        value = wrtResultData->result;
                    else if (i == 1)
                        value = wrtResultData->value1;
                    else if (i == 2)
                        value = wrtResultData->value2;
                    else if (i == 3)
                        value = wrtResultData->value3;
                    else if (i == 4)
                        value = wrtResultData->value4;
                    else if (i == 5)
                        value = wrtResultData->value5;

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
#endif  //end of BARCODE_READER
        }

        if (isConnected && WaitForPlcReady(kWaitTimeout))
        {
            WaitLock();
            qint64 writeResult = 0;
            if (format_PLC_ == _PLC_Format_ASCII_)
            {
                if(dm::plc_log){
                    //qDebug() << "[DB PLC] Write to PLC (W): " << str_send.toLatin1();
                }
                writeResult = socket_->write(str_send.toLatin1());
            }
            else if (format_PLC_ == _PLC_Format_Binary_)
            {
                uint8_t wr_len = 23;
                if (mode == _PLC_WRITE_CCD_RESULT_)
                    wr_len = 37;
#ifdef BARCODE_READER	//garly_20220309
                else if (mode == _PLC_WRITE_BARCODE_RESULT_)
                    wr_len = 45;
#endif
                else if (mode == _PLC_WRITE_ERROR_CODE_)
                    wr_len = 25;
                else if (mode == _PLC_WRITE_DEFECT_DETECTION_)
                    wr_len = 29;
                else
                    wr_len = 23;
                QByteArray wr((char*)write_cmd, wr_len);
                if(dm::plc_log){
                    if (log_req_ != wr){
                        isBufLogCanUpdate = true;
                        qDebug() << "[DB PLC] Require (W): " << wr;
                        log_req_ = wr;
                    }else
                        isBufLogCanUpdate = false;
                }
                writeResult = socket_->write(wr);

                CurrentWriteCommandMode = mode; //garly_20220808, add for retry write when receive write fail from plc
            }

            bool BoolFlush = socket_->waitForBytesWritten(); // flush();
            if (writeResult != -1 && BoolFlush == 1)
            {
                if (writeResult == 0)
                {
                    PLCSystemLog("[Warning] Write result = 0! @PlcWriteCommand().");
                }
            }
            else
            {
                PLCSystemLog("[Fail] Send data failed! @PlcWriteCommand().");

                //garly_20220808, add for retry write when receive write fail from plc
                time_retryWriteCommand++;
                if (time_retryWriteCommand > 3)
                {
                    PLCSystemLog("[Fail] Send fail, retry over 3 time! Current cmd (" + QString("%1").arg(CurrentWriteCommandMode) + ")");
                    time_retryWriteCommand = 0;
                }
                else
                    emit signalWriteRetry();
                //garly_20220808, add for retry write when receive write fail from plc
            }
        }
        else {
            if(dm::plc_log){
                std::cout << "[PLC Write CMD] waiting PLC ready is timeout!" << std::endl;
            }
            PLCSystemLog("[PLC Write CMD] waiting PLC ready is timeout!");
            emit signalLogs("[Error p002] 等待超時，PLC無回應.");
        }
    }
    catch (std::exception e) {
        std::cout << "[PLC Write CMD] PLC write cmd error: " << e.what() << std::endl;
        PLCSystemLog(QString("[PLC Write CMD] PLC write cmd error: %1").arg(e.what()));
    }
}

QString PlcConnector::ConvertDec2Hex(int dec, int bit)
{
    // sample：12，4 -> 0C 00     7000，6 -> 58 1B 00
    QString str = QString::number(dec, 16).toUpper();

    int len = bit - str.length();
    for ( int i = 0; i < len; ++i )
    {
        str.push_front("0");
    }

    if ( bit == 4 ) {
        return str.right(2) + str.left(2);
    } else if ( bit == 6 ) {
        return str.right(2) + str.mid(2, 2) + str.left(2);
    } else {
        return "";
    }
}

void PlcConnector::BuildConnection()
{
    // This function will build a TCP connection to server.
    if(dm::plc_log){
        qDebug() << "[PLC] *** Connecting to PLC *** ";
        PLCSystemLog(" *** Connecting to PLC *** ");
    }

    socket_ = new QTcpSocket(); // new a socket
    timer_ = new QTimer();      // set a timer
    timer_->start(kTimeout_);
#ifdef SYNC_TIME_FROM_PLC
    timer_autoSyncTime_ = new QTimer();
    timer_autoSyncTime_->start(autoSyncTime_timeout);   // call every 1 min
#endif
    Connect();
    if (!isConnected){
        emit signalLogs("[Error p001] Connection to PLC failed.");
        isReconnected = true;   //garly_20220122
    }

    // QObject connections
    QObject::connect(socket_, &QTcpSocket::connected, this, [this](){ /*qDebug("Socket Connected!");*/ });
    QObject::connect(socket_, &QTcpSocket::disconnected, this, &PlcConnector::ReconnectAfterDisconnect);
    //QObject::connect(socket_, &QTcpSocket::stateChanged, this, &Connector::UpdateState);
    QObject::connect(socket_, &QTcpSocket::readyRead, this, &PlcConnector::ProcessData);

    QObject::connect(this, &PlcConnector::signalReadPlcTrigCmd, this, &PlcConnector::ReadPlcTrig);
    QObject::connect(timer_, &QTimer::timeout, this, &PlcConnector::Run);
#ifdef SYNC_TIME_FROM_PLC
    QObject::connect(this, &PlcConnector::signalReadCurrentTimeCmd, this, &PlcConnector::ReadCurrentTime);
    QObject::connect(timer_autoSyncTime_, &QTimer::timeout, this, &PlcConnector::RunSyncCurrentTime);
#endif

    st_plc_ = _PLC_IDLE_;
#ifdef SYNC_TIME_FROM_PLC
    if (isConnected)
        emit signalReadCurrentTimeCmd();
#endif
}

bool PlcConnector::LoadSettingfromFile()
{
    if (dm::plc_log) {
        qDebug("[PLC] Loading params file...");
    }
    // Load connection settings from paras file
    QString param_dir_path = proj_path_ + "Data/system_config/";
    QDir new_dir(param_dir_path);

    PLCSystemLog("[Action] Load PLC setting...");
    if (!new_dir.exists())
        new_dir.mkpath(param_dir_path);
    //move to /system_config. Jimmy 20220715.
    QString param_path = param_dir_path + "plc_setting.ini";

    if (!fio::exists(param_path.toStdString())){

        // if file isn't existed. Copy file from backup.
        QString backup_path = param_dir_path + "../../";
        QString backup_name = QString("%1/plc_setting_backup.ini").arg(backup_path);

        QFile check_file(backup_name);
        if (check_file.exists()){
            QFile::copy(backup_name, param_path);
        }
    }

    // Read the parameters from file
    if (fio::exists(param_path.toStdString())){

        // FileIO. 20220419. Jimmy. #fio11
        fio::FileIO fs(param_path.toStdString(), fio::FileioFormat::R);
        if (!fs.isEmpty()){
            std::string PLC_IP_std = "";
            std::string AddrPLCTrigger_std = "";
            std::string AddrCCDTrigger_std = "";
            std::string AddrCommand_std = "";
            std::string AddrResult_std = "";
            std::string AddrRecipeNum_r = "";
            std::string AddrRecipeNum_w = "";
            std::string AddrCurrentTime = "";
            std::string AddrErrorCode = ""; //garly_20220617
            std::string AddrPanelId = "";   //garly_20240930, add for panel id
            std::string AddrDefectDetection = "";
#ifdef BARCODE_READER   //garly_20220309
            std::string AddrBarcode = "";
#endif
            int PLC_PORT_std = 0;
            int plc_format_std = 0;
            int plc_swap_xy = 0;
            int plc_sign_x = 1;
            int plc_sign_y = 1;
            int plc_sign_theta = 1;

            std::string found_data = "";
            int found_num = 0;
            found_data = fs.Read("IP");             if(fs.isOK()) PLC_IP_std         = found_data;
            found_data = fs.Read("PLCTriggerAddr"); if(fs.isOK()) AddrPLCTrigger_std = found_data;
            found_data = fs.Read("CCDTriggerAddr"); if(fs.isOK()) AddrCCDTrigger_std = found_data;
            found_data = fs.Read("CommandAddr");    if(fs.isOK()) AddrCommand_std    = found_data;
            found_data = fs.Read("ResultAddr");     if(fs.isOK()) AddrResult_std     = found_data;
            found_data = fs.Read("RecipeNumRAddr"); if(fs.isOK()) AddrRecipeNum_r    = found_data;
            found_data = fs.Read("RecipeNumWAddr"); if(fs.isOK()) AddrRecipeNum_w    = found_data;
            found_data = fs.Read("CurrentTimeAddr");if(fs.isOK()) AddrCurrentTime    = found_data;
            found_data = fs.Read("ErrorCodeAddr");  if(fs.isOK()) AddrErrorCode      = found_data;  //garly_20220617
            found_data = fs.Read("PanelIdAddr");    if(fs.isOK()) AddrPanelId        = found_data;  //garly_20240930, add for panel id
#ifdef DEFECT_DETECTION
            found_data = fs.Read("DefectDetectionAddr");  if(fs.isOK()) AddrDefectDetection      = found_data;
#endif

            found_num = fs.ReadtoInt("Port");           if(fs.isOK()) PLC_PORT_std       = found_num;
            found_num = fs.ReadtoInt("PLCFormat");      if(fs.isOK()) plc_format_std     = found_num;
            found_num = fs.ReadtoInt("IsPLCSwapXY");    if(fs.isOK()) plc_swap_xy        = found_num;
            found_num = fs.ReadtoInt("PLCSignX");       if(fs.isOK()) plc_sign_x         = found_num;
            found_num = fs.ReadtoInt("PLCSignY");       if(fs.isOK()) plc_sign_y         = found_num;
            found_num = fs.ReadtoInt("PLCSignTheta");   if(fs.isOK()) plc_sign_theta     = found_num;

#ifdef BARCODE_READER   //garly_20220309
            found_data = fs.Read("BarcodeResultAddr"); if(fs.isOK()) AddrBarcode    = found_data;
#endif

            ip_                     = QString::fromStdString(PLC_IP_std);
            addr_plc_trigger_       = QString::fromStdString(AddrPLCTrigger_std);
            addr_ccd_trigger_       = QString::fromStdString(AddrCCDTrigger_std);
            addr_cmd_               = QString::fromStdString(AddrCommand_std);
            addr_result_            = QString::fromStdString(AddrResult_std);
            addr_recipe_num_read_   = QString::fromStdString(AddrRecipeNum_r);
            addr_recipe_num_write_  = QString::fromStdString(AddrRecipeNum_w);
            addr_current_time_      = QString::fromStdString(AddrCurrentTime);
            addr_error_code_write_  = QString::fromStdString(AddrErrorCode);    //garly_20220617
            addr_panelId_           = QString::fromStdString(AddrPanelId);    //garly_20240930, add for panel id
#ifdef DEFECT_DETECTION
            addr_defect_detection_write_ = QString::fromStdString(AddrDefectDetection);
#endif
#ifdef BARCODE_READER   //garly_20220309
            addr_barcode_result_    = QString::fromStdString(AddrBarcode);
#endif
            port_                   = PLC_PORT_std;
            format_PLC_             = (uint8_t)plc_format_std;
            is_plc_swap_xy_         = (bool)(plc_swap_xy);
            plc_sign_x_             = plc_sign_x;
            plc_sign_y_             = plc_sign_y;
            plc_sign_theta_         = plc_sign_theta;

            if (fs.isAllOK()) {
                PLCSystemLog("[OK] Load setting successfully!");
                fs.close();
            }
            else {
                fs.close();
                PLCSystemLog("[NG] There are some problems while reading setting...");
                PLCSystemLog("[Action] Reading backup file instead.");
                // if some terms are not existed, write again!
                if (LoadSettingFromBackupFile()) {
                    WriteSetting2File();
                } else {
                    emit signalErrorLoadingParmSetting();
                }
            }

            return true; // load setting success
        } else {
            PLCSystemLog("[Fail] Load setting failed: file empty!");
            fs.close();
            return false;
        }
    }else {
        PLCSystemLog("[Fail] Setting file isn't exist !");
        return false;
    }
    return false;
}

bool PlcConnector::LoadSettingFromBackupFile()
{
    if (dm::plc_log) {
        qDebug("[PLC] Loading backup params file...");
    }
    // Load connection settings from paras file
    QString param_dir_path = proj_path_ + "Data/";
    QDir new_dir(param_dir_path);

    PLCSystemLog("[Action] Load PLC backup setting...");
    if (!new_dir.exists())
        new_dir.mkpath(param_dir_path);
    //move to /system_config. Jimmy 20220715.
    QString param_path = param_dir_path + "plc_setting_backup.ini";

    if (!fio::exists(param_path.toStdString())){
        PLCSystemLog("[Fail] Cannot find PLC backup setting!");
        return false;
    }

    // Read the parameters from file
    if (fio::exists(param_path.toStdString())){

        fio::FileIO fs(param_path.toStdString(), fio::FileioFormat::R);
        if (!fs.isEmpty()){
            std::string PLC_IP_std = "";
            std::string AddrPLCTrigger_std = "";
            std::string AddrCCDTrigger_std = "";
            std::string AddrCommand_std = "";
            std::string AddrResult_std = "";
            std::string AddrRecipeNum_r = "";
            std::string AddrRecipeNum_w = "";
            std::string AddrCurrentTime = "";
            std::string AddrErrorCode = "";
            std::string AddrPanelId = "";   //garly_20240930, add for panel id
            std::string AddrDefectDetection = "";

            int PLC_PORT_std = 0;
            int plc_format_std = 0;
            int plc_swap_xy = 0;
            int plc_sign_x = 1;
            int plc_sign_y = 1;
            int plc_sign_theta = 1;

            std::string found_data = "";
            int found_num = 0;

            found_data = fs.Read("IP");             if(fs.isOK()) PLC_IP_std         = found_data;
            found_data = fs.Read("PLCTriggerAddr"); if(fs.isOK()) AddrPLCTrigger_std = found_data;
            found_data = fs.Read("CCDTriggerAddr"); if(fs.isOK()) AddrCCDTrigger_std = found_data;
            found_data = fs.Read("CommandAddr");    if(fs.isOK()) AddrCommand_std    = found_data;
            found_data = fs.Read("ResultAddr");     if(fs.isOK()) AddrResult_std     = found_data;
            found_data = fs.Read("RecipeNumRAddr"); if(fs.isOK()) AddrRecipeNum_r    = found_data;
            found_data = fs.Read("RecipeNumWAddr"); if(fs.isOK()) AddrRecipeNum_w    = found_data;
            found_data = fs.Read("CurrentTimeAddr");if(fs.isOK()) AddrCurrentTime    = found_data;
            found_data = fs.Read("ErrorCodeAddr");  if(fs.isOK()) AddrErrorCode      = found_data;
            found_data = fs.Read("PanelIdAddr");    if(fs.isOK()) AddrPanelId        = found_data;  //garly_20240930, add for panel id
#ifdef DEFECT_DETECTION
            found_data = fs.Read("DefectDetectionAddr");  if(fs.isOK()) AddrDefectDetection      = found_data;
#endif

            found_num = fs.ReadtoInt("Port");           if(fs.isOK()) PLC_PORT_std       = found_num;
            found_num = fs.ReadtoInt("PLCFormat");      if(fs.isOK()) plc_format_std     = found_num;
            found_num = fs.ReadtoInt("IsPLCSwapXY");    if(fs.isOK()) plc_swap_xy        = found_num;
            found_num = fs.ReadtoInt("PLCSignX");       if(fs.isOK()) plc_sign_x         = found_num;
            found_num = fs.ReadtoInt("PLCSignY");       if(fs.isOK()) plc_sign_y         = found_num;
            found_num = fs.ReadtoInt("PLCSignTheta");   if(fs.isOK()) plc_sign_theta     = found_num;

            ip_                     = QString::fromStdString(PLC_IP_std);
            addr_plc_trigger_       = QString::fromStdString(AddrPLCTrigger_std);
            addr_ccd_trigger_       = QString::fromStdString(AddrCCDTrigger_std);
            addr_cmd_               = QString::fromStdString(AddrCommand_std);
            addr_result_            = QString::fromStdString(AddrResult_std);
            addr_recipe_num_read_   = QString::fromStdString(AddrRecipeNum_r);
            addr_recipe_num_write_  = QString::fromStdString(AddrRecipeNum_w);
            addr_current_time_      = QString::fromStdString(AddrCurrentTime);
            addr_error_code_write_  = QString::fromStdString(AddrErrorCode);
            addr_panelId_           = QString::fromStdString(AddrPanelId);    //garly_20240930, add for panel id
#ifdef DEFECT_DETECTION
            addr_defect_detection_write_ = QString::fromStdString(AddrDefectDetection);
#endif

            port_                   = PLC_PORT_std;
            format_PLC_             = (uint8_t)plc_format_std;
            is_plc_swap_xy_         = (bool)(plc_swap_xy);
            plc_sign_x_             = plc_sign_x;
            plc_sign_y_             = plc_sign_y;
            plc_sign_theta_         = plc_sign_theta;

            if (fs.isAllOK()) {
                PLCSystemLog("[OK] Load backup setting successfully!");
                fs.close();
            }
            else {
                fs.close();
                PLCSystemLog("[Fail] Load backup setting failed: invalid reading.");
                return false;
            }

            return true; // load setting success
        } else {
            PLCSystemLog("[Fail] Load backup setting failed: file empty!");
            fs.close();
            return false;
        }
    }else {
        PLCSystemLog("[Fail] PLC backup setting file isn't exist!");
        return false;
    }
    return false;
}

bool PlcConnector::LoadDefaultSettingfromINIFile(const std::string file_path, const std::string &section_name)
{
    try{
        // FileIO. 20220419. Jimmy. #fio12
        fio::FileIO fs(file_path, fio::FileioFormat::INI);

        std::string PLC_IP_std = "";
        int PLC_PORT_std = 0;
        std::string AddrPLCTrigger_std = "";
        std::string AddrCCDTrigger_std = "";
        std::string AddrCommand_std = "";
        std::string AddrResult_std = "";
        std::string AddrRecipeNum_r = "";
        std::string AddrRecipeNum_w = "";
        std::string AddrCurrentTime = "";
        std::string AddrErrorCode = ""; //garly_20220617
        std::string AddrPanelId = "";   //garly_20240930, add for panel id
        std::string AddrDefectDetection = "";

#ifdef BARCODE_READER   //garly_20220309
        std::string AddrBarcode = "";
#endif
        int plc_format_std = 0;
        // PLC direction, Jimmy, 20220504
        int plc_swap_xy = 0;
        int plc_sign_x = 1;
        int plc_sign_y = 1;
        int plc_sign_theta = 1;

        fs.IniSetSection(section_name);
        std::string found_data = "";
        int found_num = 0;
        found_data = fs.IniRead("IP");              if(fs.isOK()) PLC_IP_std         = found_data;
        PLC_PORT_std = fs.IniReadtoInt("Port", PLC_PORT_std);
        found_data = fs.IniRead("PLCTriggerAddr");  if(fs.isOK()) AddrPLCTrigger_std = found_data;
        found_data = fs.IniRead("CCDTriggerAddr");  if(fs.isOK()) AddrCCDTrigger_std = found_data;
        found_data = fs.IniRead("CommandAddr");     if(fs.isOK()) AddrCommand_std    = found_data;
        found_data = fs.IniRead("ResultAddr");      if(fs.isOK()) AddrResult_std     = found_data;
        plc_format_std = fs.IniReadtoInt("PLCFormat", plc_format_std);
        found_data = fs.IniRead("RecipeNumRAddr");  if(fs.isOK()) AddrRecipeNum_r    = found_data;
        found_data = fs.IniRead("RecipeNumWAddr");  if(fs.isOK()) AddrRecipeNum_w    = found_data;
        found_data = fs.IniRead("CurrentTimeAddr"); if(fs.isOK()) AddrCurrentTime    = found_data;
        found_data = fs.IniRead("ErrorCodeAddr");   if(fs.isOK()) AddrErrorCode      = found_data;  //garly_20220617
        found_data = fs.Read("PanelIdAddr");        if(fs.isOK()) AddrPanelId        = found_data;  //garly_20240930, add for panel id
#ifdef DEFECT_DETECTION
        found_data = fs.Read("DefectDetectionAddr");  if(fs.isOK()) AddrDefectDetection      = found_data;
#endif
        found_num = fs.IniReadtoInt("IsPLCSwapXY");    if(fs.isOK()) plc_swap_xy        = found_num;
        found_num = fs.IniReadtoInt("PLCSignX");       if(fs.isOK()) plc_sign_x         = found_num;
        found_num = fs.IniReadtoInt("PLCSignY");       if(fs.isOK()) plc_sign_y         = found_num;
        found_num = fs.IniReadtoInt("PLCSignTheta");   if(fs.isOK()) plc_sign_theta     = found_num;

#ifdef BARCODE_READER   //garly_20220309
        found_data = fs.IniRead("BarcodeResultAddr"); if(fs.isOK()) AddrBarcode    = found_data;
#endif
        ip_ = QString::fromStdString(PLC_IP_std);
        port_ = (PLC_PORT_std);
        addr_plc_trigger_ = QString::fromStdString(AddrPLCTrigger_std);
        addr_ccd_trigger_ = QString::fromStdString(AddrCCDTrigger_std);
        addr_cmd_ = QString::fromStdString(AddrCommand_std);
        addr_result_ = QString::fromStdString(AddrResult_std);
        addr_recipe_num_read_ = QString::fromStdString(AddrRecipeNum_r);
        addr_recipe_num_write_ = QString::fromStdString(AddrRecipeNum_w);
        addr_current_time_ = QString::fromStdString(AddrCurrentTime);
        addr_error_code_write_ = QString::fromStdString(AddrErrorCode); //garly_20220617
        addr_panelId_           = QString::fromStdString(AddrPanelId);    //garly_20240930, add for panel id
#ifdef DEFECT_DETECTION
        addr_defect_detection_write_ = QString::fromStdString(AddrDefectDetection);
#endif
#ifdef BARCODE_READER   //garly_20220309
        addr_barcode_result_ = QString::fromStdString(AddrBarcode);
#endif
        format_PLC_ = (uint8_t)plc_format_std;
        is_plc_swap_xy_ = (bool)(plc_swap_xy);
        plc_sign_x_ = plc_sign_x;
        plc_sign_y_ = plc_sign_y;
        plc_sign_theta_ = plc_sign_theta;

        PLCSystemLog("[OK] Load PLC defualt setting success!");
        fs.close();

        return true;
    } catch (std::exception e){
        std::cerr << "LoadDefaultSettingfromINIFile error: " << e.what() << std::endl;
    }
    return false;
}

void PlcConnector::WriteSetting2File()
{
    try {
        QString param_dir_path = proj_path_ + "Data/system_config/";
        //move to /system_config. Jimmy 20220715.
        QString param_path = param_dir_path + "plc_setting.ini";

        // FileIO. 20220419. #fio13
        fio::FileIO fs(param_path.toStdString(), fio::FileioFormat::W);

        fs.Write("IP", ip_.toStdString());
        fs.Write("Port", std::to_string(port_));
        fs.Write("PLCTriggerAddr", addr_plc_trigger_.toStdString());
        fs.Write("CCDTriggerAddr", addr_ccd_trigger_.toStdString());
        fs.Write("CommandAddr", addr_cmd_.toStdString());
        fs.Write("ResultAddr", addr_result_.toStdString());
        fs.Write("RecipeNumRAddr", addr_recipe_num_read_.toStdString());
        fs.Write("RecipeNumWAddr", addr_recipe_num_write_.toStdString());
        fs.Write("CurrentTimeAddr", addr_current_time_.toStdString());
        fs.Write("ErrorCodeAddr", addr_error_code_write_.toStdString());    //garly_20220617
        fs.Write("PanelIdAddr", addr_panelId_.toStdString());    //garly_20240930, add for panel id
#ifdef DEFECT_DETECTION
        fs.Write("DefectDetectionAddr", addr_defect_detection_write_.toStdString());
#endif
#ifdef BARCODE_READER   //garly_20220309
        fs.Write("BarcodeResultAddr", addr_barcode_result_.toStdString());
#endif
        fs.Write("PLCFormat", std::to_string(format_PLC_));

        fs.Write("IsPLCSwapXY", std::to_string(is_plc_swap_xy_));
        fs.Write("PLCSignX", std::to_string(plc_sign_x_));
        fs.Write("PLCSignY", std::to_string(plc_sign_y_));
        fs.Write("PLCSignTheta", std::to_string(plc_sign_theta_));

        fs.close();
    }catch(...){
        //
    }
}

//garly_20220808, add for retry write when receive write fail from plc
void PlcConnector::WriteCommandRetry()
{
    switch(CurrentWriteCommandMode)
    {
        case _PLC_WRITE_CCD_TRIG_:
            PlcWriteCommand(_PLC_WRITE_CCD_TRIG_, addr_ccd_trigger_);
            break;
        case _PLC_WRITE_CCD_RESULT_:
            PlcWriteCommand(_PLC_WRITE_CCD_RESULT_, addr_result_);
            break;
        case _PLC_WRITE_PLC_TRIG_:
            PlcWriteCommand(_PLC_WRITE_PLC_TRIG_, addr_plc_trigger_);
            break;
        case _PLC_WRITE_RECIPE_NUM_:
            PlcWriteCommand(_PLC_WRITE_RECIPE_NUM_, addr_recipe_num_write_);
            break;
    #ifdef BARCODE_READER
        case _PLC_WRITE_BARCODE_RESULT_:
            PlcWriteCommand(_PLC_WRITE_BARCODE_RESULT_, addr_barcode_result_);
            break;
    #endif
        case _PLC_WRITE_ERROR_CODE_:
            PlcWriteCommand(_PLC_WRITE_ERROR_CODE_, addr_error_code_write_);
            break;
    #ifdef DEFECT_DETECTION
        case _PLC_WRITE_DEFECT_DETECTION_:
            PlcWriteCommand(_PLC_WRITE_DEFECT_DETECTION_, addr_defect_detection_write_);
            break;
    #endif
        default:
            break;
    }
}
#ifdef SYNC_TIME_FROM_PLC
void PlcConnector::setCurrentSystemTime(uint16_t yy, uint16_t mm, uint16_t dd, uint16_t h, uint16_t m, uint16_t s)
{
    //char *m= "echo 50511881635 | sudo -S date -s\"" + QString("%1%2%3 %4:%5:%6").arg(yy).arg(mm).arg(dd).arg(h).arg(m).arg(s) +"\"";
    QString str = QString("echo 50511881635 | sudo -S date -s \"%1%2%3 %4:%5:%6\"").arg(yy).arg(mm, 2, 10, QLatin1Char('0')).arg(dd, 2, 10, QLatin1Char('0'))
            .arg(h, 2, 10, QLatin1Char('0')).arg(m, 2, 10, QLatin1Char('0')).arg(s, 2, 10, QLatin1Char('0'));
    system(str.toLocal8Bit());
}

void PlcConnector::setSyncTimeMode(bool status, uint8_t index)
{
    enableSyncTime = status;

    switch(index)
    {
        case 0: // 30min
            syncTimeUnit = 30;
            break;
        case 1: //1hr
            syncTimeUnit = 30*2;
            break;
        case 2: //6hr
            syncTimeUnit = 30*2*6;
            break;
        case 3: //12hr
            syncTimeUnit = 30*2*12;
            break;
        case 4: //24hr
            syncTimeUnit = 30*2*24;
            break;
        case 5:	//disable
        default:
            syncTimeUnit = 0;
            break;
    }
}
#endif

