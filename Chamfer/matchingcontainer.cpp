#include "matchingcontainer.h"
#include <iostream>
#include <cstdlib>

void createDirectoriesRecusively(const std::string& path) {
    // Using mkdir -p is a common way to ensure a directory path exists in linux system.
    // The command is wrapped in quotes to handle potential spaces in paths.
    std::string command = "mkdir -p \"" + path + "\"";
    int result = system(command.c_str());
    if (result != 0) {
        // You might want to add more robust error handling here
        std::cerr << "Warning: Could not execute command: " << command << std::endl;
    }
}

MatchingContainer::MatchingContainer(const std::string& root_path, const std::string& recipe_name)
    : m_root_path(root_path), m_recipe_name(recipe_name), m_current_mode(MatchingMode::CHAMFER)
{
    img_proc = new ImageProcessing();
    initializePaths();
}

MatchingContainer::~MatchingContainer() {
//    if (img_proc) {
//        delete img_proc;
//        img_proc = nullptr;
//    }
}

void MatchingContainer::SetRecipe(const std::string &root_path, const std::string &recipe_name) {
    m_root_path = root_path;
    m_recipe_name = recipe_name;
    initializePaths();
}

void MatchingContainer::SetMode(MatchingMode mode) {
    m_current_mode = mode;
    initializePaths();
}

void MatchingContainer::SetUseCudaForImageProcessing(bool use_cuda) {
    m_use_cuda_for_image_process = use_cuda;
    m_match.SetImageProc(img_proc, true, m_use_cuda_for_image_process);
}

void MatchingContainer::initializePaths() {
    // Ensure root path exists
    createDirectoriesRecusively(m_root_path);

    // --- Path setup for ShapeAlignments ---
    std::string shape_param_path = m_root_path + "/Data/" + m_recipe_name + "/parameters/";
    std::string log_path = m_root_path + "/Log/";
    createDirectoriesRecusively(shape_param_path);
    createDirectoriesRecusively(log_path);

    img_proc->SetSaveDir(shape_param_path);
    img_proc->InitLog(log_path, "imgproc");

    // Set image processing class right after shape align is initialized

    // --- Path setup for Match (ChamferMatching)
    std::string method_name;
    switch (m_current_mode) {
        case MatchingMode::CHAMFER:
            method_name = "chamfer";
            break;
        case MatchingMode::FASTRST:
            method_name = "fastrst";
            break;
        case MatchingMode::SHAPE_ALIGNMENT: return;
    }

    m_method_data_path = m_root_path + "/Data/" + m_recipe_name + "/methods/" + method_name + "/";
    std::string match_log_path = log_path + method_name + "/";
    std::string match_params_path = m_root_path + "/Data/" + m_recipe_name + "/parameters/";
    createDirectoriesRecusively(m_method_data_path);
    createDirectoriesRecusively(match_log_path);

    if(m_current_mode == MatchingMode::CHAMFER) {
        //=============Chamfer Matching=============
        m_match.InitPaths(m_method_data_path, match_log_path, match_params_path);
        m_match.SetImageProc(img_proc, true, true);
        m_match.checkCUDAandSysInfo();
    }
    else if (m_current_mode == MatchingMode::FASTRST){
        //=============FastRST Matching=============
        m_rst.SetLogPath(match_log_path);
        m_rst.SetDataPath(m_method_data_path);
    }
    

}

float MatchingContainer::Train(const cv::Mat &template_img) {
    // If no image provided, try to load existing trained data
    float time_exec = 0;
    switch (m_current_mode) {
    case MatchingMode::CHAMFER:
    {
        if (template_img.empty()) {
            std::cout << "No template image provided, attemping to load pre-trained data." << std::endl;
            float time1 = m_match.LoadChamferData();
            if (time1 > 0) {
                std::string template_path = m_method_data_path + "0_orig_template_img_mat.png";
                m_trained_template_image = cv::imread(template_path);
                if (!m_trained_template_image.empty()) {
                    std::cout << "Successfully load pre-trained and template image" << std::endl;
                    time_exec += time1;
                    return time_exec;
                }
            }
            else {
                std::string template_path = m_method_data_path + "0_orig_template_img_mat.png";
                m_trained_template_image = cv::imread(template_path);
                cv::Mat template_gray;
                if (m_trained_template_image.channels() > 1) {
                    cv::cvtColor(m_trained_template_image, template_gray, cv::COLOR_BGR2GRAY);
                } else {
                    template_gray = m_trained_template_image;
                }
                time_exec = m_match.ChamferTrain(template_gray);
            }
            return time_exec;
        }
        // if an image is provided, train with it
        m_trained_template_image = template_img.clone();
        cv::Mat template_gray;
        if (m_trained_template_image.channels() > 1) {
            cv::cvtColor(m_trained_template_image, template_gray, cv::COLOR_BGR2GRAY);
        } else {
            template_gray = m_trained_template_image;
        }
        time_exec = m_match.ChamferTrain(template_gray);
        break;
    }
    case MatchingMode::FASTRST:
    {
        if (template_img.empty()) {
            // Load train data nếu có
            // Start calculating time
            auto start = std::chrono::steady_clock::now();
            bool loaded = m_rst.loadTrain("");
            auto end = std::chrono::steady_clock::now();
            auto duration = end - start;
            time_exec = std::chrono::duration<float, std::milli>(duration).count();
            if (loaded) {
                std::string template_path = m_method_data_path + "0_orig_template_img_mat.png";
                m_trained_template_image = cv::imread(template_path);
                std::cout << "Loaded FastRST train data from: " << m_method_data_path << std::endl;
                std::cout << "Time taken: " << time_exec << " ms" << std::endl;
                return time_exec;
            }
            else {
                std::string template_path = m_method_data_path + "0_orig_template_img_mat.png";
                m_trained_template_image = cv::imread(template_path);
                cv::Mat template_gray;
                if (m_trained_template_image.channels() > 1) {
                    cv::cvtColor(m_trained_template_image, template_gray, cv::COLOR_BGR2GRAY);
                } else {
                    template_gray = m_trained_template_image;
                }
                {
                    auto start = std::chrono::steady_clock::now();
                    m_rst.trainTemplate(gray);
                    m_rst.saveTrain("");
                    auto end = std::chrono::steady_clock::now();
                    auto duration = end - start;
                    time_exec = std::chrono::duration<float, std::milli>(duration).count();
                    
                }
            }
            //std::cerr << "No FastRST train data found!" << std::endl;
            return time_exec;
        }
        m_trained_template_image = template_img.clone();
        cv::Mat gray;
        if (m_trained_template_image.channels() > 1) {
            cv::cvtColor(m_trained_template_image, gray, cv::COLOR_BGR2GRAY);
        } else {
            gray = m_trained_template_image;
        }
        {
            auto start = std::chrono::steady_clock::now();
            m_rst.trainTemplate(gray);
            m_rst.saveTrain("");
            std::string template_path = m_method_data_path + "0_orig_template_img_mat.png";
            cv::imwrite(template_path, m_trained_template_image);
            auto end = std::chrono::steady_clock::now();
            auto duration = end - start;
            time_exec = std::chrono::duration<float, std::milli>(duration).count();
        }
        break;
    }
    case MatchingMode::SHAPE_ALIGNMENT:
        break;
    }
    if (time_exec > 0) {
        std::cout << "Training successfully" << std::endl;
    }
    return time_exec;
}

float MatchingContainer::Test(const cv::Mat &source_img) {
//    m_last_result.clear();
    float time_exec = 0;
    if (source_img.empty()) {
        std::cerr << "Source image is empty." << std::endl;
        return -1;
    }

    cv::Mat source_gray;
    if (source_img.channels() > 1) {
        cv::cvtColor(source_img, source_gray, cv::COLOR_BGR2GRAY);
    }else {
        source_gray = source_img;
    }

    switch(m_current_mode) {
    case MatchingMode::CHAMFER:
        time_exec = m_match.ChamferInference(source_gray, true);
        m_last_result = m_match.GetPointResult();
        break;
    case MatchingMode::FASTRST:
        if (!m_rst.IsPatternLearned()) {
            bool loaded = m_rst.loadTrain("");
            if (!loaded) {
                std::cerr << "FastRST: No trained data found. Please train first." << std::endl;
                return -1;
            }
        }
        
        auto start = std::chrono::steady_clock::now();
        cv::Mat template_img = m_rst.GetTemplate();
        m_last_result = m_rst.match(template_img, source_gray);
        auto end = std::chrono::steady_clock::now();
        auto duration = end - start;
        time_exec = std::chrono::duration<float, std::milli>(duration).count() /1000;
        
        break;
    }
    // 250701 @Tri comment and move it to CASE CHAMFER
    //m_last_result = m_match.GetPointResult();


//    if (!m_last_result.empty() && m_last_result[0]._coef > 0) {
//        return time_exec;
//    }
    return time_exec;
}

std::vector<cv::Point> MatchingContainer::GetResult() const {
    return m_last_result;
}

cv::Mat MatchingContainer::GetTrainedTemplateImg() const {
    return m_trained_template_image;
}

bool MatchingContainer::IsTrainedOK() {
    switch (m_current_mode) {
    case MatchingMode::CHAMFER:
        return m_match.IsChamferTemplateOk();
    case MatchingMode::FASTRST:
        return m_rst.IsPatternLearned();
    default:
        return false;
    }
}


