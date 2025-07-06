#ifndef ABSTRACTMATCH_H
#define ABSTRACTMATCH_H

#include <iostream>
#include <algorithm>
#include <vector>
#include <string>
#include <cmath>

#include <time.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <opencv2/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/imgproc/types_c.h>

/**
 * @brief The AbstractMatch class
 * To build a match method/algorithm.
 * @author Jimmy Shen. 20220421.
 */

struct MatchData
{
    int x;
    int y;
    float scale;
    float angle;
    float coef;
    MatchData() {}
    MatchData(int x, int y, float s, float a, float coef)
        : x{x}, y{y}, scale{s}, angle{a}, coef{coef}
    {}
};

template <typename T>
inline T clip(const T val, const T lower, const T upper){
    return std::max(lower, std::min(val, upper));
}

class AbstractMatch
{
public:
    AbstractMatch(){}
    virtual ~AbstractMatch(){}

    virtual void SetupImages(const cv::Mat& temp_img, const cv::Mat& src_img);

    // return time(double, ms)
    virtual double Train() = 0;
    virtual double Inference() = 0;

    std::vector<MatchData> GetResults() const { return result_cands_; }

protected:
    cv::Mat template_img_;
    cv::Mat source_img_;
    std::vector<MatchData> result_cands_;

    // functions
    /// function for getting currenct time.
    /// How to use:
    ///     struct timespec before, after;
    ///     GetMonotonicTime(&before);
    ///     ...
    ///     GetMonotonicTime(&after);
    void GetMonotonicTime(struct timespec *ts);

    /// function for computing elapsed time btw time struct before and after.
    /// How to use:
    ///     double runtime_in_second = GetElapsedTime(&before, &after);
    double GetElapsedTime(struct timespec *before, struct timespec *after);

};

inline void AbstractMatch::SetupImages(const cv::Mat &temp_img, const cv::Mat &src_img)
{
    template_img_ = temp_img;
    source_img_ = src_img;
}

inline void AbstractMatch::GetMonotonicTime(timespec *ts) {
#ifdef __MACH__
    clock_serv_t cclock;
    mach_timespec_t mts;
    host_get_clock_service(mach_host_self(), SYSTEM_CLOCK, &cclock);
    clock_get_time(cclock, &mts);
    mach_port_deallocate(mach_task_self(), cclock);
    ts->tv_sec = mts.tv_sec;
    ts->tv_nsec = mts.tv_nsec;
#else
    clock_gettime(CLOCK_MONOTONIC, ts);
#endif
}

inline double AbstractMatch::GetElapsedTime(timespec *before, timespec *after) {
    double deltat_s  = after->tv_sec - before->tv_sec;
    double deltat_ns = after->tv_nsec - before->tv_nsec;
    return deltat_s + deltat_ns * 1e-9;
}

#endif // ABSTRACTMATCH_H
