#pragma once
#ifndef __LOG_WRITER_HPP__
#define __LOG_WRITER_HPP__

#if defined(_WIN32)
    #pragma warning(disable:4996)
    #include <experimental/filesystem>
    namespace fs = std::experimental::filesystem;
//    #include <filesystem> // if Windows
//    namespace fs = std::filesystem;
#elif defined(__unix)
    #include <experimental/filesystem> // if Linux on Jetson
    //using namespace std::experimental::filesystem;
    namespace fs = std::experimental::filesystem;
#endif

#include <iostream>
#include <sstream>
#include <fstream>
#include <ctime>
#include <string>
#include <any>

//#define _CRT_SECURE_NO_WARNINGS

#include <iostream>
#include <fstream>
#include <ctime>

#include <opencv2/core/mat.hpp>
#include <opencv2/imgproc.hpp>

#define BAN_CV_IMSHOW 0
#define SHOW_CV_IMSHOW 1

namespace logwriter {

    class LogWriter {
    public:
        LogWriter();
        LogWriter(std::string path, std::string name, struct tm* pTime);
        ~LogWriter();
        void Init();
        bool IsReady(){
            return init_finished;
        }

    private:
        std::string log_buffer;
        std::string any_log_buffer;
    private:
        bool init_finished;
        bool is_first;
        bool show_log_date;
        bool merge_one_day;
        bool write_all;
        std::string file_name;
        int file_num;
        std::string log_path;
        std::string log_name;
        std::string log_status;
        struct tm* pTm;

    public:
        int show_cv_imshow; // For admin
    public:
        int GetShowCvImshow();
        void SetShowCvImshow(int do_show);

    public:
        void SetDirPath(std::string path);
        void SetLogName(std::string name);
        void SetLogStat(std::string stat);
        void SetShowDate(bool do_show);

        struct tm* GetSysBeginTime();
        // Jang 20220411
        const std::string GetCurrentTimeStr();

        void WriteLog(std::string str);
        void WriteLog(int log);
        void WriteLog(float log);
        void WriteLog(double log);
        void WriteLog(std::size_t log);
        void WriteLog(cv::Point log);
        void WriteLog(cv::Mat log);

        void WriteLineChange();


        LogWriter& operator<<(int log);
        LogWriter& operator<<(float log);
        LogWriter& operator<<(double log);
        LogWriter& operator<<(std::size_t log);
        LogWriter& operator<<(std::string log);
        LogWriter& operator<<(cv::Point log);
        LogWriter& operator<<(cv::Mat log);

        LogWriter& operator<<(LogWriter& (*pf)(LogWriter& lw));

        static LogWriter& endl(LogWriter& lw) {
            lw << "\n";
            return lw;
        }

    };
}
#endif
