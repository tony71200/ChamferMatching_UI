#ifndef MATCHINGCONTAINER_H
#define MATCHINGCONTAINER_H

#include <string>
#include <vector>
#include <opencv2/opencv.hpp>
#include "chamfermatch.h"
#include "fastrst.h"


//* @brief Enum to define the matching algorithm to be used
enum class MatchingMode {
    CHAMFER, FASTRST, SHAPE_ALIGNMENT
};

class MatchingContainer
{
public:
    MatchingContainer(const std::string& root_path = "./", const std::string& recipe_name = "default_recipe");
    ~MatchingContainer();

    void SetRecipe(const std::string& root_path, const std::string& recipe_name);

    void SetMode(MatchingMode mode);

    void SetUseCudaForImageProcessing(bool use_cuda);

    float Train(const cv::Mat& template_img);

    float Test(const cv::Mat& source_img);

    std::vector<cv::Point> GetResult() const;

    cv::Mat GetTrainedTemplateImg() const;
    bool IsTrainedOK();
    MatchingMode GetMode() {
        return m_current_mode;
    };
private:

    void initializePaths();
    //=============Chamfer Matching=============
    ImageProcessing* img_proc;
    CudaChamfer m_match;
    //=============FastRST Matching=============
    // 250701 @Tri Adding FastRST
    FastRST m_rst;


    MatchingMode m_current_mode;
    std::string m_root_path;
    std::string m_recipe_name;
    std::string m_method_data_path;
    bool m_use_cuda_for_image_process = true;

    cv::Mat m_trained_template_image;
    std::vector<cv::Point> m_last_result;
};

#endif // MATCHINGCONTAINER_H
