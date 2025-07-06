#ifndef TIMELOG_H
#define TIMELOG_H

#include "logwriter.h"

struct ElapsedTime{
    float time;
    std::string str;
};

class TimeLog
{
public:
    TimeLog();
    ~TimeLog();

    void SetProjectPath(std::string proj_path);
    void StartRecordingTime(std::string name);
    void SaveElapsedTime(std::string name);
    void FinishRecordingTime(std::string name);
    std::vector<ElapsedTime> GetElapsedTimeList();

private:
    bool is_path_set_;
    bool is_record_started_;
    std::string project_path_;

    logwriter::LogWriter lw;

    std::vector<ElapsedTime> elapsed_time_;
    struct timespec elapsed_start_;
    struct timespec elapsed_save_;
    struct timespec elapsed_end_;


    double clock_diff (struct timespec *t1, struct timespec *t2);


};

#endif // TIMELOG_H
