#include "timelog.h"

TimeLog::TimeLog()
{
    this->project_path_ = "./";
    is_path_set_ = false;
    is_record_started_ = false;
}

TimeLog::~TimeLog(){
    elapsed_time_.clear();
}

double TimeLog::clock_diff (struct timespec *t1, struct timespec *t2)
{
    return t2->tv_sec - t1->tv_sec + (t2->tv_nsec - t1->tv_nsec) / 1e9;
}

void TimeLog::SetProjectPath(std::string proj_path){
    this->project_path_ = proj_path;

    lw.SetDirPath(project_path_ + "/Log");
    lw.SetLogName("elapsed_time");
    lw.SetShowDate(true);
    is_path_set_ = true;
}

void TimeLog::StartRecordingTime(std::string name){
    if(!is_path_set_) SetProjectPath("./Log");
    elapsed_time_.clear();
    clock_gettime(CLOCK_MONOTONIC, &elapsed_start_);
    clock_gettime(CLOCK_MONOTONIC, &elapsed_save_);

    ElapsedTime e_time;
    e_time.str = name;
    e_time.time = 0.00;

    elapsed_time_.push_back(e_time);
    is_record_started_ = true;

    lw << "[START] " << name << ", 0.0" << lw.endl;
}

void TimeLog::SaveElapsedTime(std::string name){
    if(!is_record_started_) {
        lw << "[WARNNING] TimeLog isn't started. " << __func__ << ", " << name << lw.endl;
        return;
    }
    clock_gettime(CLOCK_MONOTONIC, &elapsed_end_);
    float elapsed = clock_diff (&elapsed_save_, &elapsed_end_);
    ElapsedTime e_time;
    e_time.str = name;
    e_time.time = elapsed;
    clock_gettime(CLOCK_MONOTONIC, &elapsed_save_);

    lw << "[Elapsed Time] " << name << ", " << elapsed << lw.endl;
}

void TimeLog::FinishRecordingTime(std::string name){
    if(!is_record_started_) {
        lw << "[WARNNING] TimeLog isn't started. " << __func__ << ", " << name << lw.endl;
        return;
    }
    clock_gettime(CLOCK_MONOTONIC, &elapsed_end_);
    float elapsed = clock_diff (&elapsed_start_, &elapsed_end_);
    ElapsedTime e_time;
    e_time.str = name;
    e_time.time = elapsed;

    is_record_started_ = false;
    lw << "[Elapsed Time - Total] " << name << ", " << elapsed << lw.endl;
}

std::vector<ElapsedTime> TimeLog::GetElapsedTimeList(){
    return elapsed_time_;
}
