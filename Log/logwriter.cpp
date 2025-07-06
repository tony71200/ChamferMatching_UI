#include "logwriter.h"

namespace logwriter {

    LogWriter::LogWriter() {
        Init();
//        fs::create_directories(log_path);
    }

    LogWriter::LogWriter(std::string path, std::string name, struct tm* pTime) {
        Init();
        log_path = path;
        fs::create_directories(log_path);
        log_name = name;
        pTm = pTime;

        log_status = "[NULL]";
        init_finished = true;
    }

    LogWriter::~LogWriter() {
        fflush(stdout);
//        delete pTm;
        pTm = nullptr;

    }

    void LogWriter::Init()
    {
        file_name = "";
        file_num = 0;
        init_finished = false;
        // Init current time
        time_t lt0;
        lt0 = time(NULL);
        pTm = localtime(&lt0);
        log_path = "./Log";
        log_name = "etc";
        is_first = true;
        show_log_date = true;
        show_cv_imshow = SHOW_CV_IMSHOW;
        log_buffer = "";
        any_log_buffer = "";
        write_all = false;
        merge_one_day = true;
    }

    int LogWriter::GetShowCvImshow() {
        return show_cv_imshow;
    }
    void LogWriter::SetShowCvImshow(int do_show) {
        show_cv_imshow = do_show;
    }

    void LogWriter::SetDirPath(std::string path) {
        if (path.size() > 0) {
            std::string::iterator str_itr = path.end();
            if (*(str_itr - 1) == '/') {
                path.erase(path.end() - 1);
            } else {
                // Do nothing
            }
        }else{
            path = "./Log";
        }
        log_path = path;
        if (!fs::exists(log_path)) {
            fs::create_directories(log_path);
        }
        init_finished = true;
    }

    void LogWriter::SetLogName(std::string name) {
        log_name = name;
    }

    void LogWriter::SetLogStat(std::string stat) {
        log_status = stat;
        show_log_date = true;
    }

    void LogWriter::SetShowDate(bool do_show)
    {
        show_log_date = do_show;
    }

    struct tm* LogWriter::GetSysBeginTime() {
        return pTm;
    }

    void LogWriter::WriteLog(std::string str) {

        std::string log_all;
        std::string log_date;
        std::string date_dir;

        ushort str_size = str.size();

        time_t lt0;
        lt0 = time(NULL);

        struct tm* local_pTm;
        local_pTm = localtime(&lt0);

        std::string tm_year = std::to_string(local_pTm->tm_year + 1900);
        std::string tm_mon = (local_pTm->tm_mon > 8 ? "" : "0" ) + std::to_string(local_pTm->tm_mon + 1);
        std::string tm_mday = (local_pTm->tm_mday > 9 ? "" : "0") + std::to_string(local_pTm->tm_mday);

        std::string tm_hour = (local_pTm->tm_hour > 9 ? "" : "0") + std::to_string(local_pTm->tm_hour);
        std::string tm_min = (local_pTm->tm_min > 9 ? "" : "0") + std::to_string(local_pTm->tm_min);
        std::string tm_sec = (local_pTm->tm_sec > 9 ? "" : "0") + std::to_string(local_pTm->tm_sec);

        log_date = std::string("[")
            + tm_hour
            + ":" + tm_min
            + ":" + tm_sec + "]";

        if (is_first) {
            // print date
            if (show_log_date) {
                log_all = log_date + " " + str;
            }
            else {
                log_all = str;
            }

            is_first = false;
        }
        else {
            log_all = str;
        }

        if (str[str_size - 1] == '\n') {
            is_first = true;
        }

        std::string name_log_path;
        std::string file_path;
        date_dir = tm_year
                + tm_mon
                + tm_mday;
        if(merge_one_day){
            log_date = tm_year
                + "_" + tm_mon
                + "_" + tm_mday;
        }
        else{
            log_date = tm_year
                + "_" + tm_mon
                + "_" + tm_mday
                + "_" + tm_hour;
        }
        name_log_path = log_path + "/" + log_name + "/" + date_dir;
        if(!fs::exists(name_log_path))
            fs::create_directories(name_log_path);

        if(file_name != (log_date + "_" + log_name)){
            file_name = log_date + "_" + log_name;
            file_num = 0;
        }
        std::fstream fs_log;
        file_path = name_log_path + "/" + log_date + "_" + log_name + "_" + std::to_string(file_num) + ".txt";
        if(fs::exists(file_path)){
            if(fs::file_size(file_path) > 10000000){
                file_num++;
            }
        }

        fs_log.open(file_path, std::ios::app);
        fs_log << log_all;
        fs_log.close();


        if(write_all){
            std::string all_log_path;
            all_log_path = log_path + "/all/" + date_dir;
            if(!fs::exists(all_log_path))
                fs::create_directories(all_log_path);

            file_path = all_log_path + "/" + log_date + "_" + log_name + "_" + std::to_string(file_num) + ".txt";
            fs_log.open(file_path, std::ios::app);
            fs_log << log_all;
            fs_log.close();
        }

        fflush(stdout);
//        delete local_pTm;
    }
    void LogWriter::WriteLog(int log) {
        std::string str = std::to_string(log);
        WriteLog(str);
    }

    void LogWriter::WriteLog(float log) {
        std::string str = std::to_string(log);
        WriteLog(str);
    }

    void LogWriter::WriteLog(double log) {
        std::string str = std::to_string(log);
        WriteLog(str);
    }

    void LogWriter::WriteLog(std::size_t log) {
        std::string str = std::to_string(log);
        WriteLog(str);
    }
    //size_type

    void LogWriter::WriteLog(cv::Point log) {
        std::string str = "[";
        str += std::to_string(log.x);
        str += ", ";
        str += std::to_string(log.y);
        str += "]";
        WriteLog(str);
    }

    void LogWriter::WriteLog(cv::Mat log) {
        std::string log_all = "";
        int cols = log.cols;
        int rows = log.rows;
        uchar depth = log.type() & CV_MAT_DEPTH_MASK;
        log_all += "[ ";
        for (int i = 0; i < rows; i++) {
            for (int j = 0; j < cols; j++) {

                switch (depth) {
                case CV_8U:  log_all += std::to_string(log.at<uchar>(i, j)); break;
                case CV_8S:  log_all += std::to_string(log.at<schar>(i, j)); break;
                case CV_16U: log_all += std::to_string(log.at<ushort>(i, j)); break;
                case CV_16S: log_all += std::to_string(log.at<short>(i, j)); break;
                case CV_32S: log_all += std::to_string(log.at<int>(i, j)); break;
                case CV_32F: log_all += std::to_string(log.at<float>(i, j)); break;
                case CV_64F: log_all += std::to_string(log.at<double>(i, j)); break;
                default:     break;
                }

                if (i != (rows - 1) || j != (cols - 1)) {
                    log_all += ", ";
                }
            }
            if (i != (rows - 1)) {
                log_all += "\n";
            }
        }
        log_all += " ]\n";

        WriteLog(log_all);
    }

    void LogWriter::WriteLineChange() {
        WriteLog("\n");
    }

    LogWriter& LogWriter::operator<<(int log) {
        WriteLog(log);
        return *this;
    }
    LogWriter& LogWriter::operator<<(float log) {
        WriteLog(log);
        return *this;
    }
    LogWriter& LogWriter::operator<<(double log) {
        WriteLog(log);
        return *this;
    }
    LogWriter& LogWriter::operator<<(std::size_t log) {
        WriteLog(log);
        return *this;
    }
    LogWriter& LogWriter::operator<<(cv::Point log) {
        WriteLog(log);
        return *this;
    }
    LogWriter& LogWriter::operator<<(cv::Mat log) {
        WriteLog(log);
        return *this;
    }
    LogWriter& LogWriter::operator<<(std::string log) {
        WriteLog(log);
        return *this;
    }

    LogWriter& LogWriter::operator<<(LogWriter& (*pf)(LogWriter& lw)) {
        return pf(*this);
    }
    // Jang 20220411
    const std::string LogWriter::GetCurrentTimeStr()
    {
        time_t lt0;
        lt0 = time(NULL);
        struct tm* local_pTm;
        local_pTm = localtime(&lt0);
        std::string tm_year = std::to_string(local_pTm->tm_year + 1900);
        std::string tm_mon = (local_pTm->tm_mon > 8 ? "" : "0" ) + std::to_string(local_pTm->tm_mon + 1);
        std::string tm_mday = (local_pTm->tm_mday > 9 ? "" : "0") + std::to_string(local_pTm->tm_mday);
        std::string tm_hour = (local_pTm->tm_hour > 9 ? "" : "0") + std::to_string(local_pTm->tm_hour);
        std::string tm_min = (local_pTm->tm_min > 9 ? "" : "0") + std::to_string(local_pTm->tm_min);
        std::string tm_sec = (local_pTm->tm_sec > 9 ? "" : "0") + std::to_string(local_pTm->tm_sec);
        std::string log_date = std::string("")
                                + tm_year + tm_mon + tm_mday
                                + "_" + tm_hour
                                + "_" + tm_min
                                + "_" + tm_sec;

        return log_date;
    }

}
