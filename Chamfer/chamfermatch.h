#ifndef CHAMFERMATCH_H
#define CHAMFERMATCH_H

#define THRUST_IGNORE_CUB_VERSION_CHECK

#include "abstractmatch.h"

#include <stdio.h>
#include <cstdlib>
#include <cmath>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <vector>
#include <string.h>
#include <cmath>

#include <unistd.h> // UNIX

#include <opencv2/highgui/highgui.hpp>
#include <opencv2/core/utility.hpp>
#include <opencv2/imgproc.hpp>

#include <thrust/host_vector.h>
#include <thrust/device_vector.h>
#include <thrust/functional.h>
#include <thrust/extrema.h>
#include <thrust/device_ptr.h>

#include <cuda.h>
#include <cuda_runtime.h>
#include <cuda_profiler_api.h>
#include <fstream>
#include <iostream>

#include "../globalparams.h"
#include "../ImageProcessing/imageprocessing.h"
#include "../Log/logwriter.h"
//#include "FileIO/fileiomanager.h"
#include "../FileIO/fileio.h"
//#include "INIManager/inimanager.h"

//namespace fm = file_io_manager;
namespace fio = file_io;

struct TrainTemplate{
    std::vector<cv::Mat> templates;
    std::vector<cv::Mat> templates_gray;
    std::vector<float> template_angle;
    std::vector<float> template_scale;

    bool is_template_ok;

    int num_templates = 0;
    int total_memory_size = 0;

    std::vector<cv::Mat> non_zero_area_templates;
    int* num_non_zeros = nullptr;

    // Flat pointer size : rotations x scalings x coorindates
    // Why just single flat pointer? Double pointer is difficult to be allocated in CUDA.
    short* flat_template_non_zero_col_ = nullptr;
    short* flat_template_non_zero_row_ = nullptr;

    // Jang 20223030
    std::vector<cv::Mat> template_weights;
    float* flat_template_weight_ = nullptr;
};

struct PreprocessedSource
{
    cv::Mat chamfer_img;

    short* source_candidate_col = nullptr;
    short* source_candidate_row = nullptr;
    float* flat_score_map = nullptr;
    float* flat_source_dist_transfrom = nullptr;// = (float*)chamfer_img.data;

    int length_source_candidate;
    int source_num_candidate_width;
    int source_num_candidate_height;
};


struct TrainTemplatePtr{
    short width;
    short height;

    std::vector<cv::Mat> templates;
    std::vector<cv::Mat> templates_gray;
    std::vector<float> template_angle;
    std::vector<float> template_scale;

    bool is_template_ok;

    int num_templates = 0;  // rot + tra
    int total_memory_size = 0;  // total non zero mat size

    std::vector<cv::Mat> non_zero_area_templates;
    std::shared_ptr<int[]> num_non_zeros; // short* num_non_zeros = nullptr;

    // Flat pointer size : rotations x scalings x coorindates
    // Why just single flat pointer? Double pointer is difficult to be allocated in CUDA.
    std::shared_ptr<short[]> flat_template_non_zero_col_; //short* flat_template_non_zero_col_ = nullptr;
    std::shared_ptr<short[]> flat_template_non_zero_row_; // short* flat_template_non_zero_row_ = nullptr;

    std::vector<cv::Mat> template_weights;
    std::shared_ptr<float[]> flat_template_weight_; // float* flat_template_weight_ = nullptr;
};

// Jang 20220530
struct PreprocessedSourcePtr
{
    cv::Mat chamfer_img;
    short width;
    short height;
    short num_pyrmd;    //short resize_scale = std::pow(2, num_pyrmd - 1); // 1, 2, 4, 8, ...

    std::shared_ptr<short[]> source_candidate_col; // short* source_candidate_col = nullptr;
    std::shared_ptr<short[]> source_candidate_row; // short* source_candidate_row = nullptr;
    std::shared_ptr<float[]> flat_score_map; // float* flat_score_map = nullptr;
    float* flat_source_dist_transfrom = nullptr; // This pointer is belong to a Mat, so that the smart pointer cannot be used.

    int length_source_candidate;
    int source_num_candidate_width;
    int source_num_candidate_height;
};

struct MatchDefaultParams{
    // RST
    int down_sampling = 4;
    float _scaleInterval = 0.1;
    float _minScale = 0.9;
    float _maxScale = 1.1;
    float _angleInterval = 10.0;
    int _circleNum = 5;

    float _cifiThreshold = 0.6; /// JANG: Circle matching threshold
    float _rafiThreshold = 0.6; /// JANG: Angle mathcing threshold
    float _tefiThreshold = 0.7;
    float _tefiThreshold2 = 0.7;    /// JANG: rotation matching threshold

    float _posMatchThreshold = 0.6;
    float _scaleMatchThreshold = 0.6;

    // Chamfer
    float _angleMin = -45.0;
    float _angleMax = 45.0;
    float _angleCoarseInterval = 1.0;
    float _angleFineInterval = 0.1;
    int skip_pixel_ = 5;
    float chamfer_score_threshold_ = 3;
    float chamfer_weight_discount_  = 0.1;
};

static double clock_diff (struct timespec *t1, struct timespec *t2)
{ return t2->tv_sec - t1->tv_sec + (t2->tv_nsec - t1->tv_nsec) / 1e9; }
bool CompareSize(const MatchData& data_0, const MatchData& data_1);
int ParseLine(char* line);
int GetCurrentMem();

class CudaChamfer : public AbstractMatch
{
public:
    const double PI = 3.14159265358979323846;
    CudaChamfer(){
    }
    CudaChamfer(std::string data_path){
        InitPaths(data_path, data_path + "log", data_path + "../../../" + parameter_matching_file);
    }
    ~CudaChamfer();
    void InitPaths(std::string data_path, std::string log_path, std::string params_path);
    std::string GetDirPath();
    bool CreateDefaultParamsIniFile(const std::string& path);
    bool CreateParamsIniFile(const std::string& path);
    void LoadParamsIniFile(const std::string& params_path);

    std::string GetErrorMsg(){
        return fail_msg;
    }
    std::vector<MatchData> GetResult() const { return rslt_match; }
    float GetTestingTime() { return t_execute; }
    int GetPyramidNum() { return down_sampling; }


    void SetLogPath(std::string dir_path); // "./example" not including '/' at the end
    float GetTemplateScore() { return template_score; }

    int loop_escaper = 0;
    // Chamfer matching
    int test_count = 0;
    // Jang 20211207
    bool IsChamferTemplateOk();
    float ChamferTrain(cv::Mat template_img);
    float LoadChamferData();
    float ChamferInference(cv::Mat source_img, bool use_weight);
    float ChamferInference2(cv::Mat source_img, bool use_weight);
    double Train(/*template_img_*/);
    double Inference(/*template_img_, source_img_*/);
    void SetupImages(const cv::Mat& template_img, const cv::Mat& src_img);
    void ShowResultImage(std::string saveFilename);
    std::vector<cv::Point> GetPointResult();
    // Jang 202203330
    void SetWeightForChamfer(bool use_weight){
        if(use_weight){
            this->use_weight_ = 1;
        }
        else{
            this->use_weight_ = 0;
        }
    }
    void SetImageProc(ImageProcessing* img_proc, bool use_image_proc, bool use_cuda_for_improc){
        this->image_proc = img_proc;
        this->use_image_proc = use_image_proc;
        this->use_cuda_for_improc = use_cuda_for_improc;
    }
    /// Variables
    /// Chamfer matchinig (non-downsampling)
    TrainTemplate trained_template;
    TrainTemplate trained_template_pyr; // after downsampling
    PreprocessedSource preprocessed_source;
    PreprocessedSource preprocessed_source_pyr;

    // Jang 20220609
    bool use_only_start_and_end_ = true;    // e.g.) if the number of pyramid is 4, only both 0 and 3 will be computed. (skip the others between start and end.)
    std::vector<TrainTemplatePtr> trained_template_vec_;
    PreprocessedSourcePtr pre_proc_src_ptr_;
    std::vector<PreprocessedSourcePtr> pre_proc_src_vec_;
    // Jang 20220711
    template <typename T>
    void MakeSharedPtr(std::shared_ptr<T[]>& shared_ptr_char, int size);
    void MakeSharedPtr(std::shared_ptr<uchar[]>& shared_ptr_char, int size);
    void MakeSharedPtr(std::shared_ptr<char[]>& shared_ptr_char, int size);
    void MakeSharedPtr(std::shared_ptr<short[]>& shared_ptr_short, int size);
    void MakeSharedPtr(std::shared_ptr<int[]>& shared_ptr_int, int size);
    void MakeSharedPtr(std::shared_ptr<float[]>& shared_ptr_float, int size);
    void MakeSharedPtr(std::shared_ptr<double[]>& shared_ptr_double, int size);

    // Jang 202203330
    void SetTemplateMask(cv::Mat template_mask);
    void ClearTemplateMask();
    void SetSrcImageMask(cv::Mat source_img_mask);
    void ClearSrcImageMask();
    // Jang 20220406
    void SetEnableSrcImageMask(bool enable_mask);
    void GetSrcImageMask(cv::Mat& source_img_mask);
    void checkCUDAandSysInfo();

private:
    // Jimmy. 20211116
    // For Chamfer Match image processing
    void createTempNonZeroMat(cv::Mat orig_temp, TrainTemplate &trained_template,
                              float angle_min, float ange_max,
                              float angle_interval, int skip, std::string path);
    void createSrcNonZeroMat(cv::Mat orig_src, PreprocessedSource &preprocessed_source,
                             int temp_w, int temp_h, int src_w, int src_h,
                             std::string path);
    //void checkCUDAandSysInfo();

    bool LoadChamferTemplate(TrainTemplate& trained_template,
                                    float angle_min, float angle_max, float angle_interval, std::string path);
    void LoadChamferTemplate(std::vector<TrainTemplatePtr> &trained_template,
                            float angle_min, float angle_max,
                             float fine_angle_interval, float coarse_angle_interval, std::string path);
    bool LoadChamferTemplate2(std::vector<TrainTemplatePtr> &trained_template,
                            float angle_min, float angle_max,
                             float fine_angle_interval, float coarse_angle_interval, std::string path);
    void SortScores(float* cuda_flat_score_map, int* cuda_flat_score_map_index,
                    float* score_candidates_result, int* score_candidates_result_index,
                    int num_scores, int target_num_scores, int steps,
                    bool is_fine_detection);
    void SortScores(float* cuda_flat_score_map, int* cuda_flat_score_map_index,
                    float* score_candidates_result, int* score_candidates_result_index,
                    int num_scores_copy, int steps,
                    std::vector<int>& num_score_list, std::vector<int>& divider_list,
                    bool is_fine_detection);
    // Jang 20220627 // Rotate contour image, not rotating original image and finding contours.
    void CreateTempNonZeroMat2(cv::Mat orig_temp, std::vector<TrainTemplatePtr> &trained_template,
                              float angle_min, float ange_max,
                              float fine_angle_interval, float coarse_angle_interval, int skip, std::string path);
    void CreateSrcNonZeroMat2(cv::Mat orig_src,
                               std::vector<PreprocessedSourcePtr> &preprocessed_source,
                               short num_pyrmd,
                               int temp_w, int temp_h,
                               std::string path);

private:
    // Jang 20220711
    template<typename T>
    bool FreeCudaMemory(T** cuda_pointer_);
    template<typename T>
    void FreeMemory(T** pointer_);

    bool FreeCudaMemory(short** cuda_pointer_);
    bool FreeCudaMemory(int** cuda_pointer_);
    bool FreeCudaMemory(float** cuda_pointer_);
    bool FreeCudaMemory(double** cuda_pointer_);
    bool FreeCudaMemory(uchar** cuda_pointer_);
    void FreeMemory(short** pointer_);
    void FreeMemory(int** pointer_);
    void FreeMemory(float** pointer_);
    void FreeMemory(double** pointer_);
    void FreeMemory(uchar** pointer_);

private:
    // Image pre-processing
    ImageProcessing* image_proc;
    bool use_image_proc = false;
    bool use_cuda_for_improc = false;

private:
    std::vector<MatchData> rslt_match;    // matching result
    float t_execute;         // testing time
    float template_score = 0.0;   // Checking whether the selected template is usable or not
    float chamfer_template_score = 0.0;

    std::string fail_msg = "";
    float threshold_score_py = 10;   // 4
    float max_score_py = 15;    // 10
    float threshold_score_orig = 20; // 9
    float max_score_orig = 30;  // 20

    // not yet to fix
    float CQ2;
    int _countrq;
    int _countcq;
    int countcq;
    float meanCQ;
    int templConv;
    int numpixel;
    float rrqi2;
    float meanrq_orig;

    // JANG
    MatchDefaultParams default_params;
    // Parameters
    int down_sampling = 4;
    float _scaleInterval = 0.1;
    float _minScale = 0.9;
    float _maxScale = 1.1;
    float _angleInterval = 10.0;
    int _circleNum = 5;

    float _cifiThreshold = 0.6; /// JANG: Circle matching threshold
    float _rafiThreshold = 0.6; /// JANG: Angle mathcing threshold
    float _tefiThreshold = 0.7;
    float _tefiThreshold2 = 0.7;    /// JANG: rotation matching threshold

    float _posMatchThreshold = 0.6;
    float _scaleMatchThreshold = 0.6;

    std::string data_path;
    std::string params_path_;
    logwriter::LogWriter lw;

    cv::Mat orig_source_img_mat_;
    cv::Mat orig_template_img_mat_;
    cv::Mat pyr_source_img_mat_;
    cv::Mat pyr_template_img_mat_;

    cv::Mat orig_source_contours_mat_;

    // Jang 202203330
    bool template_mask_exists_ {false};
    cv::Mat template_mask_;
    cv::Mat pyr_template_mask_;
    bool src_mask_exists_ {false};
    cv::Mat source_img_mask_;
    cv::Mat pyr_source_img_mask_;
    short use_weight_ {1};
    // Chamfer threshold
    int down_sampling_chamfer_ = 2; // 0, 1, 2, 3, ...  // Jang 20220629
    float angle_min_ = -10.0;
    float angle_max_ = 10.0;
    float angle_interval_coarse_align_ = 1.0;
    float angle_interval_fine_align_ = 0.1;
    int skip_pixel_ = 1;
    float chamfer_score_threshold_ = 3;
    float chamfer_weight_discount_  = 0.5;
    // Jang 20220413
    bool expand_img_ = true;
    int chamfer_method_ = 0; // Chamfer method selector // 0: original, 1 < : others, 3: current method // Jang 20220711

    // File paths
    const std::string parameter_matching_file = "matching_parameters.ini";
    const std::string result_score_path = "result_score.data";
    const std::string result_matched_area_path = "result_matched_area.data";
};

#endif // CHAMFERMATCH_H
