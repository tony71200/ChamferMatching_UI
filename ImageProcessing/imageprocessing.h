#ifndef IMAGEPROCESSING_H
#define IMAGEPROCESSING_H

#include <iostream>
#include <fstream>

#include <opencv2/core/mat.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/calib3d.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/core/utility.hpp>

//#include <opencv2/core/ocl.hpp>
//#include <opencv2/core/cuda.hpp>
//#include <opencv2/cudaarithm.hpp>
//#include <opencv2/cudaimgproc.hpp>
//#include <opencv2/cudafilters.hpp>

#include "imageparams.h"
#include "../Log/logwriter.h"
#include "../globalparams.h"
//#include "../FileIO/fileiomanager.h"

#if defined(_WIN32)
    #include <filesystem> // if Windows
    //using namespace std::filesystem;
    namespace stdfs = std::filesystem;
#elif defined(__unix)
    #include <experimental/filesystem> // if Linux on Jetson
    //using namespace std::experimental::filesystem;
    namespace stdfs = std::experimental::filesystem;
#endif

//namespace fm = file_io_manager;
class ImageProcessing
{
public:
    ImageProcessing();
    ~ImageProcessing();

private:
    int selected_img;
    std::string save_path;
    cv::Mat orig_img;
    cv::Mat processed_img;

    ImageParams current_iparam;
    bool skip_blur_;
    bool skip_threshold_contour_;
    // Jang 20220330
    std::vector<std::vector<cv::Point>> current_contours_;

    // Trackbar
private:
    logwriter::LogWriter lw;


public:
    bool SetSaveDir(std::string path);
    void InitLog(std::string dir_path, std::string name);
    std::string GetSaveDir();
    void SaveParams(ImageParams iparam);
    bool LoadParams(ImageParams& iparam);
    bool LoadINIParams(std::ifstream& fs, ImageParams& iparam);
    void SkipBlurContour(bool skip_blur, bool skip_thres_contour);


    void UploadImage(cv::Mat img);
    void ProcessContour(cv::Mat& img, cv::Mat& out_img, ImageParams iparam);
    void GetImageBySavedInfo(cv::Mat& img, cv::Mat& out_img, bool use_cuda = false);
    void GetImageBySavedInfo(cv::Mat& img, cv::Mat& out_img, ImageParams iparam);
    void GetImageBySavedInfo(cv::Mat& img, cv::Mat& out_img, ImageParams iparam, bool use_cuda);
    // Jang 20220330
    std::vector<std::vector<cv::Point>> GetCurrentContourVector();
    // Jang 20220630
    void AdjustParametersForDownsampling(ImageParams input_param, ImageParams& output_param, int downsample_val);


    cv::Mat SelectImage(const int img_num);
    cv::Mat UpdateImage(const int blur, const int thres);

};

#endif // IMAGEPROCESSING_H
